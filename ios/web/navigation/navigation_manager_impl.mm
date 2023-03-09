// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#import <Foundation/Foundation.h>
#import <algorithm>
#import <memory>
#import <utility>

#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/mac/bundle_locations.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/numerics/checked_math.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/elapsed_timer.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

}  // namespace

namespace web {

const char kRestoreNavigationItemCount[] = "IOS.RestoreNavigationItemCount";
const char kRestoreNavigationTime[] = "IOS.RestoreNavigationTime";

NavigationManager::WebLoadParams::WebLoadParams(const GURL& url)
    : url(url),
      transition_type(ui::PAGE_TRANSITION_LINK),
      is_renderer_initiated(false),
      post_data(nil),
      https_upgrade_type(HttpsUpgradeType::kNone) {}

NavigationManager::WebLoadParams::~WebLoadParams() {}

NavigationManager::WebLoadParams::WebLoadParams(const WebLoadParams& other)
    : url(other.url),
      virtual_url(other.virtual_url),
      referrer(other.referrer),
      transition_type(other.transition_type),
      is_renderer_initiated(other.is_renderer_initiated),
      extra_headers([other.extra_headers copy]),
      post_data([other.post_data copy]),
      https_upgrade_type(other.https_upgrade_type) {}

NavigationManager::WebLoadParams& NavigationManager::WebLoadParams::operator=(
    const WebLoadParams& other) {
  url = other.url;
  virtual_url = other.virtual_url;
  referrer = other.referrer;
  is_renderer_initiated = other.is_renderer_initiated;
  transition_type = other.transition_type;
  extra_headers = [other.extra_headers copy];
  post_data = [other.post_data copy];
  https_upgrade_type = other.https_upgrade_type;

  return *this;
}

NavigationManagerImpl::NavigationManagerImpl()
    : delegate_(nullptr),
      browser_state_(nullptr),
      pending_item_index_(-1),
      last_committed_item_index_(-1),
      web_view_cache_(this) {}

NavigationManagerImpl::~NavigationManagerImpl() = default;

void NavigationManagerImpl::SetDelegate(NavigationManagerDelegate* delegate) {
  delegate_ = delegate;
}

void NavigationManagerImpl::SetBrowserState(BrowserState* browser_state) {
  browser_state_ = browser_state;
}

void NavigationManagerImpl::OnNavigationItemCommitted() {
  NavigationItem* item = GetLastCommittedItemInCurrentOrRestoredSession();
  DCHECK(item);
  delegate_->OnNavigationItemCommitted(item);

  if (!wk_navigation_util::IsRestoreSessionUrl(item->GetURL())) {
    restored_visible_item_.reset();
    if (is_restore_session_in_progress_) {
      // There are crashes because restored_visible_item_ is nil and
      // is_restore_session_in_progress_ is true. This is a speculative fix,
      // based on the idea that a navigation item could be committed before
      // OnNavigationStarted is called. See crbug.com/1127434.
      FinalizeSessionRestore();
    }
  }
}

void NavigationManagerImpl::OnNavigationStarted(const GURL& url) {
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

    // Get the last committed item directly because the restoration is in
    // progress so the item returned by the last committed item is the
    // last_committed_web_view_item_ as the origins mistmatch.
    int index = GetLastCommittedItemIndexInCurrentOrRestoredSession();
    DCHECK(index != -1 || 0 == GetItemCount());
    if (index != -1 && restored_visible_item_ &&
        restored_visible_item_->GetUserAgentType() != UserAgentType::NONE) {
      NavigationItemImpl* last_committed_item =
          GetNavigationItemImplAtIndex(static_cast<size_t>(index));
      last_committed_item->SetUserAgentType(
          restored_visible_item_->GetUserAgentType());
    }

    FinalizeSessionRestore();
  }
}

void NavigationManagerImpl::DetachFromWebView() {
  web_view_cache_.DetachFromWebView();
  is_restore_session_in_progress_ = false;
}

void NavigationManagerImpl::AddPendingItem(
    const GURL& url,
    const web::Referrer& referrer,
    ui::PageTransition navigation_type,
    NavigationInitiationType initiation_type,
    bool is_post_navigation,
    HttpsUpgradeType https_upgrade_type) {
  DiscardNonCommittedItems();

  pending_item_index_ = -1;
  NavigationItem* last_committed_item =
      GetLastCommittedItemInCurrentOrRestoredSession();
  pending_item_ = CreateNavigationItemWithRewriters(
      url, referrer, navigation_type, initiation_type, https_upgrade_type,
      last_committed_item ? last_committed_item->GetURL() : GURL::EmptyGURL(),
      &transient_url_rewriters_);
  RemoveTransientURLRewriters();

  if (!next_pending_url_should_skip_serialization_.is_empty() &&
      url == next_pending_url_should_skip_serialization_) {
    pending_item_->SetShouldSkipSerialization(true);
  }
  next_pending_url_should_skip_serialization_ = GURL::EmptyGURL();

  // No need to detect renderer-initiated back/forward navigation in detached
  // mode because there is no renderer.
  if (!web_view_cache_.IsAttachedToWebView())
    return;

  // AddPendingItem is called no later than `didCommitNavigation`. The only time
  // when all three of WKWebView's URL, the pending URL and WKBackForwardList's
  // current item URL are identical before `didCommitNavigation` is when the
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

  // Restore the UserAgent when navigating forward to a Session restoration URL.
  if (navigation_type & ui::PAGE_TRANSITION_RELOAD &&
      !(navigation_type & ui::PAGE_TRANSITION_FORWARD_BACK) &&
      web::wk_navigation_util::IsRestoreSessionUrl(current_item_url) &&
      GetNavigationItemFromWKItem(current_wk_item) &&
      GetNavigationItemFromWKItem(current_wk_item)->GetUserAgentType() !=
          UserAgentType::NONE &&
      wk_navigation_util::URLNeedsUserAgentType(pending_item_->GetURL())) {
    pending_item_->SetUserAgentType(
        GetNavigationItemFromWKItem(current_wk_item)->GetUserAgentType());
  }

  BOOL isCurrentURLSameAsPending =
      current_item_url == pending_item_->GetURL() &&
      current_item_url == net::GURLWithNSURL(proxy.URL);

  bool is_form_post =
      is_post_navigation &&
      (navigation_type & ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT);
  if (proxy.backForwardList.currentItem && isCurrentURLSameAsPending &&
      !is_form_post) {
    pending_item_index_ = web_view_cache_.GetCurrentItemIndex();

    // If `currentItem` is not already associated with a NavigationItemImpl,
    // associate the newly created item with it. Otherwise, discard the new item
    // since it will be a duplicate.
    NavigationItemImpl* current_item =
        GetNavigationItemFromWKItem(current_wk_item);
    ui::PageTransition transition = pending_item_->GetTransitionType();
    if (!current_item) {
      current_item = pending_item_.get();
      SetNavigationItemInWKItem(current_wk_item, std::move(pending_item_));
    }
    // Updating the transition type of the item is needed, for example when
    // doing a FormSubmit with a GET method on the same URL. See
    // crbug.com/1211879.
    current_item->SetTransitionType(transition);

    pending_item_.reset();
  }
}

void NavigationManagerImpl::CommitPendingItem() {
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

  if (pending_item_index_ == -1) {
    pending_item_->ResetForCommit();
    pending_item_->SetTimestamp(
        time_smoother_.GetSmoothedTime(base::Time::Now()));

    id<CRWWebViewNavigationProxy> proxy =
        delegate_->GetWebViewNavigationProxy();

    // If WKBackForwardList exists but `currentItem` is nil at this point, it is
    // because the current navigation is an empty window open navigation.
    // If `currentItem` is not nil, it is the last committed item in the
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
  // If the newly committed item is the empty window open item, fake an index of
  // 0 because WKBackForwardList is empty at this point.
  last_committed_item_index_ =
      empty_window_open_item_ ? 0 : web_view_cache_.GetCurrentItemIndex();
  OnNavigationItemCommitted();
}

void NavigationManagerImpl::CommitPendingItem(
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

  item->ResetForCommit();
  item->SetTimestamp(time_smoother_.GetSmoothedTime(base::Time::Now()));

  id<CRWWebViewNavigationProxy> proxy = delegate_->GetWebViewNavigationProxy();

  // If WKBackForwardList exists but `currentItem` is nil at this point, it is
  // because the current navigation is an empty window open navigation.
  // If `currentItem` is not nil, it is the last committed item in the
  // WKWebView.
  if (proxy.backForwardList && !proxy.backForwardList.currentItem) {
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
      // Sometimes `currentItem.URL` is not updated correctly while the webView
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
        // `back_forward_list.currentItem.URL` doesn't get updated when going
        // from a file:// scheme to about:// scheme.
        SetNavigationItemInWKItem(back_forward_list.currentItem,
                                  std::move(item));
      }
    }
  }

  // If the newly committed item is the empty window open item, fake an index of
  // 0 because WKBackForwardList is empty at this point.
  last_committed_item_index_ =
      empty_window_open_item_ ? 0 : web_view_cache_.GetCurrentItemIndex();
  OnNavigationItemCommitted();
}

std::unique_ptr<web::NavigationItemImpl>
NavigationManagerImpl::ReleasePendingItem() {
  return std::move(pending_item_);
}

void NavigationManagerImpl::SetPendingItem(
    std::unique_ptr<web::NavigationItemImpl> item) {
  pending_item_ = std::move(item);
}

int NavigationManagerImpl::GetIndexForOffset(int offset) const {
  int current_item_index = pending_item_index_;
  if (pending_item_index_ == -1) {
    current_item_index =
        empty_window_open_item_ ? 0 : web_view_cache_.GetCurrentItemIndex();
  }

  // Handled signed integer overflow or underflow.
  int index;
  if (!base::CheckAdd(current_item_index, offset).AssignIfValid(&index)) {
    return -1;
  }

  return index;
}

void NavigationManagerImpl::SetPendingItemIndex(int index) {
  pending_item_index_ = index;
}

void NavigationManagerImpl::SetWKWebViewNextPendingUrlNotSerializable(
    const GURL& url) {
  next_pending_url_should_skip_serialization_ = url;
}

bool NavigationManagerImpl::RestoreNativeSession(const GURL& url) {
  DCHECK(is_restore_session_in_progress_);

  GURL targetURL;
  if (!web::wk_navigation_util::IsRestoreSessionUrl(url) ||
      web::wk_navigation_util::ExtractTargetURL(url, &targetURL)) {
    return false;
  }

  if (!web::GetWebClient()->RestoreSessionFromCache(GetWebState()) &&
      !synthesized_restore_helper_.Restore(GetWebState())) {
    return false;
  }

  // Native restore worked, abort unsafe restore.
  DiscardNonCommittedItems();
  last_committed_item_index_ = web_view_cache_.GetCurrentItemIndex();
  if (restored_visible_item_ &&
      restored_visible_item_->GetUserAgentType() != UserAgentType::NONE) {
    NavigationItem* last_committed_item =
        GetLastCommittedItemInCurrentOrRestoredSession();
    if (last_committed_item) {
      last_committed_item->SetUserAgentType(
          restored_visible_item_->GetUserAgentType());
    }
  }
  restored_visible_item_.reset();
  FinalizeSessionRestore();
  return true;
}

void NavigationManagerImpl::RemoveTransientURLRewriters() {
  transient_url_rewriters_.clear();
}

void NavigationManagerImpl::UpdatePendingItemUrl(const GURL& url) const {
  // If there is no pending item, navigation is probably happening within the
  // back forward history. Don't modify the item list.
  NavigationItemImpl* pending_item = GetPendingItemInCurrentOrRestoredSession();
  if (!pending_item || url == pending_item->GetURL())
    return;

  // UpdatePendingItemUrl is used to handle redirects after loading starts for
  // the currenting pending item.
  pending_item->SetURL(url);
  pending_item->SetVirtualURL(url);
  // Redirects (3xx response code), or client side navigation must change POST
  // requests to GETs.
  pending_item->SetPostData(nil);
  pending_item->ResetHttpRequestHeaders();
}

NavigationItemImpl* NavigationManagerImpl::GetCurrentItemImpl() const {
  NavigationItemImpl* pending_item = GetPendingItemInCurrentOrRestoredSession();
  if (pending_item)
    return pending_item;

  return GetLastCommittedItemInCurrentOrRestoredSession();
}

NavigationItemImpl* NavigationManagerImpl::GetLastCommittedItemImpl() const {
  // GetLastCommittedItemImpl() should return null while session restoration is
  // in progress and real item after the first post-restore navigation is
  // finished. IsRestoreSessionInProgress(), will return true until the first
  // post-restore is started.
  if (IsRestoreSessionInProgress())
    return nullptr;

  NavigationItemImpl* result = GetLastCommittedItemInCurrentOrRestoredSession();
  if (!result || wk_navigation_util::IsRestoreSessionUrl(result->GetURL())) {
    // Session restoration has completed, but the first post-restore navigation
    // has not finished yet, so there is no committed URLs in the navigation
    // stack.
    return nullptr;
  }

  return result;
}

void NavigationManagerImpl::UpdateCurrentItemForReplaceState(
    const GURL& url,
    NSString* state_object) {
  NavigationItemImpl* current_item = GetCurrentItemImpl();
  current_item->SetURL(url);
  current_item->SetSerializedStateObject(state_object);
  current_item->SetPostData(nil);
}

void NavigationManagerImpl::GoToIndex(int index,
                                      NavigationInitiationType initiation_type,
                                      bool has_user_gesture) {
  if (index < 0 || index >= GetItemCount()) {
    // There are bugs in WKWebView where the back/forward list can fall out
    // of sync with reality. In these situations, a navigation item that
    // appears in the back or forward list might not actually exist. See
    // crbug.com/1407244.
    return;
  }

  delegate_->RecordPageStateInNavigationItem();
  delegate_->ClearDialogs();

  if (!web_view_cache_.IsAttachedToWebView()) {
    // GoToIndex from detached mode is equivalent to restoring history with
    // `last_committed_item_index` updated to `index`.
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
    delegate_->GoToBackForwardListItem(wk_item, item, initiation_type,
                                       has_user_gesture);
    going_to_back_forward_list_item_ = false;
  } else {
    DCHECK(index == 0 && empty_window_open_item_)
        << " wk_item should not be nullptr. index: " << index
        << " has_empty_window_open_item: "
        << (empty_window_open_item_ != nullptr);
  }
}

void NavigationManagerImpl::GoToIndex(int index) {
  // Silently return if still on a restore URL.  This state should only last a
  // few moments, but may be triggered when a user mashes the back or forward
  // button quickly.
  NavigationItemImpl* item = GetLastCommittedItemInCurrentOrRestoredSession();
  if (item && wk_navigation_util::IsRestoreSessionUrl(item->GetURL())) {
    return;
  }
  GoToIndex(index, NavigationInitiationType::BROWSER_INITIATED,
            /*has_user_gesture=*/true);
}

BrowserState* NavigationManagerImpl::GetBrowserState() const {
  return browser_state_;
}

WebState* NavigationManagerImpl::GetWebState() const {
  return delegate_->GetWebState();
}

NavigationItem* NavigationManagerImpl::GetVisibleItem() const {
  if (is_restore_session_in_progress_ || restored_visible_item_)
    return restored_visible_item_.get();

  // Only return pending_item_ for new (non-history), user-initiated
  // navigations in order to prevent URL spoof attacks.
  NavigationItemImpl* pending_item = GetPendingItemInCurrentOrRestoredSession();
  if (pending_item) {
    bool is_user_initiated = pending_item->NavigationInitiationType() ==
                             NavigationInitiationType::BROWSER_INITIATED;
    bool safe_to_show_pending = is_user_initiated &&
                                pending_item_index_ == -1 &&
                                GetWebState()->IsLoading();
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

NavigationItem* NavigationManagerImpl::GetLastCommittedItem() const {
  return GetLastCommittedItemImpl();
}

int NavigationManagerImpl::GetLastCommittedItemIndex() const {
  // GetLastCommittedItemIndex() should return -1 while session restoration is
  // in progress and real item after the first post-restore navigation is
  // finished. IsRestoreSessionInProgress(), will return true until the first
  // post-restore is started.
  if (IsRestoreSessionInProgress())
    return -1;

  NavigationItem* item = GetLastCommittedItemInCurrentOrRestoredSession();
  if (!item || wk_navigation_util::IsRestoreSessionUrl(item->GetURL())) {
    // Session restoration has completed, but the first post-restore
    // navigation has not finished yet, so there is no committed URLs in the
    // navigation stack.
    return -1;
  }

  return GetLastCommittedItemIndexInCurrentOrRestoredSession();
}

NavigationItem* NavigationManagerImpl::GetPendingItem() const {
  if (IsRestoreSessionInProgress())
    return nullptr;
  return GetPendingItemInCurrentOrRestoredSession();
}

void NavigationManagerImpl::DiscardNonCommittedItems() {
  pending_item_.reset();
  pending_item_index_ = -1;
}

void NavigationManagerImpl::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  if (IsRestoreSessionInProgress() &&
      !wk_navigation_util::IsRestoreSessionUrl(params.url)) {
    AddRestoreCompletionCallback(
        base::BindOnce(&NavigationManagerImpl::LoadURLWithParams,
                       base::Unretained(this), params));
    return;
  }

  DCHECK(!(params.transition_type & ui::PAGE_TRANSITION_FORWARD_BACK));
  delegate_->ClearDialogs();
  delegate_->RecordPageStateInNavigationItem();

  NavigationInitiationType initiation_type =
      params.is_renderer_initiated
          ? NavigationInitiationType::RENDERER_INITIATED
          : NavigationInitiationType::BROWSER_INITIATED;
  AddPendingItem(params.url, params.referrer, params.transition_type,
                 initiation_type, /*is_post_navigation=*/false,
                 params.https_upgrade_type);

  // Mark pending item as created from hash change if necessary. This is needed
  // because window.hashchange message may not arrive on time.
  NavigationItemImpl* pending_item = GetPendingItemInCurrentOrRestoredSession();
  if (pending_item) {
    NavigationItem* last_committed_item =
        GetLastCommittedItemInCurrentOrRestoredSession();
    GURL last_committed_url = last_committed_item
                                  ? last_committed_item->GetVirtualURL()
                                  : GURL::EmptyGURL();
    GURL pending_url = pending_item->GetURL();
    if (last_committed_url != pending_url &&
        last_committed_url.EqualsIgnoringRef(pending_url)) {
      pending_item->SetIsCreatedFromHashChange(true);
    }

    if (params.virtual_url.is_valid())
      pending_item->SetVirtualURL(params.virtual_url);

    pending_item->SetHttpsUpgradeType(params.https_upgrade_type);
  }

  // Add additional headers to the NavigationItem before loading it in the web
  // view.
  NavigationItemImpl* added_item =
      pending_item ? pending_item
                   : GetLastCommittedItemInCurrentOrRestoredSession();
  DCHECK(added_item);
  if (params.extra_headers)
    added_item->AddHttpRequestHeaders(params.extra_headers);

  added_item->SetHttpsUpgradeType(params.https_upgrade_type);

  if (params.post_data) {
    DCHECK([added_item->GetHttpRequestHeaders() objectForKey:@"Content-Type"])
        << "Post data should have an associated content type";
    added_item->SetPostData(params.post_data);
  }

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
      cached_items[next_item_index] = std::move(pending_item_);
      Restore(next_item_index, std::move(cached_items));
      DCHECK(web_view_cache_.IsAttachedToWebView());
      return;
    }
    web_view_cache_.ResetToAttached();
  }

  delegate_->LoadCurrentItem(initiation_type);
}

void NavigationManagerImpl::LoadIfNecessary() {
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

void NavigationManagerImpl::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {
  DCHECK(rewriter);
  transient_url_rewriters_.push_back(rewriter);
}

int NavigationManagerImpl::GetItemCount() const {
  if (empty_window_open_item_) {
    return 1;
  }

  return web_view_cache_.GetBackForwardListItemCount();
}

NavigationItem* NavigationManagerImpl::GetItemAtIndex(size_t index) const {
  return GetNavigationItemImplAtIndex(index);
}

int NavigationManagerImpl::GetIndexOfItem(const NavigationItem* item) const {
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

int NavigationManagerImpl::GetPendingItemIndex() const {
  if (is_restore_session_in_progress_)
    return -1;
  return pending_item_index_;
}

bool NavigationManagerImpl::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool NavigationManagerImpl::CanGoForward() const {
  return CanGoToOffset(1);
}

bool NavigationManagerImpl::CanGoToOffset(int offset) const {
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

void NavigationManagerImpl::GoBack() {
  GoToIndex(GetIndexForOffset(-1));
}

void NavigationManagerImpl::GoForward() {
  GoToIndex(GetIndexForOffset(1));
}

void NavigationManagerImpl::Reload(ReloadType reload_type,
                                   bool check_for_reposts) {
  if (IsRestoreSessionInProgress()) {
    // Do not interrupt session restoration process. Last committed item will
    // eventually reload once the session is restored.
    return;
  }

  // Use GetLastCommittedItemInCurrentOrRestoredSession() instead of
  // GetLastCommittedItem() so restore session URL's aren't suppressed.
  // Otherwise a cancelled/stopped navigation during the first post-restore
  // navigation will always return early from Reload.
  if (!GetPendingItem() && !GetLastCommittedItemInCurrentOrRestoredSession())
    return;

  delegate_->ClearDialogs();

  // Reload with ORIGINAL_REQUEST_URL type should reload with the original
  // request url of the pending item, or last committed item if the pending item
  // doesn't exist. The reason is that a server side redirect may change the
  // item's url. For example, the user visits www.chromium.org and is then
  // redirected to m.chromium.org, when the user wants to refresh the page with
  // a different configuration (e.g. user agent), the user would be expecting to
  // visit www.chromium.org instead of m.chromium.org.
  if (reload_type == web::ReloadType::ORIGINAL_REQUEST_URL) {
    NavigationItem* reload_item = nullptr;
    if (GetPendingItem())
      reload_item = GetPendingItem();
    else
      reload_item = GetLastCommittedItemInCurrentOrRestoredSession();
    DCHECK(reload_item);

    reload_item->SetURL(reload_item->GetOriginalRequestURL());
  }

  if (!web_view_cache_.IsAttachedToWebView()) {
    // Reload from detached mode is equivalent to restoring history unchanged.
    Restore(web_view_cache_.GetCurrentItemIndex(),
            web_view_cache_.ReleaseCachedItems());
    DCHECK(web_view_cache_.IsAttachedToWebView());
    return;
  }

  delegate_->Reload();
}

void NavigationManagerImpl::ReloadWithUserAgentType(
    UserAgentType user_agent_type) {
  DCHECK_NE(user_agent_type, UserAgentType::NONE);

  NavigationItem* item_to_reload = GetVisibleItem();
  if (!item_to_reload) {
    NavigationItem* last_committed_item = GetLastCommittedItem();
    if (last_committed_item) {
      item_to_reload = last_committed_item;
    }
  }

  if (!item_to_reload)
    return;

  // `reloadURL` will be empty if a page was open by DOM.
  GURL reload_url(item_to_reload->GetOriginalRequestURL());
  if (reload_url.is_empty()) {
    reload_url = item_to_reload->GetVirtualURL();
  }

  WebLoadParams params(reload_url);
  if (item_to_reload->GetVirtualURL() != reload_url)
    params.virtual_url = item_to_reload->GetVirtualURL();
  params.referrer = item_to_reload->GetReferrer();
  params.transition_type = ui::PAGE_TRANSITION_RELOAD;

  delegate_->SetWebStateUserAgent(user_agent_type);
  item_to_reload->SetUserAgentType(user_agent_type);

  LoadURLWithParams(params);
}

std::vector<NavigationItem*> NavigationManagerImpl::GetBackwardItems() const {
  std::vector<NavigationItem*> items;

  if (is_restore_session_in_progress_)
    return items;

  int current_back_forward_item_index = web_view_cache_.GetCurrentItemIndex();
  for (int index = current_back_forward_item_index - 1; index >= 0; index--) {
    items.push_back(GetItemAtIndex(index));
  }

  return items;
}

std::vector<NavigationItem*> NavigationManagerImpl::GetForwardItems() const {
  std::vector<NavigationItem*> items;

  if (is_restore_session_in_progress_)
    return items;

  for (int index = web_view_cache_.GetCurrentItemIndex() + 1;
       index < GetItemCount(); index++) {
    items.push_back(GetItemAtIndex(index));
  }
  return items;
}

void NavigationManagerImpl::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
  DCHECK(!is_restore_session_in_progress_);
  WillRestore(items.size());

  DCHECK_LT(last_committed_item_index, static_cast<int>(items.size()));
  DCHECK(items.empty() || last_committed_item_index >= 0);

  if (!web_view_cache_.IsAttachedToWebView())
    web_view_cache_.ResetToAttached();

  if (items.empty())
    return;

  DiscardNonCommittedItems();
  if (GetItemCount() > 0) {
    delegate_->RemoveWebView();
  }
  DCHECK_EQ(0, GetItemCount());
  DCHECK_EQ(-1, pending_item_index_);
  last_committed_item_index_ = -1;
  UnsafeRestore(last_committed_item_index, std::move(items));
}

bool NavigationManagerImpl::IsRestoreSessionInProgress() const {
  return is_restore_session_in_progress_;
}

void NavigationManagerImpl::AddRestoreCompletionCallback(
    base::OnceClosure callback) {
  if (!is_restore_session_in_progress_) {
    std::move(callback).Run();
    return;
  }
  restore_session_completion_callbacks_.push_back(std::move(callback));
}

NavigationItemImpl*
NavigationManagerImpl::GetPendingItemInCurrentOrRestoredSession() const {
  if (pending_item_index_ == -1) {
    if (!pending_item_) {
      return delegate_->GetPendingItem();
    }
    return pending_item_.get();
  }
  return GetNavigationItemImplAtIndex(pending_item_index_);
}

NavigationItemImpl*
NavigationManagerImpl::GetLastCommittedItemInCurrentOrRestoredSession() const {
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
          NavigationInitiationType::RENDERER_INITIATED, HttpsUpgradeType::kNone,
          /*previous_url=*/GURL::EmptyGURL(),
          nullptr /* use default rewriters only */);
      last_committed_web_view_item_->SetUntrusted();
    }
    last_committed_web_view_item_->SetURL(document_url);
    // Don't expose internal restore session URL's.
    GURL virtual_url;
    if (wk_navigation_util::IsRestoreSessionUrl(document_url) &&
        wk_navigation_util::ExtractTargetURL(document_url, &virtual_url)) {
      last_committed_web_view_item_->SetVirtualURL(virtual_url);
    } else {
      last_committed_web_view_item_->SetVirtualURL(document_url);
    }
    last_committed_web_view_item_->SetTimestamp(
        time_smoother_.GetSmoothedTime(base::Time::Now()));
    return last_committed_web_view_item_.get();
  }
  return last_committed_item;
}

int NavigationManagerImpl::GetLastCommittedItemIndexInCurrentOrRestoredSession()
    const {
  // WKBackForwardList's `currentItem` is usually the last committed item,
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

NavigationItemImpl* NavigationManagerImpl::GetNavigationItemImplAtIndex(
    size_t index) const {
  if (empty_window_open_item_) {
    // Return nullptr for index != 0 instead of letting the code fall through
    // (which in most cases will return null anyways because wk_item should be
    // nil) for the slim chance that WKBackForwardList has been updated for a
    // new navigation but WKWebView has not triggered the `didCommitNavigation:`
    // callback. NavigationItem for the new wk_item should not be returned until
    // after DidCommitPendingItem() is called.
    return index == 0 ? empty_window_open_item_.get() : nullptr;
  }

  return web_view_cache_.GetNavigationItemImplAtIndex(
      index, true /* create_if_missing */);
}

void NavigationManagerImpl::RestoreItemsState(
    RestoreItemListType list_type,
    std::vector<std::unique_ptr<NavigationItem>> items_restored) {
  bool back_list = list_type == RestoreItemListType::kBackList;
  size_t current_item_index = web_view_cache_.GetCurrentItemIndex();
  size_t cache_offset = back_list ? 0 : current_item_index + 1;
  size_t cache_limit = back_list
                           ? current_item_index
                           : web_view_cache_.GetBackForwardListItemCount();

  for (size_t index = 0; index < items_restored.size(); index++) {
    size_t cache_index = index + cache_offset;
    if (cache_index >= cache_limit)
      break;

    NavigationItemImpl* cached_item =
        web_view_cache_.GetNavigationItemImplAtIndex(
            cache_index, true /* create_if_missing */);
    NavigationItem* restore_item = items_restored[index].get();

    // `cached_item` appears to be nil sometimes, perhaps due to a mismatch in
    // WKWebView's backForwardList.  Returning early here may break some restore
    // state features, but should not put the user in a broken state.
    if (!cached_item || !restore_item) {
      continue;
    }

    bool is_same_url = cached_item->GetURL() == restore_item->GetURL();
    if (wk_navigation_util::IsRestoreSessionUrl(cached_item->GetURL())) {
      GURL target_url;
      if (wk_navigation_util::ExtractTargetURL(cached_item->GetURL(),
                                               &target_url))
        is_same_url = target_url == restore_item->GetURL();
    }

    if (is_same_url) {
      cached_item->RestoreStateFromItem(restore_item);
    }
  }
}

void NavigationManagerImpl::UnsafeRestore(
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

  bool off_the_record = browser_state_->IsOffTheRecord();
  synthesized_restore_helper_.Init(last_committed_item_index, items,
                                   off_the_record);

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

  // Grab the title of the first item before `restored_visible_item_` (which may
  // or may not be the first index) is moved out of `items` below.
  const std::u16string& firstTitle = items[first_index]->GetTitle();

  // Ordering is important. Cache the visible item of the restored session
  // before starting the new navigation, which may trigger client lookup of
  // visible item. The visible item of the restored session is the last
  // committed item, because a restored session has no pending item.
  is_restore_session_in_progress_ = true;
  if (last_committed_item_index > -1)
    restored_visible_item_ = std::move(items[last_committed_item_index]);

  std::vector<std::unique_ptr<NavigationItem>> back_items;
  for (int index = 0; index < last_committed_item_index; index++) {
    back_items.push_back(std::move(items[index]));
  }

  std::vector<std::unique_ptr<NavigationItem>> forward_items;
  for (size_t index = last_committed_item_index + 1; index < items.size();
       index++) {
    forward_items.push_back(std::move(items[index]));
  }

  AddRestoreCompletionCallback(base::BindOnce(
      &NavigationManagerImpl::RestoreItemsState, base::Unretained(this),
      RestoreItemListType::kBackList, std::move(back_items)));
  AddRestoreCompletionCallback(base::BindOnce(
      &NavigationManagerImpl::RestoreItemsState, base::Unretained(this),
      RestoreItemListType::kForwardList, std::move(forward_items)));

  LoadURLWithParams(params);

  // On restore prime the first navigation item with the title.  The remaining
  // navItem titles will be set from the WKBackForwardListItem title value.
  NavigationItemImpl* pendingItem = GetPendingItemInCurrentOrRestoredSession();
  if (pendingItem) {
    pendingItem->SetTitle(firstTitle);
  }
}

void NavigationManagerImpl::WillRestore(size_t item_count) {
  // It should be uncommon for the user to have more than 100 items in their
  // session, so bucketing 100+ logs together is fine.
  UMA_HISTOGRAM_COUNTS_100(kRestoreNavigationItemCount, item_count);
}

void NavigationManagerImpl::RewriteItemURLIfNecessary(
    NavigationItem* item) const {
  GURL url = item->GetURL();
  if (web::BrowserURLRewriter::GetInstance()->RewriteURLIfNecessary(
          &url, browser_state_)) {
    // `url` must be set first for -SetVirtualURL to not no-op.
    GURL virtual_url = item->GetURL();
    item->SetURL(url);
    item->SetVirtualURL(virtual_url);
  }
}

std::unique_ptr<NavigationItemImpl>
NavigationManagerImpl::CreateNavigationItemWithRewriters(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    NavigationInitiationType initiation_type,
    HttpsUpgradeType https_upgrade_type,
    const GURL& previous_url,
    const std::vector<BrowserURLRewriter::URLRewriter>* additional_rewriters)
    const {
  GURL loaded_url(url);

  // Navigation code relies on this special URL to implement native view and
  // WebUI, and rewriter code should not be exposed to this special type of
  // about:blank URL.
  bool url_was_rewritten = false;
  if (additional_rewriters && !additional_rewriters->empty()) {
    url_was_rewritten = web::BrowserURLRewriter::RewriteURLWithWriters(
        &loaded_url, browser_state_, *additional_rewriters);
  }

    if (!url_was_rewritten) {
      web::BrowserURLRewriter::GetInstance()->RewriteURLIfNecessary(
          &loaded_url, browser_state_);
    }

  // The URL should not be changed to app-specific URL if the load is
  // renderer-initiated or a reload requested by non-app-specific URL. Pages
  // with app-specific urls have elevated previledges and should not be allowed
  // to open app-specific URLs.
  if ((initiation_type == web::NavigationInitiationType::RENDERER_INITIATED ||
       PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) &&
      loaded_url != url && web::GetWebClient()->IsAppSpecificURL(loaded_url) &&
      !web::GetWebClient()->IsAppSpecificURL(previous_url)) {
    loaded_url = url;
  }

  GURL original_url = loaded_url;
  if (ui::PageTransitionIsRedirect(transition) && GetLastCommittedItem()) {
    original_url = GetLastCommittedItem()->GetURL();
  }

  auto item = std::make_unique<NavigationItemImpl>();
  item->SetOriginalRequestURL(original_url);
  item->SetURL(loaded_url);
  item->SetReferrer(referrer);
  item->SetTransitionType(transition);
  item->SetNavigationInitiationType(initiation_type);
  item->SetHttpsUpgradeType(https_upgrade_type);

  return item;
}

NavigationItem* NavigationManagerImpl::GetLastCommittedItemWithUserAgentType()
    const {
  for (int index = GetLastCommittedItemIndexInCurrentOrRestoredSession();
       index >= 0; index--) {
    NavigationItem* item = GetItemAtIndex(index);
    if (wk_navigation_util::URLNeedsUserAgentType(item->GetURL())) {
      DCHECK_NE(item->GetUserAgentType(), UserAgentType::NONE);
      return item;
    }
  }
  return nullptr;
}

bool NavigationManagerImpl::CanTrustLastCommittedItem(
    const NavigationItem* last_committed_item) const {
  DCHECK(last_committed_item);
  if (!web_view_cache_.IsAttachedToWebView())
    return true;

  // Fast back-forward navigations can be performed synchronously, with the
  // WKWebView.URL updated before enough callbacks occur to update the
  // last committed item.  As a result, any calls to
  // -CanTrustLastCommittedItem during a call to WKWebView
  // -goToBackForwardListItem are wrapped in the
  // `going_to_back_forward_list_item_` flag. This flag is set and immediately
  // unset because the the mismatch between URL and last_committed_item is
  // expected.
  if (going_to_back_forward_list_item_)
    return true;

  // Only compare origins, as any mismatch between `web_view_url` and
  // `last_committed_url` with the same origin are safe to return as
  // visible.
  const GURL& web_view_origin_url =
      web_view_cache_.GetVisibleWebViewOriginURL();
  const GURL& last_committed_url = last_committed_item->GetURL();
  if (web_view_origin_url == last_committed_url.DeprecatedGetOriginAsURL())
    return true;

  // WKWebView.URL will update immediately when navigating to and from
  // about, file or chrome scheme URLs.
  if (web_view_origin_url.SchemeIs(url::kAboutScheme) ||
      last_committed_url.SchemeIs(url::kAboutScheme) ||
      web_view_origin_url.SchemeIs(url::kFileScheme) ||
      last_committed_url.SchemeIs(url::kFileScheme) ||
      web::GetWebClient()->IsAppSpecificURL(web_view_origin_url) ||
      web::GetWebClient()->IsAppSpecificURL(last_committed_url)) {
    return true;
  }

  return false;
}

void NavigationManagerImpl::FinalizeSessionRestore() {
  is_restore_session_in_progress_ = false;
  synthesized_restore_helper_.Clear();

  for (base::OnceClosure& callback : restore_session_completion_callbacks_) {
    std::move(callback).Run();
  }
  restore_session_completion_callbacks_.clear();
  LoadIfNecessary();
}

NavigationManagerImpl::WKWebViewCache::WKWebViewCache(
    NavigationManagerImpl* navigation_manager)
    : navigation_manager_(navigation_manager), attached_to_web_view_(true) {}

NavigationManagerImpl::WKWebViewCache::~WKWebViewCache() = default;

bool NavigationManagerImpl::WKWebViewCache::IsAttachedToWebView() const {
  return attached_to_web_view_;
}

void NavigationManagerImpl::WKWebViewCache::DetachFromWebView() {
  if (IsAttachedToWebView()) {
    cached_current_item_index_ = GetCurrentItemIndex();
    cached_items_.resize(GetBackForwardListItemCount());
    for (size_t index = 0; index < GetBackForwardListItemCount(); index++) {
      cached_items_[index].reset(new NavigationItemImpl(
          *GetNavigationItemImplAtIndex(index, true /* create_if_missing */)));
      // Don't put restore URL's into `cached_items`, extract them first.
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

void NavigationManagerImpl::WKWebViewCache::ResetToAttached() {
  cached_items_.clear();
  cached_current_item_index_ = -1;
  attached_to_web_view_ = true;
}

std::vector<std::unique_ptr<NavigationItem>>
NavigationManagerImpl::WKWebViewCache::ReleaseCachedItems() {
  DCHECK(!IsAttachedToWebView());
  std::vector<std::unique_ptr<NavigationItem>> result(cached_items_.size());
  for (size_t index = 0; index < cached_items_.size(); index++) {
    result[index] = std::move(cached_items_[index]);
  }
  cached_items_.clear();
  return result;
}

size_t NavigationManagerImpl::WKWebViewCache::GetBackForwardListItemCount()
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

const GURL& NavigationManagerImpl::WKWebViewCache::GetVisibleWebViewOriginURL()
    const {
  if (!IsAttachedToWebView())
    return GURL::EmptyGURL();

  id<CRWWebViewNavigationProxy> proxy =
      navigation_manager_->delegate_->GetWebViewNavigationProxy();
  if (proxy) {
    if (![cached_visible_host_nsstring_ isEqualToString:proxy.URL.host] ||
        ![cached_visible_scheme_nsstring_ isEqualToString:proxy.URL.scheme]) {
      cached_visible_origin_url_ =
          net::GURLWithNSURL(proxy.URL).DeprecatedGetOriginAsURL();
      cached_visible_host_nsstring_ = proxy.URL.host;
      cached_visible_scheme_nsstring_ = proxy.URL.scheme;
    }
    return cached_visible_origin_url_;
  }
  return GURL::EmptyGURL();
}

int NavigationManagerImpl::WKWebViewCache::GetCurrentItemIndex() const {
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
NavigationManagerImpl::WKWebViewCache::GetNavigationItemImplAtIndex(
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

  WKBackForwardListItem* prev_wk_item =
      index == 0 ? nil : GetWKItemAtIndex(index - 1);
  std::unique_ptr<web::NavigationItemImpl> new_item =
      navigation_manager_->CreateNavigationItemWithRewriters(
          net::GURLWithNSURL(wk_item.URL),
          (prev_wk_item ? web::Referrer(net::GURLWithNSURL(prev_wk_item.URL),
                                        web::ReferrerPolicyAlways)
                        : web::Referrer()),
          ui::PageTransition::PAGE_TRANSITION_LINK,
          NavigationInitiationType::RENDERER_INITIATED, HttpsUpgradeType::kNone,
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
      new_item->SetVirtualURL(virtual_url);
    }
  }

  SetNavigationItemInWKItem(wk_item, std::move(new_item));
  return GetNavigationItemFromWKItem(wk_item);
}

WKBackForwardListItem* NavigationManagerImpl::WKWebViewCache::GetWKItemAtIndex(
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
