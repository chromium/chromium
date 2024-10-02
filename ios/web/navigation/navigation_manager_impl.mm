// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#import <Foundation/Foundation.h>

#import <algorithm>
#import <memory>
#import <utility>

#import "base/containers/span.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
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
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/page_transition_types.h"

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

void RecordSessionRestorationHasFetchers(bool has_fetchers) {
  base::UmaHistogramBoolean("Session.WebStates.NativeRestoreHasFetchers",
                            has_fetchers);
}

// Records metrics about session restoration `success` from `source`.
void RecordSessionRestorationResultForSource(
    bool success,
    web::NavigationManagerImpl::SessionDataBlobSource source) {
  switch (source) {
    case web::NavigationManagerImpl::SessionDataBlobSource::kSessionCache:
      base::UmaHistogramBoolean(
          "Session.WebStates.NativeRestoreSessionFromCache", success);
      break;

    case web::NavigationManagerImpl::SessionDataBlobSource::kSynthesized:
      base::UmaHistogramBoolean("Session.WebStates.NativeRestoreSession",
                                success);
      break;
  }
}

void RecordSessionRestorationFetcherHasDataForSource(
    web::NavigationManagerImpl::SessionDataBlobSource source,
    bool fetcher_has_data) {
  switch (source) {
    case web::NavigationManagerImpl::SessionDataBlobSource::kSessionCache:
      base::UmaHistogramBoolean(
          "Session.WebStates.NativeRestoreSessionFromCacheHasData",
          fetcher_has_data);
      break;

    case web::NavigationManagerImpl::SessionDataBlobSource::kSynthesized:
      base::UmaHistogramBoolean("Session.WebStates.NativeRestoreSessionHasData",
                                fetcher_has_data);
      break;
  }
}

}  // namespace

namespace web {

const char kRestoreNavigationItemCount[] = "IOS.RestoreNavigationItemCount";

NavigationManager::WebLoadParams::WebLoadParams(const GURL& url) : url(url) {}

NavigationManager::WebLoadParams::~WebLoadParams() = default;

NavigationManager::WebLoadParams::WebLoadParams(const WebLoadParams& other) =
    default;

NavigationManager::WebLoadParams& NavigationManager::WebLoadParams::operator=(
    const WebLoadParams& other) = default;

NavigationManagerImpl::NavigationManagerImpl(
    BrowserState* browser_state,
    NavigationManagerDelegate* delegate)
    : delegate_(delegate), browser_state_(browser_state) {
  CHECK(browser_state_);
  CHECK(delegate_);
}

NavigationManagerImpl::~NavigationManagerImpl() = default;

void NavigationManagerImpl::RestoreFromProto(
    const proto::NavigationStorage& storage) {
  std::vector<std::unique_ptr<NavigationItem>> items;
  items.reserve(storage.items_size());

  for (const auto& item_storage : storage.items()) {
    auto item = std::make_unique<NavigationItemImpl>(item_storage);
    RewriteItemURLIfNecessary(item.get());
    items.push_back(std::move(item));
  }

  Restore(storage.last_committed_item_index(), std::move(items));
}

void NavigationManagerImpl::SerializeToProto(
    proto::NavigationStorage& storage) const {
  const int count = GetItemCount();

  // The last committed item index may be equal to -1 if a session is saved
  // during restoration. In that case use GetItemCount() - 1.
  int last_committed_item_index = GetLastCommittedItemIndex();
  if (last_committed_item_index == -1) {
    last_committed_item_index = count - 1;
  }

  DCHECK_LT(last_committed_item_index, count);

  // As some items may be skipped during serialization (e.g. because their
  // URL is too large, or they were marked "to skip during serialisation")
  // collect the items that will be serialized in a first pass.
  std::vector<const NavigationItemImpl*> items;
  items.reserve(static_cast<size_t>(count));

  const int original_last_committed_item_index = last_committed_item_index;
  for (int index = 0; index < count; ++index) {
    const NavigationItemImpl* item =
        GetNavigationItemImplAtIndex(static_cast<size_t>(index));

    if (item->ShouldSkipSerialization()) {
      // Update the index of the last committed item if necessary when
      // skipping an item.
      if (index <= original_last_committed_item_index) {
        --last_committed_item_index;
      }
      continue;
    }

    items.push_back(item);
  }

  // Ensure that the last committed item index is still in range.
  const int items_size = static_cast<int>(items.size());
  DCHECK_LE(items_size, count);
  DCHECK_LT(last_committed_item_index, items_size);

  // Limit the number of navigation item that are serialised to prevent
  // the storage required to grow indefinitely.
  int offset_int = 0;
  int length_int = 0;
  last_committed_item_index = wk_navigation_util::GetSafeItemRange(
      last_committed_item_index, items_size, &offset_int, &length_int);

  DCHECK_GE(offset_int, 0);
  DCHECK_GE(length_int, 0);
  DCHECK_LT(last_committed_item_index, length_int);

  const size_t offset = static_cast<size_t>(offset_int);
  const size_t length = static_cast<size_t>(length_int);

  DCHECK_LE(offset, items.size());
  DCHECK_LE(length + offset, items.size());

  storage.set_last_committed_item_index(last_committed_item_index);
  for (const auto* item : base::make_span(items.begin() + offset, length)) {
    item->SerializeToProto(*storage.add_items());
  }
}

void NavigationManagerImpl::SetNativeSessionFetcher(
    SessionDataBlobFetcher native_session_fetcher) {
  CHECK(session_data_blob_fetchers_.empty());
  if (base::FeatureList::IsEnabled(features::kForceSynthesizedRestoreSession)) {
    // If the use of synthesized native WKWebView session is force, then drop
    // the `native_session_fetcher`. This simulate a missing native session
    // and force the synthese of a native WKWebView session.
    return;
  }

  AppendSessionDataBlobFetcher(std::move(native_session_fetcher),
                               SessionDataBlobSource::kSessionCache);
}

void NavigationManagerImpl::OnNavigationItemCommitted() {
  NavigationItem* item = GetLastCommittedItem();
  DCHECK(item);
  delegate_->OnNavigationItemCommitted(item);

  if (native_restore_in_progress_) {
    native_restore_in_progress_ = false;
  }
  restored_visible_item_.reset();
}

void NavigationManagerImpl::DetachFromWebView() {
  web_view_cache_.DetachFromWebView();
}

void NavigationManagerImpl::AddPendingItem(
    const GURL& url,
    const web::Referrer& referrer,
    ui::PageTransition navigation_type,
    NavigationInitiationType initiation_type,
    bool is_post_navigation,
    bool is_error_navigation,
    HttpsUpgradeType https_upgrade_type) {
  DiscardNonCommittedItems();

  pending_item_index_ = -1;
  NavigationItem* last_committed_item = GetLastCommittedItem();
  pending_item_ = CreateNavigationItemWithRewriters(
      url, referrer, navigation_type, initiation_type, https_upgrade_type,
      last_committed_item ? last_committed_item->GetURL() : GURL(),
      &transient_url_rewriters_);
  RemoveTransientURLRewriters();

  if (!next_pending_url_should_skip_serialization_.is_empty() &&
      url == next_pending_url_should_skip_serialization_) {
    pending_item_->SetShouldSkipSerialization(true);
  }
  next_pending_url_should_skip_serialization_ = GURL();

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

  BOOL isCurrentURLSameAsPending =
      current_item_url == pending_item_->GetURL() &&
      current_item_url == net::GURLWithNSURL(proxy.URL);

  bool is_form_post =
      is_post_navigation &&
      (navigation_type & ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT);
  if (proxy.backForwardList.currentItem && isCurrentURLSameAsPending &&
      !is_form_post && !is_error_navigation) {
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
      // TODO(crbug.com/41414501): Use GURL::IsAboutBlank() instead.
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

void NavigationManagerImpl::RestoreNativeSession() {
  RecordSessionRestorationHasFetchers(!session_data_blob_fetchers_.empty());

  // Try to load session data blob from each registered source in order,
  // stopping at the first that is successfully loaded.
  bool success = false;
  for (auto& [fetcher, source] : session_data_blob_fetchers_) {
    NSData* data = std::move(fetcher).Run();

    bool fetcher_has_data = data.length != 0;
    RecordSessionRestorationFetcherHasDataForSource(source, fetcher_has_data);
    if (fetcher_has_data) {
      success = GetWebState()->SetSessionStateData(data);
      RecordSessionRestorationResultForSource(success, source);
      if (success) {
        break;
      }
    }
  }

  if (!success) {
    return;
  }

  // Native restore worked, abort unsafe restore.
  DiscardNonCommittedItems();
  last_committed_item_index_ = web_view_cache_.GetCurrentItemIndex();
  if (restored_visible_item_ &&
      restored_visible_item_->GetUserAgentType() != UserAgentType::NONE) {
    NavigationItem* last_committed_item = GetLastCommittedItem();
    if (last_committed_item) {
      last_committed_item->SetUserAgentType(
          restored_visible_item_->GetUserAgentType());
    }
  }
  restored_visible_item_.reset();
  FinalizeSessionRestore();
}

void NavigationManagerImpl::RemoveTransientURLRewriters() {
  transient_url_rewriters_.clear();
}

void NavigationManagerImpl::UpdatePendingItemUrl(const GURL& url) const {
  // If there is no pending item, navigation is probably happening within the
  // back forward history. Don't modify the item list.
  NavigationItemImpl* pending_item = GetPendingItemImpl();
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
  NavigationItemImpl* pending_item = GetPendingItemImpl();
  if (pending_item)
    return pending_item;

  return GetLastCommittedItemImpl();
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
    // Button actions are executed asynchronously, so it is possible for the
    // client to call this with an invalid index if the user quickly taps the
    // back or foward button mulitple times. See crbug.com/1407244.
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
  if (restored_visible_item_) {
    return restored_visible_item_.get();
  }

  // Only return pending_item_ for new (non-history), user-initiated
  // navigations in order to prevent URL spoof attacks.
  NavigationItemImpl* pending_item = GetPendingItemImpl();
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

  return nullptr;
}

NavigationItem* NavigationManagerImpl::GetLastCommittedItem() const {
  return GetLastCommittedItemImpl();
}

NavigationItemImpl* NavigationManagerImpl::GetLastCommittedItemImpl() const {
  if (empty_window_open_item_) {
    return empty_window_open_item_.get();
  }

  int index = GetLastCommittedItemIndex();
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
    GURL document_url = delegate_->GetCurrentURL();
    if (!last_committed_web_view_item_) {
      last_committed_web_view_item_ = CreateNavigationItemWithRewriters(
          /*url=*/GURL(), Referrer(), ui::PageTransition::PAGE_TRANSITION_LINK,
          NavigationInitiationType::RENDERER_INITIATED, HttpsUpgradeType::kNone,
          /*previous_url=*/GURL(), nullptr /* use default rewriters only */);
      last_committed_web_view_item_->SetUntrusted();
    }
    last_committed_web_view_item_->SetURL(document_url);
    last_committed_web_view_item_->SetVirtualURL(document_url);
    last_committed_web_view_item_->SetTimestamp(
        time_smoother_.GetSmoothedTime(base::Time::Now()));
    return last_committed_web_view_item_.get();
  }
  return last_committed_item;
}

int NavigationManagerImpl::GetLastCommittedItemIndex() const {
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

NavigationItem* NavigationManagerImpl::GetPendingItem() const {
  return GetPendingItemImpl();
}

NavigationItemImpl* NavigationManagerImpl::GetPendingItemImpl() const {
  if (pending_item_index_ == -1) {
    if (!pending_item_) {
      return delegate_->GetPendingItem();
    }
    return pending_item_.get();
  }
  return GetNavigationItemImplAtIndex(pending_item_index_);
}

void NavigationManagerImpl::DiscardNonCommittedItems() {
  pending_item_.reset();
  pending_item_index_ = -1;
}

void NavigationManagerImpl::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  DCHECK(!(params.transition_type & ui::PAGE_TRANSITION_FORWARD_BACK));
  delegate_->ClearDialogs();
  delegate_->RecordPageStateInNavigationItem();

  NavigationInitiationType initiation_type =
      params.is_renderer_initiated
          ? NavigationInitiationType::RENDERER_INITIATED
          : NavigationInitiationType::BROWSER_INITIATED;
  AddPendingItem(params.url, params.referrer, params.transition_type,
                 initiation_type, /*is_post_navigation=*/false,
                 /*is_error_navigation=*/false, params.https_upgrade_type);

  // Mark pending item as created from hash change if necessary. This is needed
  // because window.hashchange message may not arrive on time.
  NavigationItemImpl* pending_item = GetPendingItemImpl();
  if (pending_item) {
    NavigationItem* last_committed_item = GetLastCommittedItem();
    GURL last_committed_url =
        last_committed_item ? last_committed_item->GetVirtualURL() : GURL();
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
      pending_item ? pending_item : GetLastCommittedItemImpl();
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
  } else if (!native_restore_in_progress_) {
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
  return pending_item_index_;
}

bool NavigationManagerImpl::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool NavigationManagerImpl::CanGoForward() const {
  return CanGoToOffset(1);
}

bool NavigationManagerImpl::CanGoToOffset(int offset) const {
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
  if (!GetPendingItem() && !GetLastCommittedItem()) {
    return;
  }

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
    if (GetPendingItem()) {
      reload_item = GetPendingItem();
    } else {
      reload_item = GetLastCommittedItem();
    }
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
  int current_back_forward_item_index = web_view_cache_.GetCurrentItemIndex();
  for (int index = current_back_forward_item_index - 1; index >= 0; index--) {
    items.push_back(GetItemAtIndex(index));
  }

  return items;
}

std::vector<NavigationItem*> NavigationManagerImpl::GetForwardItems() const {
  std::vector<NavigationItem*> items;
  for (int index = web_view_cache_.GetCurrentItemIndex() + 1;
       index < GetItemCount(); index++) {
    items.push_back(GetItemAtIndex(index));
  }
  return items;
}

void NavigationManagerImpl::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
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

  if (!web_view_cache_.IsAttachedToWebView()) {
    web_view_cache_.ResetToAttached();
  }

  for (size_t index = 0; index < items.size(); ++index) {
    RewriteItemURLIfNecessary(items[index].get());
  }

  NSData* synthesized_data = SynthesizedSessionRestore(
      last_committed_item_index, items, browser_state_->IsOffTheRecord());
  if (synthesized_data != nil) {
    AppendSessionDataBlobFetcher(
        base::BindOnce([](NSData* data) { return data; }, synthesized_data),
        SessionDataBlobSource::kSynthesized);
  }

  native_restore_in_progress_ = true;

  // Ordering is important. Cache the visible item of the restored session
  // before starting the new navigation, which may trigger client lookup of
  // visible item. The visible item of the restored session is the last
  // committed item, because a restored session has no pending item.
  if (last_committed_item_index > -1) {
    restored_visible_item_ = std::move(items[last_committed_item_index]);
  }

  std::vector<std::unique_ptr<NavigationItem>> back_items;
  for (int index = 0; index < last_committed_item_index; index++) {
    back_items.push_back(std::move(items[index]));
  }

  std::vector<std::unique_ptr<NavigationItem>> forward_items;
  for (size_t index = last_committed_item_index + 1; index < items.size();
       index++) {
    forward_items.push_back(std::move(items[index]));
  }

  RestoreNativeSession();

  RestoreItemsState(RestoreItemListType::kBackList, std::move(back_items));
  RestoreItemsState(RestoreItemListType::kForwardList,
                    std::move(forward_items));
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

void NavigationManagerImpl::AppendSessionDataBlobFetcher(
    SessionDataBlobFetcher fetcher,
    SessionDataBlobSource source) {
  session_data_blob_fetchers_.push_back(
      std::make_pair(std::move(fetcher), source));
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
    if (is_same_url) {
      cached_item->RestoreStateFromItem(restore_item);
    }
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
  for (int index = GetLastCommittedItemIndex(); index >= 0; index--) {
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

  const GURL& last_committed_url = last_committed_item->GetURL();
  // WKWebView.URL will update immediately when navigating to and from
  // about, file or chrome scheme URLs.
  // Checks `last_committed_url` in advance to reduce the calling to
  // `web_view_cache_.GetVisibleWebViewOriginURL()` for better performance.
  if (last_committed_url.SchemeIs(url::kAboutScheme) ||
      last_committed_url.SchemeIs(url::kFileScheme) ||
      web::GetWebClient()->IsAppSpecificURL(last_committed_url)) {
    return true;
  }

  // Only compare origins, as any mismatch between `web_view_url` and
  // `last_committed_url` with the same origin are safe to return as
  // visible.
  const GURL& web_view_origin_url =
      web_view_cache_.GetVisibleWebViewOriginURL();
  if (web_view_origin_url == last_committed_url.DeprecatedGetOriginAsURL())
    return true;

  // WKWebView.URL will update immediately when navigating to and from
  // about, file or chrome scheme URLs.
  if (web_view_origin_url.SchemeIs(url::kAboutScheme) ||
      web_view_origin_url.SchemeIs(url::kFileScheme) ||
      web::GetWebClient()->IsAppSpecificURL(web_view_origin_url)) {
    return true;
  }

  return false;
}

void NavigationManagerImpl::FinalizeSessionRestore() {
  session_data_blob_fetchers_.clear();
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
    cached_items_.reserve(GetBackForwardListItemCount());
    for (size_t index = 0; index < GetBackForwardListItemCount(); index++) {
      std::unique_ptr<NavigationItemImpl> clone =
          GetNavigationItemImplAtIndex(index, /* create_if_missing = */ true)
              ->Clone();
      cached_items_.push_back(std::move(clone));
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
  if (!IsAttachedToWebView()) {
    return cached_items_.size();
  }

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
    // Retain the url to reduce the number of calls to `proxy.URL` which may be
    // very expensive after being called hundreds of time for one navigation.
    NSURL* url = proxy.URL;
    if (![cached_visible_host_nsstring_ isEqualToString:url.host] ||
        ![cached_visible_scheme_nsstring_ isEqualToString:url.scheme]) {
      cached_visible_origin_url_ =
          net::GURLWithNSURL(url).DeprecatedGetOriginAsURL();
      cached_visible_host_nsstring_ = url.host;
      cached_visible_scheme_nsstring_ = url.scheme;
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
  if (new_item->GetTitle().empty() &&
      GetCurrentItemIndex() == static_cast<int>(index)) {
    // Normally, The WKBackforwardList.title equals to the document.title when
    // the page loads. But it's not always accurate. For WKWebView, The
    // WKBackforwardList.title is empty when the new page is opened via
    // history.pushState(). But the document.title (WKWebView.title) doesn't
    // have to be empty or changed. So when opening a new page via
    // history.pushState(), its title should be the current document.title.
    // Here, When the WKBackforwardList.title is empty for current navigation,
    // the document.title is read as the title of the new item to solve this
    // problem.
    new_item->SetTitle(GetWKWebViewTitle());
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

const std::u16string NavigationManagerImpl::WKWebViewCache::GetWKWebViewTitle()
    const {
  DCHECK(IsAttachedToWebView());

  id<CRWWebViewNavigationProxy> proxy =
      navigation_manager_->delegate_->GetWebViewNavigationProxy();
  NSString* title = proxy.title;
  if (!title) {
    return std::u16string();
  }
  return base::SysNSStringToUTF16(title);
}

}  // namespace web
