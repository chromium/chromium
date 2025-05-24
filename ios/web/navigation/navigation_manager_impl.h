// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#import "base/memory/raw_ptr.h"
#include "ios/web/navigation/navigation_initiation_type.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/navigation/synthesized_session_restore.h"
#include "ios/web/navigation/time_smoother.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class WKBackForwardListItem;

namespace base {
class ElapsedTimer;
}

namespace web {
namespace proto {
class NavigationStorage;
}  // namespace proto
class BrowserState;
class NavigationItem;
class NavigationManagerDelegate;

// Name of UMA histogram to log the number of items Navigation Manager was
// requested to restore. 100 is logged when the number of navigation items is
// greater than 100. This is just a requested count and actual number of
// restored items can be smaller.
extern const char kRestoreNavigationItemCount[];

// WKBackForwardList-based implementation of NavigationManager.
// Generally mirrors upstream's NavigationController.
//
// This class relies on the following WKWebView APIs, defined by the
// CRWWebViewNavigationProxy protocol:
//   @property URL
//   @property backForwardList
//   - goToBackForwardListItem:
//
// This navigation manager uses WKBackForwardList as the ground truth for back-
// forward navigation history. It uses the Associated Objects runtime feature
// to link a NavigationItemImpl object to each WKBackForwardListItem to store
// additional states needed by the embedder.
//
// For all main frame navigations (both UI-initiated and renderer-initiated),
// the NavigationItemImpl objects are created proactively via AddPendingItem and
// CommitPendingItem.
//
// Non-main-frame navigations can only be initiated from the renderer. The
// NavigationItemImpl objects in this case are created lazily in GetItemAtIndex
// because the provisional load and commit events for iframe navigation are not
// visible via the WKNavigationDelegate interface. Consequently, pending item
// and previous item are only tracked for the main frame.
//
// Empty Window Open Navigation edge case:
//
//   If window.open() is called with an empty URL, WKWebView does not seem to
//   create a WKBackForwardListItem for the first about:blank navigation. Any
//   subsequent navigation in this window will replace the about:blank entry.
//   This is consistent with the HTML spec regarding Location-object navigation
//   when the browser context's only Document is about:blank:
//   https://html.spec.whatwg.org/multipage/history.html (Section 7.7.4)
//
//   This navigation manager will still create a pendingNavigationItem for this
//   "empty window open item" and allow CommitPendingItem() to be called on it.
//   All accessors will behave identically as if the navigation history has a
//   single normal entry. The only difference is that a subsequent call to
//   CommitPendingItem() will *replace* the empty window open item. From this
//   point onward, it is as if the empty window open item never occurred.
//
// Detach from web view edge case:
//
//   As long as this navigation manager is alive, the navigation manager
//   delegate should not delete its WKWebView. However, legacy use cases exist
//   (e.g. https://crbug/770914). As a workaround, before deleting the
//   WKWebView, the delegate must call
//   NavigationManagerImpl::DetachFromWebView() to cache the current session
//   history. This puts the navigation manager in a detached state. While in
//   this state, all getters are serviced using the cached session history.
//   Mutation methods are not allowed. The navigation manager returns to the
//   attached state when a new navigation starts.
class NavigationManagerImpl final : public NavigationManager {
 public:
  // Callback used to fetch WKWebView session data blob.
  using SessionDataBlobFetcher = base::OnceCallback<NSData*()>;

  // Enumeration representing the source of a WKWebView session data blob.
  enum class SessionDataBlobSource {
    kSessionCache,
    kSynthesized,
  };

  NavigationManagerImpl(BrowserState* browser_state,
                        NavigationManagerDelegate* delegate);
  ~NavigationManagerImpl() final;

  NavigationManagerImpl(const NavigationManagerImpl&) = delete;
  NavigationManagerImpl& operator=(const NavigationManagerImpl&) = delete;

  // Restores state from `storage`.
  void RestoreFromProto(const proto::NavigationStorage& storage);

  // Serializes the NavigationItemImpl into `storage`.
  void SerializeToProto(proto::NavigationStorage& storage) const;

  // Setter for the callback used to fetch the native session data blob from
  // the session cache.
  void SetNativeSessionFetcher(SessionDataBlobFetcher native_session_fetcher);

  // Helper functions for notifying WebStateObservers of changes.
  // TODO(stuartmorgan): Make these private once the logic triggering them moves
  // into this layer.
  void OnNavigationItemCommitted();

  // Prepares for the deletion of WKWebView such as caching necessary data.
  void DetachFromWebView();

  // Adds a new item with the given url, referrer, navigation type, initiation
  // type and user agent override option, making it the pending item. If pending
  // item is the same as the current item, this does nothing. `referrer` may be
  // nil if there isn't one. The item starts out as pending, and will be lost
  // unless `-commitPendingItem` is called.
  // `is_post_navigation` is true if the navigation is using a POST HTTP method.
  // `is_error_navigation` is true if the navigation leads to an internal error
  // page. `https_upgrade_type` indicates the type of the HTTPS upgrade applied
  // on this navigation.
  void AddPendingItem(const GURL& url,
                      const web::Referrer& referrer,
                      ui::PageTransition navigation_type,
                      NavigationInitiationType initiation_type,
                      bool is_post_navigation,
                      bool is_error_navigation,
                      web::HttpsUpgradeType https_upgrade_type);

  // Commits the pending item, if any.
  // TODO(crbug.com/41444193): Remove this method.
  void CommitPendingItem();

  // Commits given pending `item` stored outside of navigation manager
  // (normally in NavigationContext). It is possible to have additional pending
  // items owned by navigation manager and/or outside of navigation manager.
  void CommitPendingItem(std::unique_ptr<NavigationItemImpl> item);

  // Removes pending item, so it can be stored in NavigationContext.
  // Pending item is stored in this object when NavigationContext object does
  // not yet exist (e.g. when navigation was just requested, or when navigation
  // has aborted).
  std::unique_ptr<NavigationItemImpl> ReleasePendingItem();

  // Allows transferring pending item from NavigationContext to this object.
  // Pending item can be moved from NavigationContext to this object when
  // navigation is aborted, but pending item should be retained.
  void SetPendingItem(std::unique_ptr<web::NavigationItemImpl> item);

  // Returns the navigation index that differs from the current item (or pending
  // item if it exists) by the specified `offset`, skipping redirect navigation
  // items. The index returned is not guaranteed to be valid.
  // TODO(crbug.com/41284081): Make this method private once navigation code is
  // moved from CRWWebController to NavigationManagerImpl.
  int GetIndexForOffset(int offset) const;

  // Sets the index of the pending navigation item. -1 means no navigation or a
  // new navigation.
  void SetPendingItemIndex(int index);

  // Set ShouldSkipSerialization to true for the next pending item, provided it
  // matches `url`.  Applies the workaround for crbug.com/997182
  void SetWKWebViewNextPendingUrlNotSerializable(const GURL& url);

  // Restores the session using the native WKWebView API from the sources
  // appended with `AppendSessionDataBlobFetcher`.
  void RestoreNativeSession();

  // Resets the transient url rewriter list.
  void RemoveTransientURLRewriters();

  // Updates the URL of the yet to be committed pending item. Useful for page
  // redirects. Does nothing if there is no pending item.
  void UpdatePendingItemUrl(const GURL& url) const;

  // The current NavigationItem. During a pending navigation, returns the
  // NavigationItem for that navigation.
  // TODO(crbug.com/41284081): Make this private once all navigation code is
  // moved out of CRWWebController.
  NavigationItemImpl* GetCurrentItemImpl() const;

  // Implementation for corresponding NavigationManager getters.
  NavigationItemImpl* GetPendingItemImpl() const;

  // Returns the last committed NavigationItem, which may be null if there
  // are no committed entries or session restoration is in-progress.
  NavigationItemImpl* GetLastCommittedItemImpl() const;

  // Updates the pending or last committed navigation item after replaceState.
  // TODO(crbug.com/41354482): This is a legacy method to maintain backward
  // compatibility for PageLoad stat. Remove this method once PageLoad no longer
  // depend on WebStateObserver::DidStartLoading.
  void UpdateCurrentItemForReplaceState(const GURL& url,
                                        NSString* state_object);

  // Same as GoToIndex(int), but allows renderer-initiated navigations and
  // specifying whether or not the navigation is caused by the user gesture.
  void GoToIndex(int index,
                 NavigationInitiationType initiation_type,
                 bool has_user_gesture);

  // NavigationManager:
  BrowserState* GetBrowserState() const final;
  WebState* GetWebState() const final;
  NavigationItem* GetVisibleItem() const final;
  NavigationItem* GetLastCommittedItem() const final;
  int GetLastCommittedItemIndex() const final;
  NavigationItem* GetPendingItem() const final;
  void DiscardNonCommittedItems() final;
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) final;
  void LoadIfNecessary() final;
  void AddTransientURLRewriter(BrowserURLRewriter::URLRewriter rewriter) final;
  int GetItemCount() const final;
  NavigationItem* GetItemAtIndex(size_t index) const final;
  int GetIndexOfItem(const NavigationItem* item) const final;
  int GetPendingItemIndex() const final;
  bool CanGoBack() const final;
  bool CanGoForward() const final;
  bool CanGoToOffset(int offset) const final;
  void GoBack() final;
  void GoForward() final;
  void GoToIndex(int index) final;
  void Reload(ReloadType reload_type, bool check_for_reposts) final;
  void ReloadWithUserAgentType(UserAgentType user_agent_type) final;
  std::vector<NavigationItem*> GetBackwardItems() const final;
  std::vector<NavigationItem*> GetForwardItems() const final;
  void Restore(int last_committed_item_index,
               std::vector<std::unique_ptr<NavigationItem>> items) final;

  // Identical to GetItemAtIndex() but returns the underlying NavigationItemImpl
  // instead of the public NavigationItem interface.
  NavigationItemImpl* GetNavigationItemImplAtIndex(size_t index) const;

 private:
  // NavigationManagerTest.TestGetVisibleWebViewOriginURLCache needs to access
  // the `web_view_cache_` member field.
  FRIEND_TEST_ALL_PREFIXES(NavigationManagerTest,
                           TestGetVisibleWebViewOriginURLCache);

  // Access shim for NavigationItems associated with the WKBackForwardList. It
  // is responsible for caching NavigationItems when the navigation manager
  // detaches from its web view.
  class WKWebViewCache {
   public:
    explicit WKWebViewCache(NavigationManagerImpl* navigation_manager);

    WKWebViewCache(const WKWebViewCache&) = delete;
    WKWebViewCache& operator=(const WKWebViewCache&) = delete;

    ~WKWebViewCache();

    // Returns true if the navigation manager is attached to a WKWebView.
    bool IsAttachedToWebView() const;

    // Caches NavigationItems from the WKWebView in `this` and changes state to
    // detached.
    void DetachFromWebView();

    // Clears the cached NavigationItems and resets state to attached. Callers
    // that wish to restore the cached navigation items into the new web view
    // must call ReleaseCachedItems() first.
    void ResetToAttached();

    // Returns ownership of the cached NavigationItems. This is convenient for
    // restoring session history when reattaching to a new web view.
    std::vector<std::unique_ptr<NavigationItem>> ReleaseCachedItems();

    // Returns the number of items in the back-forward history.
    size_t GetBackForwardListItemCount() const;

    // Returns the absolute index of WKBackForwardList's `currentItem` or -1 if
    // `currentItem` is nil. If navigation manager is in detached mode, returns
    // the cached value of this property captured at the last call of
    // DetachFromWebView().
    int GetCurrentItemIndex() const;

    // Returns the visible WKWebView origin (host) URL. If navigation manager is
    // detached, returns an empty GURL.
    const GURL& GetVisibleWebViewOriginURL() const;

    // Returns the NavigationItem associated with the WKBackForwardListItem at
    // `index`. If `create_if_missing` is true and the WKBackForwardListItem
    // does not have an associated NavigationItem, creates a new one and returns
    // it to the caller.
    NavigationItemImpl* GetNavigationItemImplAtIndex(
        size_t index,
        bool create_if_missing) const;

    // Returns the WKBackForwardListItem at `index`. Must only be called when
    // IsAttachedToWebView() is true.
    WKBackForwardListItem* GetWKItemAtIndex(size_t index) const;

   private:
    raw_ptr<NavigationManagerImpl> navigation_manager_;
    bool attached_to_web_view_;

    mutable GURL cached_visible_origin_url_;
    mutable NSString* cached_visible_host_nsstring_;
    mutable NSString* cached_visible_scheme_nsstring_;

    std::vector<std::unique_ptr<NavigationItemImpl>> cached_items_;
    int cached_current_item_index_;

    // Returns the WKWebView title. Must only be called when
    // IsAttachedToWebView() is true.
    const std::u16string GetWKWebViewTitle() const;
  };

  // Type of the list passed to restore items.
  enum class RestoreItemListType {
    kBackList,
    kForwardList,
  };

  // Appends a new session blob fetcher with given source.
  void AppendSessionDataBlobFetcher(SessionDataBlobFetcher loader,
                                    SessionDataBlobSource source);

  // Restores the state of the `items_restored` in the navigation items
  // associated with the WKBackForwardList. `back_list` is used to specify if
  // the items passed are the list containing the back list or the forward list.
  void RestoreItemsState(
      RestoreItemListType list_type,
      std::vector<std::unique_ptr<NavigationItem>> items_restored);

  // Must be called by subclasses before restoring `item_count` navigation
  // items.
  void WillRestore(size_t item_count);

  // Some app-specific URLs need to be rewritten to about: scheme.
  void RewriteItemURLIfNecessary(NavigationItem* item) const;

  // Creates a NavigationItem using the given properties, where `previous_url`
  // is the URL of the navigation just prior to the current one. If
  // `url_rewriters` is not nullptr, apply them before applying the permanent
  // URL rewriters from BrowserState.
  std::unique_ptr<NavigationItemImpl> CreateNavigationItemWithRewriters(
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition transition,
      NavigationInitiationType initiation_type,
      HttpsUpgradeType https_upgrade_type,
      const GURL& previous_url,
      const std::vector<BrowserURLRewriter::URLRewriter>* url_rewriters) const;

  // Returns the most recent NavigationItem with an URL that generates an HTTP
  // request.
  NavigationItem* GetLastCommittedItemWithUserAgentType() const;

  // Returns true if `last_committed_item` matches WKWebView.URL when expected.
  // WKWebView is more aggressive than Chromium is in updating the committed
  // URL, and there are cases where, even though WKWebView's URL has updated,
  // Chromium still wants to display last committed.  Normally this is managed
  // by NavigationManagerImpl last committed, but there are short periods
  // during fast navigations where WKWebView.URL has updated and ios/web can't
  // validate what should be shown for the visible item.  More importantly,
  // there are bugs in WkWebView where WKWebView's URL and
  // backForwardList.currentItem can fall out of sync.  In these situations,
  // return false as a safeguard so committed item is always trusted.
  bool CanTrustLastCommittedItem(
      const NavigationItem* last_committed_item) const;

  // Update state to reflect session restore is complete, and call any post
  // restore callbacks.
  void FinalizeSessionRestore();

  // The primary delegate for this manager.
  const raw_ptr<NavigationManagerDelegate> delegate_;

  // The BrowserState that is associated with this instance.
  const raw_ptr<BrowserState> browser_state_;

  // List of transient url rewriters added by `AddTransientURLRewriter()`.
  std::vector<BrowserURLRewriter::URLRewriter> transient_url_rewriters_;

  // The pending main frame navigation item. This is nullptr if there is no
  // pending item or if the pending item is a back-forward navigation, in which
  // case the NavigationItemImpl is stored on the WKBackForwardListItem.
  std::unique_ptr<NavigationItemImpl> pending_item_;

  // -1 if pending_item_ represents a new navigation or there is no pending
  // navigation. Otherwise, this is the index of the pending_item in the
  // back-forward list.
  int pending_item_index_ = -1;

  // Index of the last committed item in the main frame. If there is none, this
  // field will equal to -1.
  int last_committed_item_index_ = -1;

  // The NavigationItem that corresponds to the empty window open navigation. It
  // has to be stored separately because it has no WKBackForwardListItem. It is
  // not null if when CommitPendingItem() is last called, the WKBackForwardList
  // is empty but not nil. Any subsequent call to CommitPendingItem() will reset
  // this field to null.
  std::unique_ptr<NavigationItemImpl> empty_window_open_item_;

  // A placeholder item used when CanTrustLastCommittedItem
  // returns false.  The navigation item returned uses crw_web_controller's
  // documentURL as the URL.
  mutable std::unique_ptr<NavigationItemImpl> last_committed_web_view_item_;

  // Time smoother for navigation item timestamps. See comment in
  // navigation_controller_impl.h.
  // NOTE: This is mutable because GetNavigationItemImplAtIndex() needs to call
  // TimeSmoother::GetSmoothedTime() with a const 'this'. Since NavigationItems
  // have to be lazily created on read, this is the only workaround.
  mutable TimeSmoother time_smoother_;

  WKWebViewCache web_view_cache_{this};

  // Whether this navigation manager is in the process of restoring session
  // history into WKWebView using native restoration.
  bool native_restore_in_progress_ = false;

  // Set to true when delegate_->GoToBackForwardListItem is being called, which
  // is useful to know when comparing the VisibleWebViewURL with the last
  // committed item.
  bool going_to_back_forward_list_item_ = false;

  // Set to an URL when the next created pending item should set
  // ShouldSkipSerialization to true, provided it matches `url`.
  GURL next_pending_url_should_skip_serialization_;

  // Non null during the session restoration. Created when session restoration
  // is started and reset when the restoration is finished. Used to log UMA
  // histogram that measures session restoration time.
  std::unique_ptr<base::ElapsedTimer> restoration_timer_;

  // The active navigation entry in the restored session. GetVisibleItem()
  // returns this item in the window between `is_restore_session_in_progress_`
  // becomes true until the first post-restore navigation is finished, so that
  // clients of this navigation manager gets sane values for visible title and
  // URL.
  std::unique_ptr<NavigationItem> restored_visible_item_;

  // Stores the different WKWebView session data blob loaders. Loaders are
  // tried in the order they are registered, and the native session loading
  // code stops at the first session successfully loaded.
  std::vector<std::pair<SessionDataBlobFetcher, SessionDataBlobSource>>
      session_data_blob_fetchers_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
