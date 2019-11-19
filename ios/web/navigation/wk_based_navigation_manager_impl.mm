// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_based_navigation_manager_impl.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/navigation/navigation_item_impl_list.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
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

void WKBasedNavigationManagerImpl::DetachFromWebView() {
  web_view_cache_.DetachFromWebView();
  is_restore_session_in_progress_ = false;
}

void WKBasedNavigationManagerImpl::OnNavigationItemCommitted() {
  NavigationItem* item = GetLastCommittedItemInCurrentOrRestoredSession();
  DCHECK(item);
  delegate_->OnNavigationItemCommitted(item);

  if (!wk_navigation_util::IsRestoreSessionUrl(item->GetURL())) {
    restored_visible_item_.reset();
  }
}

void WKBasedNavigationManagerImpl::OnNavigationStarted(const GURL& url) {
  if (!is_restore_session_in_progress_)
    return;

  GURL target_url;
  if (wk_navigation_util::IsRestoreSessionUrl(url) &&
      !web::wk_navigation_util::ExtractTargetURL(url, &target_url)) {
    restoration_timer_ = std::make_unique<base::ElapsedTimer>();
  } else if (!wk_navigation_util::IsRestoreSessionUrl(url)) {
    // It's possible for there to be pending navigations for a session that is
    // going to be restored (such as for the -ForwardHistoryClobber workaround).
    // In this case, the pending navigation will start while the navigation
    // manager is in restore mode.  There are other edges cases where a restore
    // session finishes without trigger it's start, such as when restoring some
    // with some app specific or blocked URLs, or when WKWebView's
    // backForwardList state is out of sync. See crbug.com/1008026 for more
    // details.
    if (restoration_timer_) {
      UMA_HISTOGRAM_TIMES(kRestoreNavigationTime,
                          restoration_timer_->Elapsed());
      restoration_timer_.reset();
    }
    FinalizeSessionRestore();
  }
}

void WKBasedNavigationManagerImpl::FinalizeSessionRestore() {
  is_restore_session_in_progress_ = false;
  for (base::OnceClosure& callback : restore_session_completion_callbacks_) {
    std::move(callback).Run();
  }
  restore_session_completion_callbacks_.clear();
  LoadIfNecessary();
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
    item = GetLastCommittedItemWithUserAgentType();
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
  NavigationItem* last_committed_item =
      GetLastCommittedItemInCurrentOrRestoredSession();
  pending_item_ = CreateNavigationItemWithRewriters(
      url, referrer, navigation_type, initiation_type,
      last_committed_item ? last_committed_item->GetURL() : GURL::EmptyGURL(),
      &transient_url_rewriters_);
  RemoveTransientURLRewriters();
  UpdatePendingItemUserAgentType(user_agent_override_option,
                                 GetLastCommittedItemWithUserAgentType(),
                                 pending_item_.get());

  if (!next_pending_url_should_skip_serialization_.is_empty() &&
      url == next_pending_url_should_skip_serialization_) {
    pending_item_->SetShouldSkipSerialization(true);
  }
  next_pending_url_should_skip_serialization_ = GURL::EmptyGURL();

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
  GURL current_item_url = net::GURLWithNSURL(current_wk_item.URL);

  // When reloading an target url redirect page, re-use the target url as the
  // current item url.
  GURL target_url;
  if (navigation_type & ui::PAGE_TRANSITION_RELOAD &&
      !(navigation_type & ui::PAGE_TRANSITION_FORWARD_BACK) &&
      initiation_type == NavigationInitiationType::BROWSER_INITIATED &&
      web::wk_navigation_util::IsRestoreSessionUrl(current_item_url) &&
      web::wk_navigation_util::ExtractTargetURL(current_item_url,
                                                &target_url)) {
    current_item_url = target_url;
  }

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
  if (pending_item_index_ == -1 && !pending_item_) {
    // Per crbug.com/1010765, it is sometimes possible for pending items to
    // never commit. If a previous pending item was copied into
    // empty_window_open_item_, clear it here.
    empty_window_open_item_.reset();
    return;
  }

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

void WKBasedNavigationManagerImpl::CommitPendingItem(
    std::unique_ptr<NavigationItemImpl> item) {
  if (!item) {
    CommitPendingItem();
    return;
  }

  DCHECK(web_view_cache_.IsAttachedToWebView());

  // CommitPendingItem may be called multiple times. Do nothing if there is no
  // pending item.
  if (!item)
    return;

  bool last_committed_item_was_empty_window_open_item =
      empty_window_open_item_ != nullptr;

  item->ResetForCommit();
  item->SetTimestamp(time_smoother_.GetSmoothedTime(base::Time::Now()));

  id<CRWWebViewNavigationProxy> proxy = delegate_->GetWebViewNavigationProxy();

  // If WKBackForwardList exists but |currentItem| is nil at this point, it is
  // because the current navigation is an empty window open navigation.
  // If |currentItem| is not nil, it is the last committed item in the
  // WKWebView.
  if (proxy.backForwardList && !proxy.backForwardList.currentItem) {
    if (!base::ios::IsRunningOnIOS13OrLater()) {
      // Prior to iOS 13 WKWebView's URL should be about:blank for empty window
      // open item. TODO(crbug.com/885249): Use GURL::IsAboutBlank() instead.
      DCHECK(base::StartsWith(net::GURLWithNSURL(proxy.URL).spec(),
                              url::kAboutBlankURL,
                              base::CompareCase::SENSITIVE));
    }
    // There should be no back-forward history for empty window open item.
    DCHECK_EQ(0UL, proxy.backForwardList.backList.count);
    DCHECK_EQ(0UL, proxy.backForwardList.forwardList.count);

    empty_window_open_item_ = std::move(item);
  } else {
    empty_window_open_item_.reset();

    const GURL item_url(item->GetURL());
    WKBackForwardList* back_forward_list = proxy.backForwardList;
    if (item_url == net::GURLWithNSURL(back_forward_list.currentItem.URL)) {
      SetNavigationItemInWKItem(back_forward_list.currentItem, std::move(item));
    } else {
      // Sometimes |currentItem.URL| is not updated correctly while the webView
      // URL is correctly updated. This is a bug in WKWebView. Check to see if
      // the next or previous item matches, and update that item instead. If
      // nothing matches, still update the the currentItem.
      if (back_forward_list.backItem &&
          item_url == net::GURLWithNSURL(back_forward_list.backItem.URL)) {
        SetNavigationItemInWKItem(back_forward_list.backItem, std::move(item));
      } else if (back_forward_list.forwardItem &&
                 item_url ==
                     net::GURLWithNSURL(back_forward_list.forwardItem.URL)) {
        SetNavigationItemInWKItem(back_forward_list.forwardItem,
                                  std::move(item));
      } else {
        // Otherwise default here. This can happen when restoring an NTP, since
        // |back_forward_list.currentItem.URL| doesn't get updated when going
        // from a file:// scheme to about:// scheme.
        SetNavigationItemInWKItem(back_forward_list.currentItem,
                                  std::move(item));
      }
    }
  }

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

std::unique_ptr<web::NavigationItemImpl>
WKBasedNavigationManagerImpl::ReleasePendingItem() {
  return std::move(pending_item_);
}

void WKBasedNavigationManagerImpl::SetPendingItem(
    std::unique_ptr<web::NavigationItemImpl> item) {
  pending_item_ = std::move(item);
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

bool WKBasedNavigationManagerImpl::IsRestoreSessionInProgress() const {
  return is_restore_session_in_progress_;
}

bool WKBasedNavigationManagerImpl::ShouldBlockUrlDuringRestore(
    const GURL& url) {
  DCHECK(is_restore_session_in_progress_);
  if (!web::GetWebClient()->ShouldBlockUrlDuringRestore(url, GetWebState()))
    return false;

  // Abort restore.
  DiscardNonCommittedItems();
  last_committed_item_index_ = web_view_cache_.GetCurrentItemIndex();
  restored_visible_item_.reset();
  FinalizeSessionRestore();
  return true;
}

void WKBasedNavigationManagerImpl::SetPendingItemIndex(int index) {
  pending_item_index_ = index;
}

BrowserState* WKBasedNavigationManagerImpl::GetBrowserState() const {
  return browser_state_;
}

WebState* WKBasedNavigationManagerImpl::GetWebState() const {
  return delegate_->GetWebState();
}

bool WKBasedNavigationManagerImpl::CanTrustLastCommittedItem(
    const NavigationItem* last_committed_item) const {
  DCHECK(last_committed_item);
  if (!web_view_cache_.IsAttachedToWebView())
    return true;

  // Only compare origins, as any mismatch between |web_view_url| and
  // |last_committed_url| with the same origin are safe to return as
  // visible.
  GURL web_view_url = web_view_cache_.GetVisibleWebViewURL();
  GURL last_committed_url = last_committed_item->GetURL();
  if (web_view_url.GetOrigin() == last_committed_url.GetOrigin())
    return true;

  // Fast back-forward navigations can be performed synchronously, with the
  // WKWebView.URL updated before enough callbacks occur to update the
  // last committed item.  As a result, any calls to
  // -CanTrustLastCommittedItem during a call to WKWebView
  // -goToBackForwardListItem are wrapped in the
  // |going_to_back_forward_list_item_| flag. This flag is set and immediately
  // unset because the the mismatch between URL and last_committed_item is
  // expected.
  if (going_to_back_forward_list_item_)
    return true;

  // WKWebView.URL will update immediately when navigating to and from
  // about, file or chrome scheme URLs.
  if (web_view_url.SchemeIs(url::kAboutScheme) ||
      last_committed_url.SchemeIs(url::kAboutScheme) ||
      web_view_url.SchemeIs(url::kFileScheme) ||
      last_committed_url.SchemeIs(url::kAboutScheme) ||
      web::GetWebClient()->IsAppSpecificURL(web_view_url) ||
      web::GetWebClient()->IsAppSpecificURL(last_committed_url)) {
    return true;
  }

  return false;
}

NavigationItem* WKBasedNavigationManagerImpl::GetVisibleItem() const {
  if (is_restore_session_in_progress_ || restored_visible_item_)
    return restored_visible_item_.get();

  NavigationItem* transient_item = GetTransientItem();
  if (transient_item) {
    return transient_item;
  }

  // Only return pending_item_ for new (non-history), user-initiated
  // navigations in order to prevent URL spoof attacks.
  NavigationItemImpl* pending_item = GetPendingItemInCurrentOrRestoredSession();
  if (pending_item) {
    bool is_user_initiated = pending_item->NavigationInitiationType() ==
                             NavigationInitiationType::BROWSER_INITIATED;
    bool safe_to_show_pending = is_user_initiated && pending_item_index_ == -1;
    if (web::features::UseWKWebViewLoading()) {
      safe_to_show_pending = safe_to_show_pending && GetWebState()->IsLoading();
    }
    if (safe_to_show_pending) {
      return pending_item;
    }
  }

  NavigationItem* last_committed_item = GetLastCommittedItem();
  if (last_committed_item) {
    return last_committed_item;
  }

  // While an -IsRestoreSessionUrl URL can not be a committed page, it is
  // OK to display it as a visible URL.  This prevents seeing about:blank while
  // navigating to a restore URL.
  NavigationItem* result = GetLastCommittedItemInCurrentOrRestoredSession();
  if (result && wk_navigation_util::IsRestoreSessionUrl(result->GetURL())) {
    return result;
  }
  return nullptr;
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
  if (is_restore_session_in_progress_)
    return -1;
  return pending_item_index_;
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

  if (is_restore_session_in_progress_)
    return items;

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

  if (is_restore_session_in_progress_)
    return items;

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

void WKBasedNavigationManagerImpl::
    ApplyWKWebViewForwardHistoryClobberWorkaround() {
  DCHECK(web_view_cache_.IsAttachedToWebView());

  int current_item_index = web_view_cache_.GetCurrentItemIndex();
  DCHECK_GE(current_item_index, 0);

  int item_count = GetItemCount();
  DCHECK_LT(current_item_index, item_count);

  std::vector<std::unique_ptr<NavigationItem>> forward_items(
      item_count - current_item_index);

  for (size_t i = 0; i < forward_items.size(); i++) {
    const NavigationItemImpl* item =
        GetNavigationItemImplAtIndex(i + current_item_index);
    forward_items[i] = std::make_unique<web::NavigationItemImpl>(*item);
  }

  DiscardNonCommittedItems();

  // Replace forward history in WKWebView with |forward_items|.
  // |last_committed_item_index| is set to 0 so that when this partial session
  // restoration finishes, the current item is the first item in
  // |forward_itmes|, which is also the current item before the session
  // restoration, but because of crbug.com/887497 is expected to be clobbered
  // with the wrong web content. The partial restore effectively forces a fresh
  // load of this item while maintaining forward history.
  UnsafeRestore(/*last_committed_item_index_=*/0, std::move(forward_items));
}

void WKBasedNavigationManagerImpl::SetWKWebViewNextPendingUrlNotSerializable(
    const GURL& url) {
  next_pending_url_should_skip_serialization_ = url;
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
  DCHECK_EQ(-1, pending_item_index_);
  previous_item_index_ = -1;
  last_committed_item_index_ = -1;

  UnsafeRestore(last_committed_item_index, std::move(items));
}

void WKBasedNavigationManagerImpl::UnsafeRestore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
  // This function restores session history by loading a magic local file
  // (restore_session.html) into the web view. The session history is encoded
  // in the query parameter. When loaded, restore_session.html parses the
  // session history and replays them into the web view using History API.
  for (size_t index = 0; index < items.size(); ++index) {
    RewriteItemURLIfNecessary(items[index].get());
  }

  // TODO(crbug.com/771200): Retain these original NavigationItems restored from
  // storage and associate them with new WKBackForwardListItems created after
  // history restore so information such as scroll position is restored.
  int first_index = -1;
  GURL url;
  wk_navigation_util::CreateRestoreSessionUrl(last_committed_item_index, items,
                                              &url, &first_index);
  DCHECK_GE(first_index, 0);
  DCHECK_LT(base::checked_cast<NSUInteger>(first_index), items.size());
  DCHECK(url.is_valid());

  WebLoadParams params(url);
  // It's not clear how this transition type will be used and what's the impact.
  // For now, use RELOAD because restoring history is kind of like a reload of
  // the current page.
  params.transition_type = ui::PAGE_TRANSITION_RELOAD;

  // This pending item will become the first item in the restored history.
  params.virtual_url = items[first_index]->GetVirtualURL();

  // Grab the title of the first item before |restored_visible_item_| (which may
  // or may not be the first index) is moved out of |items| below.
  const base::string16& firstTitle = items[first_index]->GetTitle();

  // Ordering is important. Cache the visible item of the restored session
  // before starting the new navigation, which may trigger client lookup of
  // visible item. The visible item of the restored session is the last
  // committed item, because a restored session has no pending or transient
  // item.
  is_restore_session_in_progress_ = true;
  if (last_committed_item_index > -1)
    restored_visible_item_ = std::move(items[last_committed_item_index]);

  LoadURLWithParams(params);

  // On restore prime the first navigation item with the title.  The remaining
  // navItem titles will be set from the WKBackForwardListItem title value.
  NavigationItemImpl* pendingItem = GetPendingItemInCurrentOrRestoredSession();
  if (pendingItem) {
    pendingItem->SetTitle(firstTitle);
  }
}

void WKBasedNavigationManagerImpl::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  if (IsRestoreSessionInProgress() &&
      !wk_navigation_util::IsRestoreSessionUrl(params.url)) {
    AddRestoreCompletionCallback(
        base::BindOnce(&NavigationManagerImpl::LoadURLWithParams,
                       base::Unretained(this), params));
    return;
  }

  NavigationManagerImpl::LoadURLWithParams(params);
}

void WKBasedNavigationManagerImpl::AddRestoreCompletionCallback(
    base::OnceClosure callback) {
  if (!is_restore_session_in_progress_) {
    std::move(callback).Run();
    return;
  }
  restore_session_completion_callbacks_.push_back(std::move(callback));
}

void WKBasedNavigationManagerImpl::LoadIfNecessary() {
  if (!web_view_cache_.IsAttachedToWebView()) {
    // Loading from detached mode is equivalent to restoring cached history.
    // This can happen after clearing browsing data by removing the web view.
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

NavigationItemImpl*
WKBasedNavigationManagerImpl::GetLastCommittedItemInCurrentOrRestoredSession()
    const {
  if (empty_window_open_item_) {
    return empty_window_open_item_.get();
  }

  int index = GetLastCommittedItemIndexInCurrentOrRestoredSession();
  if (index == -1) {
    DCHECK_EQ(0, GetItemCount());
    return nullptr;
  }
  NavigationItemImpl* last_committed_item =
      GetNavigationItemImplAtIndex(static_cast<size_t>(index));
  if (last_committed_item && GetWebState() &&
      !CanTrustLastCommittedItem(last_committed_item)) {
    // Don't check trust level here, as at this point it's expected
    // the _documentURL and the last_commited_item URL have an origin
    // mismatch.
    GURL document_url = GetWebState()->GetCurrentURL(/*trust_level=*/nullptr);
    if (!last_committed_web_view_item_) {
      last_committed_web_view_item_ = CreateNavigationItemWithRewriters(
          /*url=*/GURL::EmptyGURL(), Referrer(),
          ui::PageTransition::PAGE_TRANSITION_LINK,
          NavigationInitiationType::RENDERER_INITIATED,
          /*previous_url=*/GURL::EmptyGURL(),
          nullptr /* use default rewriters only */);
      last_committed_web_view_item_->SetUntrusted();
    }
    last_committed_web_view_item_->SetURL(document_url);
    // Don't expose internal restore session URL's.
    GURL virtual_url;
    if (wk_navigation_util::IsRestoreSessionUrl(document_url) &&
        wk_navigation_util::ExtractTargetURL(document_url, &virtual_url)) {
      if (wk_navigation_util::IsPlaceholderUrl(virtual_url)) {
        last_committed_web_view_item_->SetVirtualURL(
            wk_navigation_util::ExtractUrlFromPlaceholderUrl(virtual_url));
      } else {
        last_committed_web_view_item_->SetVirtualURL(virtual_url);
      }
    } else {
      last_committed_web_view_item_->SetVirtualURL(document_url);
    }
    last_committed_web_view_item_->SetTimestamp(
        time_smoother_.GetSmoothedTime(base::Time::Now()));
    return last_committed_web_view_item_.get();
  }
  return last_committed_item;
}

int WKBasedNavigationManagerImpl::
    GetLastCommittedItemIndexInCurrentOrRestoredSession() const {
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

NavigationItemImpl*
WKBasedNavigationManagerImpl::GetPendingItemInCurrentOrRestoredSession() const {
  if (pending_item_index_ == -1) {
    if (!pending_item_) {
      return delegate_->GetPendingItem();
    }
    return pending_item_.get();
  }
  return GetNavigationItemImplAtIndex(pending_item_index_);
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
    going_to_back_forward_list_item_ = true;
    delegate_->GoToBackForwardListItem(wk_item, item, type, has_user_gesture);
    going_to_back_forward_list_item_ = false;
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

void WKBasedNavigationManagerImpl::FinishLoadURLWithParams(
    NavigationInitiationType initiation_type) {
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

  delegate_->LoadCurrentItem(initiation_type);
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
      // Don't put restore URL's into |cached_items|, extract them first.
      GURL url = cached_items_[index]->GetURL();
      if (wk_navigation_util::IsRestoreSessionUrl(url)) {
        GURL extracted_url;
        if (wk_navigation_util::ExtractTargetURL(url, &extracted_url))
          cached_items_[index]->SetURL(extracted_url);
      }
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

GURL WKBasedNavigationManagerImpl::WKWebViewCache::GetVisibleWebViewURL()
    const {
  if (!IsAttachedToWebView())
    return GURL();

  id<CRWWebViewNavigationProxy> proxy =
      navigation_manager_->delegate_->GetWebViewNavigationProxy();
  if (proxy)
    return net::GURLWithNSURL(proxy.URL);
  return GURL();
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
  new_item->SetTitle(base::SysNSStringToUTF16(wk_item.title));
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

  // TODO(crbug.com/1003680) This seems to happen if a restored navigation fails
  // provisionally before the NavigationContext associates with the original
  // navigation. Rather than expose the internal placeholder to the UI and to
  // URL-sensing components outside of //ios/web layer, set virtual URL to the
  // placeholder original URL here.
  if (wk_navigation_util::IsPlaceholderUrl(url)) {
    new_item->SetVirtualURL(
        wk_navigation_util::ExtractUrlFromPlaceholderUrl(url));
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
