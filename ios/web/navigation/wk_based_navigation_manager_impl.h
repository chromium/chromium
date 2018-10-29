// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_WK_BASED_NAVIGATION_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_WK_BASED_NAVIGATION_MANAGER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/time_smoother.h"
#include "ios/web/public/reload_type.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class WKBackForwardListItem;

namespace base {
class ElapsedTimer;
}

namespace web {
class BrowserState;
class NavigationItem;
struct Referrer;
class SessionStorageBuilder;

// Name of UMA histogram to log the time spent on asynchronous session
// restoration.
extern const char kRestoreNavigationTime[];

// WKBackForwardList based implementation of NavigationManagerImpl.
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
class WKBasedNavigationManagerImpl : public NavigationManagerImpl {
 public:
  WKBasedNavigationManagerImpl();
  ~WKBasedNavigationManagerImpl() override;

  // NavigationManagerImpl:
  void SetSessionController(CRWSessionController* session_controller) override;
  void InitializeSession() override;
  void OnNavigationItemsPruned(size_t pruned_item_count) override;
  void OnNavigationItemChanged() override;
  void OnNavigationItemCommitted() override;
  void DetachFromWebView() override;
  CRWSessionController* GetSessionController() const override;
  void AddTransientItem(const GURL& url) override;
  void AddPendingItem(
      const GURL& url,
      const web::Referrer& referrer,
      ui::PageTransition navigation_type,
      NavigationInitiationType initiation_type,
      UserAgentOverrideOption user_agent_override_option) override;
  void CommitPendingItem() override;
  int GetIndexForOffset(int offset) const override;
  // Returns the previous navigation item in the main frame.
  int GetPreviousItemIndex() const override;
  void SetPreviousItemIndex(int previous_item_index) override;
  void AddPushStateItemIfNecessary(const GURL& url,
                                   NSString* state_object,
                                   ui::PageTransition transition) override;

  // NavigationManager:
  BrowserState* GetBrowserState() const override;
  WebState* GetWebState() const override;
  NavigationItem* GetVisibleItem() const override;
  void DiscardNonCommittedItems() override;
  int GetItemCount() const override;
  NavigationItem* GetItemAtIndex(size_t index) const override;
  int GetIndexOfItem(const NavigationItem* item) const override;
  int GetPendingItemIndex() const override;
  int GetLastCommittedItemIndex() const override;
  bool RemoveItemAtIndex(int index) override;
  bool CanGoBack() const override;
  bool CanGoForward() const override;
  bool CanGoToOffset(int offset) const override;
  void GoBack() override;
  void GoForward() override;
  NavigationItemList GetBackwardItems() const override;
  NavigationItemList GetForwardItems() const override;
  void CopyStateFromAndPrune(const NavigationManager* source) override;
  bool CanPruneAllButLastCommittedItem() const override;
  void Restore(int last_committed_item_index,
               std::vector<std::unique_ptr<NavigationItem>> items) override;
  void LoadIfNecessary() override;

 private:
  // The SessionStorageBuilder functions require access to private variables of
  // NavigationManagerImpl.
  friend SessionStorageBuilder;

  // Access shim for NavigationItems associated with the WKBackForwardList. It
  // is responsible for caching NavigationItems when the navigation manager
  // detaches from its web view.
  class WKWebViewCache {
   public:
    explicit WKWebViewCache(WKBasedNavigationManagerImpl* navigation_manager);
    ~WKWebViewCache();

    // Returns true if the navigation manager is attached to a WKWebView.
    bool IsAttachedToWebView() const;

    // Caches NavigationItems from the WKWebView in |this| and changes state to
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

    // Returns the absolute index of WKBackForwardList's |currentItem| or -1 if
    // |currentItem| is nil. If navigation manager is in detached mode, returns
    // the cached value of this property captured at the last call of
    // DetachFromWebView().
    int GetCurrentItemIndex() const;

    // Returns the NavigationItem associated with the WKBackForwardListItem at
    // |index|. If |create_if_missing| is true and the WKBackForwardListItem
    // does not have an associated NavigationItem, creates a new one and returns
    // it to the caller.
    NavigationItemImpl* GetNavigationItemImplAtIndex(
        size_t index,
        bool create_if_missing) const;

    // Returns the WKBackForwardListItem at |index|. Must only be called when
    // IsAttachedToWebView() is true.
    WKBackForwardListItem* GetWKItemAtIndex(size_t index) const;

   private:
    WKBasedNavigationManagerImpl* navigation_manager_;
    bool attached_to_web_view_;

    std::vector<std::unique_ptr<NavigationItemImpl>> cached_items_;
    int cached_current_item_index_;

    DISALLOW_COPY_AND_ASSIGN(WKWebViewCache);
  };

  // NavigationManagerImpl:
  NavigationItemImpl* GetNavigationItemImplAtIndex(size_t index) const override;
  NavigationItemImpl* GetLastCommittedItemImpl() const override;
  // Returns the pending navigation item in the main frame.
  NavigationItemImpl* GetPendingItemImpl() const override;
  NavigationItemImpl* GetTransientItemImpl() const override;
  void FinishGoToIndex(int index,
                       NavigationInitiationType initiation_type,
                       bool has_user_gesture) override;
  void FinishReload() override;
  void FinishLoadURLWithParams() override;
  bool IsPlaceholderUrl(const GURL& url) const override;

  // The pending main frame navigation item. This is nullptr if there is no
  // pending item or if the pending item is a back-forward navigation, in which
  // case the NavigationItemImpl is stored on the WKBackForwardListItem.
  std::unique_ptr<NavigationItemImpl> pending_item_;

  // -1 if pending_item_ represents a new navigation or there is no pending
  // navigation. Otherwise, this is the index of the pending_item in the
  // back-forward list.
  int pending_item_index_;

  // Index of the previous navigation item in the main frame. If there is none,
  // this field will have value -1.
  int previous_item_index_;

  // Index of the last committed item in the main frame. If there is none, this
  // field will equal to -1.
  int last_committed_item_index_;

  // The NavigationItem that corresponds to the empty window open navigation. It
  // has to be stored separately because it has no WKBackForwardListItem. It is
  // not null if when CommitPendingItem() is last called, the WKBackForwardList
  // is empty but not nil. Any subsequent call to CommitPendingItem() will reset
  // this field to null.
  std::unique_ptr<NavigationItemImpl> empty_window_open_item_;

  // The transient item in main frame.
  std::unique_ptr<NavigationItemImpl> transient_item_;

  // Time smoother for navigation item timestamps. See comment in
  // navigation_controller_impl.h.
  // NOTE: This is mutable because GetNavigationItemImplAtIndex() needs to call
  // TimeSmoother::GetSmoothedTime() with a const 'this'. Since NavigationItems
  // have to be lazily created on read, this is the only workaround.
  mutable TimeSmoother time_smoother_;

  WKWebViewCache web_view_cache_;

  // Whether this navigation manager is in the process of restoring session
  // history into WKWebView. It is set in Restore() and unset in the first
  // OnNavigationItemCommitted() callback.
  bool is_restore_session_in_progress_ = false;

  // Non null during the session restoration. Created when session restoration
  // is started and reset when the restoration is finished. Used to log UMA
  // histogram that measures session restoration time.
  std::unique_ptr<base::ElapsedTimer> restoration_timer_;

  // The active navigation entry in the restored session. GetVisibleItem()
  // returns this item when |is_restore_session_in_progress_| is true so that
  // clients of this navigation manager gets sane values for visible title and
  // URL.
  std::unique_ptr<NavigationItem> restored_visible_item_;

  DISALLOW_COPY_AND_ASSIGN(WKBasedNavigationManagerImpl);
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WK_BASED_NAVIGATION_MANAGER_IMPL_H_
