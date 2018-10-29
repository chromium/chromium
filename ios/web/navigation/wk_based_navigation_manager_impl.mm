// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_based_navigation_manager_impl.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/navigation/navigation_item_impl_list.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class CRWSessionController;

namespace {

void SetNavigationItemInWKItem(WKBackForwardListItem* wk_item,
                               std::unique_ptr<web::NavigationItemImpl> item) {
  DCHECK(wk_item);
  [[CRWNavigationItemHolder holderForBackForwardListItem:wk_item]
      setNavigationItem:std::move(item)];
}

web::NavigationItemImpl* GetNavigationItemFromWKItem(
    WKBackForwardListItem* wk_item) {
  if (!wk_item)
    return nullptr;

  return [[CRWNavigationItemHolder holderForBackForwardListItem:wk_item]
      navigationItem];
}

// Returns true if |url1| is the same as |url2| or is a placeholder of |url2|.
bool IsSameOrPlaceholderOf(const GURL& url1, const GURL& url2) {
  return url1 == url2 ||
         url1 == web::wk_navigation_util::CreatePlaceholderUrlForUrl(url2);
}

}  // namespace

namespace web {

const char kRestoreNavigationTime[] = "IOS.RestoreNavigationTime";

WKBasedNavigationManagerImpl::WKBasedNavigationManagerImpl()
    : pending_item_index_(-1),
      previous_item_index_(-1),
      last_committed_item_index_(-1),
      web_view_cache_(this) {}

WKBasedNavigationManagerImpl::~WKBasedNavigationManagerImpl() = default;

void WKBasedNavigationManagerImpl::SetSessionController(
    CRWSessionController* session_controller) {}

void WKBasedNavigationManagerImpl::InitializeSession() {}

void WKBasedNavigationManagerImpl::OnNavigationItemsPruned(
    size_t pruned_item_count) {
  delegate_->OnNavigationItemsPruned(pruned_item_count);
}

void WKBasedNavigationManagerImpl::OnNavigationItemChanged() {
  delegate_->OnNavigationItemChanged();
}

void WKBasedNavigationManagerImpl::DetachFromWebView() {
  web_view_cache_.DetachFromWebView();
}

void WKBasedNavigationManagerImpl::OnNavigationItemCommitted() {
  LoadCommittedDetails details;
  details.item = GetLastCommittedItem();
  DCHECK(details.item);

  if (!wk_navigation_util::IsRestoreSessionUrl(details.item->GetURL()) &&
      is_restore_session_in_progress_) {
    is_restore_session_in_progress_ = false;
    restored_visible_item_.reset();

    UMA_HISTOGRAM_TIMES(kRestoreNavigationTime, restoration_timer_->Elapsed());
    restoration_timer_.reset();
  }

  details.previous_item_index = GetPreviousItemIndex();
  NavigationItem* previous_item = GetItemAtIndex(details.previous_item_index);
  details.is_in_page =
      previous_item ? IsFragmentChangeNavigationBetweenUrls(
                          previous_item->GetURL(), details.item->GetURL())
                    : NO;

  delegate_->OnNavigationItemCommitted(details);
}

CRWSessionController* WKBasedNavigationManagerImpl::GetSessionController()
    const {
  return nil;
}

void WKBasedNavigationManagerImpl::AddTransientItem(const GURL& url) {
  DCHECK(web_view_cache_.IsAttachedToWebView());
  NavigationItem* last_committed_item = GetLastCommittedItem();
  transient_item_ = CreateNavigationItemWithRewriters(
      url, Referrer(), ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      NavigationInitiationType::BROWSER_INITIATED,
      last_committed_item ? last_committed_item->GetURL() : GURL::EmptyGURL(),
      nullptr /* use default rewriters only */);
  transient_item_->SetTimestamp(
      time_smoother_.GetSmoothedTime(base::Time::Now()));

  // Transient item is only supposed to be added for pending non-app-specific
  // navigations.
  // TODO(crbug.com/865727): captive portal detection seems to call this code
  // without there being a pending item. This may be an improper use of
  // navigation manager.
  NavigationItem* item = GetPendingItem();
  if (!item)
    item = GetLastCommittedNonAppSpecificItem();
  // Item may still be null in captive portal case if chrome://newtab is the
  // only entry in back/forward history.
  if (item) {
    DCHECK(item->GetUserAgentType() != UserAgentType::NONE);
    transient_item_->SetUserAgentType(item->GetUserAgentType());
  }
}

void WKBasedNavigationManagerImpl::AddPendingItem(
    const GURL& url,
    const web::Referrer& referrer,
    ui::PageTransition navigation_type,
    NavigationInitiationType initiation_type,
    UserAgentOverrideOption user_agent_override_option) {
  DiscardNonCommittedItems();

  pending_item_index_ = -1;
  NavigationItem* last_committed_item = GetLastCommittedItem();
  pending_item_ = CreateNavigationItemWithRewriters(
      url, referrer, navigation_type, initiation_type,
      last_committed_item ? last_committed_item->GetURL() : GURL::EmptyGURL(),
      &transient_url_rewriters_);
  RemoveTransientURLRewriters();
  UpdatePendingItemUserAgentType(user_agent_override_option,
                                 GetLastCommittedNonAppSpecificItem(),
                                 pending_item_.get());

  // No need to detect renderer-initiated back/forward navigation in detached
  // mode because there is no renderer.
  if (!web_view_cache_.IsAttachedToWebView())
    return;

  // AddPendingItem is called no later than |didCommitNavigation|. The only time
  // when all three of WKWebView's URL, the pending URL and WKBackForwardList's
  // current item URL are identical before |didCommitNavigation| is when the
  // in-progress navigation is a back-forward navigation. In this case, current
  // item has already been updated to point to the new location in back-forward
  // history, so pending item index should be set to the current item index.
  // Similarly, current item should be reused when reloading a placeholder URL.
  //
  // WebErrorPages and URL rewriting in ErrorRetryStateMachine make it possible
  // for the web view URL to be the target URL even though current item is the
  // placeholder. This is taken into consideration when checking equivalence
  // between the URLs.
  id<CRWWebViewNavigationProxy> proxy = delegate_->GetWebViewNavigationProxy();
  WKBackForwardListItem* current_wk_item = proxy.backForwardList.currentItem;
  const GURL current_item_url = net::GURLWithNSURL(current_wk_item.URL);
  if (proxy.backForwardList.currentItem &&
      IsSameOrPlaceholderOf(current_item_url, pending_item_->GetURL()) &&
      IsSameOrPlaceholderOf(current_item_url, net::GURLWithNSURL(proxy.URL))) {
    pending_item_index_ = web_view_cache_.GetCurrentItemIndex();

    // If |currentItem| is not already associated with a NavigationItemImpl,
    // associate the newly created item with it. Otherwise, discard the new item
    // since it will be a duplicate.
    NavigationItemImpl* current_item =
        GetNavigationItemFromWKItem(current_wk_item);
    if (!current_item) {
      SetNavigationItemInWKItem(current_wk_item, std::move(pending_item_));
    }
    pending_item_.reset();
  }
}

void WKBasedNavigationManagerImpl::CommitPendingItem() {
  DCHECK(web_view_cache_.IsAttachedToWebView());

  // CommitPendingItem may be called multiple times. Do nothing if there is no
  // pending item.
  if (pending_item_index_ == -1 && !pending_item_)
    return;

  bool last_committed_item_was_empty_window_open_item =
      empty_window_open_item_ != nullptr;

  if (pending_item_index_ == -1) {
    pending_item_->ResetForCommit();
    pending_item_->SetTimestamp(
        time_smoother_.GetSmoothedTime(base::Time::Now()));

    id<CRWWebViewNavigationProxy> proxy =
        delegate_->GetWebViewNavigationProxy();

    // If WKBackForwardList exists but |currentItem| is nil at this point, it is
    // because the current navigation is an empty window open navigation.
    // If |currentItem| is not nil, it is the last committed item in the
    // WKWebView.
    if (proxy.backForwardList && !proxy.backForwardList.currentItem) {
      // WKWebView's URL should be about:blank for empty window open item.
      // TODO(crbug.com/885249): Use GURL::IsAboutBlank() instead.
      DCHECK(base::StartsWith(net::GURLWithNSURL(proxy.URL).spec(),
                              url::kAboutBlankURL,
                              base::CompareCase::SENSITIVE));
      // There should be no back-forward history for empty window open item.
      DCHECK_EQ(0UL, proxy.backForwardList.backList.count);
      DCHECK_EQ(0UL, proxy.backForwardList.forwardList.count);

      empty_window_open_item_ = std::move(pending_item_);
    } else {
      empty_window_open_item_.reset();
      SetNavigationItemInWKItem(proxy.backForwardList.currentItem,
                                std::move(pending_item_));
    }
  }

  pending_item_index_ = -1;
  pending_item_.reset();
  // If the last committed item is the empty window open item, then don't update
  // previous item because the new commit replaces the last committed item.
  if (!last_committed_item_was_empty_window_open_item) {
    previous_item_index_ = last_committed_item_index_;
  }
  // If the newly committed item is the empty window open item, fake an index of
  // 0 because WKBackForwardList is empty at this point.
  last_committed_item_index_ =
      empty_window_open_item_ ? 0 : web_view_cache_.GetCurrentItemIndex();
  OnNavigationItemCommitted();
}

int WKBasedNavigationManagerImpl::GetIndexForOffset(int offset) const {
  int current_item_index = pending_item_index_;
  if (pending_item_index_ == -1) {
    current_item_index =
        empty_window_open_item_ ? 0 : web_view_cache_.GetCurrentItemIndex();
  }

  if (offset < 0 && GetTransientItem() && pending_item_index_ == -1) {
    // Going back from transient item that added to the end navigation stack
    // is a matter of discarding it as there is no need to move navigation
    // index back.
    offset++;
  }
  return current_item_index + offset;
}

int WKBasedNavigationManagerImpl::GetPreviousItemIndex() const {
  return previous_item_index_;
}

void WKBasedNavigationManagerImpl::SetPreviousItemIndex(
    int previous_item_index) {
  DCHECK(web_view_cache_.IsAttachedToWebView());
  previous_item_index_ = previous_item_index;
}

void WKBasedNavigationManagerImpl::AddPushStateItemIfNecessary(
    const GURL& url,
    NSString* state_object,
    ui::PageTransition transition) {
  // WKBasedNavigationManager doesn't directly manage session history. Nothing
  // to do here.
}

BrowserState* WKBasedNavigationManagerImpl::GetBrowserState() const {
  return browser_state_;
}

WebState* WKBasedNavigationManagerImpl::GetWebState() const {
  return delegate_->GetWebState();
}

NavigationItem* WKBasedNavigationManagerImpl::GetVisibleItem() const {
  if (is_restore_session_in_progress_)
    return restored_visible_item_.get();

  NavigationItem* transient_item = GetTransientItem();
  if (transient_item) {
    return transient_item;
  }

  // Only return pending_item_ for new (non-history), user-initiated
  // navigations in order to prevent URL spoof attacks.
  NavigationItemImpl* pending_item = GetPendingItemImpl();
  if (pending_item) {
    bool is_user_initiated = pending_item->NavigationInitiationType() ==
                             NavigationInitiationType::BROWSER_INITIATED;
    bool safe_to_show_pending = is_user_initiated && pending_item_index_ == -1;
    if (safe_to_show_pending) {
      return pending_item;
    }
  }
  return GetLastCommittedItem();
}

void WKBasedNavigationManagerImpl::DiscardNonCommittedItems() {
  pending_item_.reset();
  transient_item_.reset();
  pending_item_index_ = -1;
}

int WKBasedNavigationManagerImpl::GetItemCount() const {
  if (empty_window_open_item_) {
    return 1;
  }

  return web_view_cache_.GetBackForwardListItemCount();
}

NavigationItem* WKBasedNavigationManagerImpl::GetItemAtIndex(
    size_t index) const {
  return GetNavigationItemImplAtIndex(index);
}

int WKBasedNavigationManagerImpl::GetIndexOfItem(
    const NavigationItem* item) const {
  if (item == empty_window_open_item_.get())
    return 0;

  for (size_t index = 0; index < web_view_cache_.GetBackForwardListItemCount();
       index++) {
    if (web_view_cache_.GetNavigationItemImplAtIndex(
            index, false /* create_if_missing */) == item)
      return index;
  }
  return -1;
}

int WKBasedNavigationManagerImpl::GetPendingItemIndex() const {
  if (GetPendingItem()) {
    if (pending_item_index_ != -1) {
      return pending_item_index_;
    }
    // TODO(crbug.com/665189): understand why last committed item index is
    // returned here.
    return GetLastCommittedItemIndex();
  }
  return -1;
}

int WKBasedNavigationManagerImpl::GetLastCommittedItemIndex() const {
  // WKBackForwardList's |currentItem| is usually the last committed item,
  // except two cases:
  // 1) when the pending navigation is a back-forward navigation, in which
  //    case it is actually the pending item. As a workaround, fall back to
  //    last_committed_item_index_. This is not 100% correct (since
  //    last_committed_item_index_ is only updated for main frame navigations),
  //    but is the best possible answer.
  // 2) when the last committed item is an empty window open item.
  if (pending_item_index_ >= 0 || empty_window_open_item_) {
    return last_committed_item_index_;
  }
  return web_view_cache_.GetCurrentItemIndex();
}

bool WKBasedNavigationManagerImpl::RemoveItemAtIndex(int index) {
  DLOG(WARNING) << "Not yet implemented.";
  return true;
}

bool WKBasedNavigationManagerImpl::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool WKBasedNavigationManagerImpl::CanGoForward() const {
  return CanGoToOffset(1);
}

bool WKBasedNavigationManagerImpl::CanGoToOffset(int offset) const {
  if (is_restore_session_in_progress_)
    return false;

  // If the last committed item is the empty window.open item, no back-forward
  // navigation is allowed.
  if (empty_window_open_item_) {
    return offset == 0;
  }
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetItemCount();
}

void WKBasedNavigationManagerImpl::GoBack() {
  GoToIndex(GetIndexForOffset(-1));
}

void WKBasedNavigationManagerImpl::GoForward() {
  GoToIndex(GetIndexForOffset(1));
}

NavigationItemList WKBasedNavigationManagerImpl::GetBackwardItems() const {
  NavigationItemList items;

  // If the current navigation item is a transient item (e.g. SSL
  // interstitial), the last committed item should also be considered part of
  // the backward history.
  int current_back_forward_item_index = web_view_cache_.GetCurrentItemIndex();
  if (GetTransientItem() && current_back_forward_item_index >= 0) {
    items.push_back(GetItemAtIndex(current_back_forward_item_index));
  }

  for (int index = current_back_forward_item_index - 1; index >= 0; index--) {
    items.push_back(GetItemAtIndex(index));
  }

  return items;
}

NavigationItemList WKBasedNavigationManagerImpl::GetForwardItems() const {
  NavigationItemList items;
  for (int index = web_view_cache_.GetCurrentItemIndex() + 1;
       index < GetItemCount(); index++) {
    items.push_back(GetItemAtIndex(index));
  }
  return items;
}

void WKBasedNavigationManagerImpl::CopyStateFromAndPrune(
    const NavigationManager* source) {
  DLOG(WARNING) << "Not yet implemented.";
}

bool WKBasedNavigationManagerImpl::CanPruneAllButLastCommittedItem() const {
  DLOG(WARNING) << "Not yet implemented.";
  return true;
}

void WKBasedNavigationManagerImpl::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
  DCHECK(!is_restore_session_in_progress_);
  WillRestore(items.size());

  DCHECK_LT(last_committed_item_index, static_cast<int>(items.size()));
  DCHECK(items.empty() || last_committed_item_index >= 0);
  if (items.empty())
    return;

  if (!web_view_cache_.IsAttachedToWebView())
    web_view_cache_.ResetToAttached();

  DiscardNonCommittedItems();
  if (GetItemCount() > 0) {
    delegate_->RemoveWebView();
  }
  DCHECK_EQ(0, GetItemCount());
  pending_item_index_ = -1;
  previous_item_index_ = -1;
  last_committed_item_index_ = -1;

  // This function restores session history by loading a magic local file
  // (restore_session.html) into the web view. The session history is encoded
  // in the query parameter. When loaded, restore_session.html parses the
  // session history and replays them into the web view using History API.

  // TODO(crbug.com/771200): Retain these original NavigationItems restored from
  // storage and associate them with new WKBackForwardListItems created after
  // history restore so information such as scroll position is restored.
  GURL url = wk_navigation_util::CreateRestoreSessionUrl(
      last_committed_item_index, items);

  WebLoadParams params(url);
  // It's not clear how this transition type will be used and what's the impact.
  // For now, use RELOAD because restoring history is kind of like a reload of
  // the current page.
  params.transition_type = ui::PAGE_TRANSITION_RELOAD;

  // This pending item will become the first item in the restored history.
  params.virtual_url = items[0]->GetVirtualURL();

  // Ordering is important. Cache the visible item of the restored session
  // before starting the new navigation, which may trigger client lookup of
  // visible item. The visible item of the restored session is the last
  // committed item, because a restored session has no pending or transient
  // item.
  is_restore_session_in_progress_ = true;
  restoration_timer_ = std::make_unique<base::ElapsedTimer>();
  if (last_committed_item_index > -1)
    restored_visible_item_ = std::move(items[last_committed_item_index]);

  LoadURLWithParams(params);
}

void WKBasedNavigationManagerImpl::LoadIfNecessary() {
  if (!web_view_cache_.IsAttachedToWebView()) {
    // Loading from detached mode is equivalent to restoring cached history.
    Restore(web_view_cache_.GetCurrentItemIndex(),
            web_view_cache_.ReleaseCachedItems());
    DCHECK(web_view_cache_.IsAttachedToWebView());
  } else {
    delegate_->LoadIfNecessary();
  }
}

NavigationItemImpl* WKBasedNavigationManagerImpl::GetNavigationItemImplAtIndex(
    size_t index) const {
  if (empty_window_open_item_) {
    // Return nullptr for index != 0 instead of letting the code fall through
    // (which in most cases will return null anyways because wk_item should be
    // nil) for the slim chance that WKBackForwardList has been updated for a
    // new navigation but WKWebView has not triggered the |didCommitNavigation:|
    // callback. NavigationItem for the new wk_item should not be returned until
    // after DidCommitPendingItem() is called.
    return index == 0 ? empty_window_open_item_.get() : nullptr;
  }

  return web_view_cache_.GetNavigationItemImplAtIndex(
      index, true /* create_if_missing */);
}

NavigationItemImpl* WKBasedNavigationManagerImpl::GetLastCommittedItemImpl()
    const {
  if (empty_window_open_item_) {
    return empty_window_open_item_.get();
  }

  int index = GetLastCommittedItemIndex();
  return index == -1 ? nullptr
                     : GetNavigationItemImplAtIndex(static_cast<size_t>(index));
}

NavigationItemImpl* WKBasedNavigationManagerImpl::GetPendingItemImpl() const {
  return (pending_item_index_ == -1)
             ? pending_item_.get()
             : GetNavigationItemImplAtIndex(pending_item_index_);
}

NavigationItemImpl* WKBasedNavigationManagerImpl::GetTransientItemImpl() const {
  return transient_item_.get();
}

void WKBasedNavigationManagerImpl::FinishGoToIndex(
    int index,
    NavigationInitiationType type,
    bool has_user_gesture) {
  if (!web_view_cache_.IsAttachedToWebView()) {
    // GoToIndex from detached mode is equivalent to restoring history with
    // |last_committed_item_index| updated to |index|.
    Restore(index, web_view_cache_.ReleaseCachedItems());
    DCHECK(web_view_cache_.IsAttachedToWebView());
    return;
  }

  DiscardNonCommittedItems();
  NavigationItem* item = GetItemAtIndex(index);
  item->SetTransitionType(ui::PageTransitionFromInt(
      item->GetTransitionType() | ui::PAGE_TRANSITION_FORWARD_BACK));
  WKBackForwardListItem* wk_item = web_view_cache_.GetWKItemAtIndex(index);
  if (wk_item) {
    [delegate_->GetWebViewNavigationProxy() goToBackForwardListItem:wk_item];
  } else {
    DCHECK(index == 0 && empty_window_open_item_)
        << " wk_item should not be nullptr. index: " << index
        << " has_empty_window_open_item: "
        << (empty_window_open_item_ != nullptr);
  }
}

void WKBasedNavigationManagerImpl::FinishReload() {
  if (!web_view_cache_.IsAttachedToWebView()) {
    // Reload from detached mode is equivalent to restoring history unchanged.
    Restore(web_view_cache_.GetCurrentItemIndex(),
            web_view_cache_.ReleaseCachedItems());
    DCHECK(web_view_cache_.IsAttachedToWebView());
    return;
  }

  delegate_->Reload();
}

void WKBasedNavigationManagerImpl::FinishLoadURLWithParams() {
  if (!web_view_cache_.IsAttachedToWebView()) {
    DCHECK_EQ(pending_item_index_, -1);
    if (pending_item_ && web_view_cache_.GetBackForwardListItemCount() > 0) {
      // Loading a pending item from detached state is equivalent to replacing
      // all forward history after the cached current item with the new pending
      // item.
      std::vector<std::unique_ptr<NavigationItem>> cached_items =
          web_view_cache_.ReleaseCachedItems();
      int next_item_index = web_view_cache_.GetCurrentItemIndex() + 1;
      DCHECK_GT(next_item_index, 0);
      cached_items.resize(next_item_index + 1);
      cached_items[next_item_index].reset(pending_item_.release());
      Restore(next_item_index, std::move(cached_items));
      DCHECK(web_view_cache_.IsAttachedToWebView());
      return;
    }
    web_view_cache_.ResetToAttached();
  }

  delegate_->LoadCurrentItem();
}

bool WKBasedNavigationManagerImpl::IsPlaceholderUrl(const GURL& url) const {
  return wk_navigation_util::IsPlaceholderUrl(url);
}

WKBasedNavigationManagerImpl::WKWebViewCache::WKWebViewCache(
    WKBasedNavigationManagerImpl* navigation_manager)
    : navigation_manager_(navigation_manager), attached_to_web_view_(true) {}

WKBasedNavigationManagerImpl::WKWebViewCache::~WKWebViewCache() = default;

bool WKBasedNavigationManagerImpl::WKWebViewCache::IsAttachedToWebView() const {
  return attached_to_web_view_;
}

void WKBasedNavigationManagerImpl::WKWebViewCache::DetachFromWebView() {
  if (IsAttachedToWebView()) {
    cached_current_item_index_ = GetCurrentItemIndex();
    cached_items_.resize(GetBackForwardListItemCount());
    for (size_t index = 0; index < GetBackForwardListItemCount(); index++) {
      cached_items_[index].reset(new NavigationItemImpl(
          *GetNavigationItemImplAtIndex(index, true /* create_if_missing */)));
    }
  }
  attached_to_web_view_ = false;
}

void WKBasedNavigationManagerImpl::WKWebViewCache::ResetToAttached() {
  cached_items_.clear();
  cached_current_item_index_ = -1;
  attached_to_web_view_ = true;
}

std::vector<std::unique_ptr<NavigationItem>>
WKBasedNavigationManagerImpl::WKWebViewCache::ReleaseCachedItems() {
  DCHECK(!IsAttachedToWebView());
  std::vector<std::unique_ptr<NavigationItem>> result(cached_items_.size());
  for (size_t index = 0; index < cached_items_.size(); index++) {
    result[index].reset(cached_items_[index].release());
  }
  cached_items_.clear();
  return result;
}

size_t
WKBasedNavigationManagerImpl::WKWebViewCache::GetBackForwardListItemCount()
    const {
  if (!IsAttachedToWebView())
    return cached_items_.size();

  id<CRWWebViewNavigationProxy> proxy =
      navigation_manager_->delegate_->GetWebViewNavigationProxy();
  if (proxy) {
    size_t count_current_page = proxy.backForwardList.currentItem ? 1 : 0;
    return proxy.backForwardList.backList.count + count_current_page +
           proxy.backForwardList.forwardList.count;
  }

  // If WebView has not been created, it's fair to say navigation has 0 item.
  return 0;
}

int WKBasedNavigationManagerImpl::WKWebViewCache::GetCurrentItemIndex() const {
  if (!IsAttachedToWebView())
    return cached_current_item_index_;

  id<CRWWebViewNavigationProxy> proxy =
      navigation_manager_->delegate_->GetWebViewNavigationProxy();
  if (proxy.backForwardList.currentItem) {
    return static_cast<int>(proxy.backForwardList.backList.count);
  }
  return -1;
}

NavigationItemImpl*
WKBasedNavigationManagerImpl::WKWebViewCache::GetNavigationItemImplAtIndex(
    size_t index,
    bool create_if_missing) const {
  if (index >= GetBackForwardListItemCount())
    return nullptr;

  if (!IsAttachedToWebView())
    return cached_items_[index].get();

  WKBackForwardListItem* wk_item = GetWKItemAtIndex(index);
  NavigationItemImpl* item = GetNavigationItemFromWKItem(wk_item);

  if (!wk_item || item || !create_if_missing) {
    return item;
  }

  // TODO(crbug.com/734150): Add a stat counter to track rebuilding frequency.
  WKBackForwardListItem* prev_wk_item =
      index == 0 ? nil : GetWKItemAtIndex(index - 1);
  std::unique_ptr<web::NavigationItemImpl> new_item =
      navigation_manager_->CreateNavigationItemWithRewriters(
          net::GURLWithNSURL(wk_item.URL),
          (prev_wk_item ? web::Referrer(net::GURLWithNSURL(prev_wk_item.URL),
                                        web::ReferrerPolicyAlways)
                        : web::Referrer()),
          ui::PageTransition::PAGE_TRANSITION_LINK,
          NavigationInitiationType::RENDERER_INITIATED,
          // Not using GetLastCommittedItem()->GetURL() in case the last
          // committed item in the WKWebView hasn't been linked to a
          // NavigationItem and this method is called in that code path to avoid
          // an infinite cycle.
          net::GURLWithNSURL(prev_wk_item.URL),
          nullptr /* use default rewriters only */);
  new_item->SetTimestamp(
      navigation_manager_->time_smoother_.GetSmoothedTime(base::Time::Now()));
  const GURL& url = new_item->GetURL();
  // If this navigation item has a restore_session.html URL, then it was created
  // to restore session history and will redirect to the target URL encoded in
  // the query parameter automatically. Set virtual URL to the target URL so the
  // internal restore_session.html is not exposed in the UI and to URL-sensing
  // components outside of //ios/web layer.
  if (wk_navigation_util::IsRestoreSessionUrl(url)) {
    GURL virtual_url;
    if (wk_navigation_util::ExtractTargetURL(url, &virtual_url)) {
      if (wk_navigation_util::IsPlaceholderUrl(virtual_url)) {
        new_item->SetVirtualURL(
            wk_navigation_util::ExtractUrlFromPlaceholderUrl(virtual_url));
      } else {
        new_item->SetVirtualURL(virtual_url);
      }
    }
  }

  SetNavigationItemInWKItem(wk_item, std::move(new_item));
  return GetNavigationItemFromWKItem(wk_item);
}

WKBackForwardListItem*
WKBasedNavigationManagerImpl::WKWebViewCache::GetWKItemAtIndex(
    size_t index) const {
  DCHECK(IsAttachedToWebView());
  if (index >= GetBackForwardListItemCount()) {
    return nil;
  }

  // Convert the index to an offset relative to backForwardList.currentItem (
  // which is also the last committed item), then use WKBackForwardList API to
  // retrieve the item.
  int offset = static_cast<int>(index) - GetCurrentItemIndex();
  id<CRWWebViewNavigationProxy> proxy =
      navigation_manager_->delegate_->GetWebViewNavigationProxy();
  return [proxy.backForwardList itemAtIndex:offset];
}

}  // namespace web
