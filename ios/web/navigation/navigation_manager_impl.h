// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/deprecated/navigation_item_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class CRWSessionController;

namespace web {
class BrowserState;
class NavigationItem;
class NavigationManagerDelegate;
class SessionStorageBuilder;

// Name of UMA histogram to log the number of items Navigation Manager was
// requested to restore. 100 is logged when the number of navigation items is
// greater than 100. This is just a requested count and actual number of
// restored items can be smaller.
extern const char kRestoreNavigationItemCount[];

// Defines the ways how a pending navigation can be initiated.
enum class NavigationInitiationType {
  // Navigation initiation type is only valid for pending navigations, use NONE
  // if a navigation is already committed.
  NONE = 0,

  // Navigation was initiated by the browser by calling NavigationManager
  // methods. Examples of methods which cause browser-initiated navigations
  // include:
  //  * NavigationManager::Reload()
  //  * NavigationManager::GoBack()
  //  * NavigationManager::GoForward()
  BROWSER_INITIATED,

  // Navigation was initiated by renderer. Examples of renderer-initiated
  // navigations include:
  //  * <a> link click
  //  * changing window.location.href
  //  * redirect via the <meta http-equiv="refresh"> tag
  //  * using window.history.pushState
  RENDERER_INITIATED,
};

// Implementation of NavigationManager.
// Generally mirrors upstream's NavigationController.
class NavigationManagerImpl : public NavigationManager {
 public:
  NavigationManagerImpl();
  ~NavigationManagerImpl() override;

  // Returns the most recent Committed Item that is not the result of a client
  // or server-side redirect from the given Navigation Manager. Returns nullptr
  // if there's an error condition on the input |nav_manager|, such as nullptr
  // or no non-redirect items.
  static NavigationItem* GetLastCommittedNonRedirectedItem(
      const NavigationManager* nav_manager);

  // Setters for NavigationManagerDelegate and BrowserState.
  virtual void SetDelegate(NavigationManagerDelegate* delegate);
  virtual void SetBrowserState(BrowserState* browser_state);

  // Sets the CRWSessionController that backs this object.
  // Keeps a strong reference to |session_controller|.
  // This method should only be called when deserializing |session_controller|
  // and joining it with its NavigationManager. Other cases should call
  // InitializeSession() or Restore().
  // TODO(stuartmorgan): Also move deserialization of CRWSessionControllers
  // under the control of this class, and move the bulk of CRWSessionController
  // logic into it.
  virtual void SetSessionController(
      CRWSessionController* session_controller) = 0;

  // Initializes a new session history.
  virtual void InitializeSession() = 0;

  // Helper functions for notifying WebStateObservers of changes.
  // TODO(stuartmorgan): Make these private once the logic triggering them moves
  // into this layer.
  virtual void OnNavigationItemsPruned(size_t pruned_item_count) = 0;
  virtual void OnNavigationItemCommitted() = 0;

  // Called when a navigation has started.
  virtual void OnNavigationStarted(const GURL& url) = 0;

  // Prepares for the deletion of WKWebView such as caching necessary data.
  virtual void DetachFromWebView();

  // Temporary accessors and content/ class pass-throughs.
  // TODO(stuartmorgan): Re-evaluate this list once the refactorings have
  // settled down.
  virtual CRWSessionController* GetSessionController() const = 0;

  // Adds a transient item with the given URL. A transient item will be
  // discarded on any navigation.
  virtual void AddTransientItem(const GURL& url) = 0;

  // Adds a new item with the given url, referrer, navigation type, initiation
  // type and user agent override option, making it the pending item. If pending
  // item is the same as the current item, this does nothing. |referrer| may be
  // nil if there isn't one. The item starts out as pending, and will be lost
  // unless |-commitPendingItem| is called.
  virtual void AddPendingItem(
      const GURL& url,
      const web::Referrer& referrer,
      ui::PageTransition navigation_type,
      NavigationInitiationType initiation_type,
      UserAgentOverrideOption user_agent_override_option) = 0;

  // Commits the pending item, if any.
  // TODO(crbug.com/936933): Remove this method.
  virtual void CommitPendingItem() = 0;

  // Commits given pending |item| stored outside of navigation manager
  // (normally in NavigationContext). It is possible to have additional pending
  // items owned by navigation manager and/or outside of navigation manager.
  virtual void CommitPendingItem(std::unique_ptr<NavigationItemImpl> item) = 0;

  // Removes pending item, so it can be stored in NavigationContext.
  // Pending item is stored in this object when NavigationContext object does
  // not yet exist (e.g. when navigation was just requested, or when navigation
  // has aborted).
  virtual std::unique_ptr<NavigationItemImpl> ReleasePendingItem() = 0;

  // Allows transferring pending item from NavigationContext to this object.
  // Pending item can be moved from NavigationContext to this object when
  // navigation is aborted, but pending item should be retained.
  virtual void SetPendingItem(
      std::unique_ptr<web::NavigationItemImpl> item) = 0;

  // Returns the navigation index that differs from the current item (or pending
  // item if it exists) by the specified |offset|, skipping redirect navigation
  // items. The index returned is not guaranteed to be valid.
  // TODO(crbug.com/661316): Make this method private once navigation code is
  // moved from CRWWebController to NavigationManagerImpl.
  virtual int GetIndexForOffset(int offset) const = 0;

  // Returns the index of the previous item. Only used by SessionStorageBuilder.
  virtual int GetPreviousItemIndex() const = 0;

  // Sets the index of the previous item. Only used by SessionStorageBuilder.
  virtual void SetPreviousItemIndex(int previous_item_index) = 0;

  // Updates navigation history (if applicable) after pushState.
  // TODO(crbug.com/783382): This is a legacy method to maintain backward
  // compatibility for PageLoad stat. Remove this method once PageLoad no longer
  // depend on WebStateObserver::DidStartLoading.
  virtual void AddPushStateItemIfNecessary(const GURL& url,
                                           NSString* state_object,
                                           ui::PageTransition transition) = 0;

  // Sets the index of the pending navigation item. -1 means no navigation or a
  // new navigation.
  virtual void SetPendingItemIndex(int index) = 0;

  // Applies the workaround for crbug.com/887497.
  virtual void ApplyWKWebViewForwardHistoryClobberWorkaround();

  // Set ShouldSkipSerialization to true for the next pending item, provided it
  // matches |url|.  Applies the workaround for crbug.com/997182
  virtual void SetWKWebViewNextPendingUrlNotSerializable(const GURL& url);

  // Returns true if specific URL is blocked from session restore.
  virtual bool ShouldBlockUrlDuringRestore(const GURL& url) = 0;

  // Resets the transient url rewriter list.
  void RemoveTransientURLRewriters();

  // Creates a NavigationItem using the given properties. Calling this method
  // resets the transient URLRewriters cached in this instance.
  // TODO(crbug.com/738020): This method is only used by CRWSessionController.
  // Remove it after switching to WKBasedNavigationManagerImpl.
  std::unique_ptr<NavigationItemImpl> CreateNavigationItem(
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition transition,
      NavigationInitiationType initiation_type);

  // Updates the URL of the yet to be committed pending item. Useful for page
  // redirects. Does nothing if there is no pending item.
  void UpdatePendingItemUrl(const GURL& url) const;

  // The current NavigationItem. During a pending navigation, returns the
  // NavigationItem for that navigation. If a transient NavigationItem exists,
  // this NavigationItem will be returned.
  // TODO(crbug.com/661316): Make this private once all navigation code is moved
  // out of CRWWebController.
  NavigationItemImpl* GetCurrentItemImpl() const;

  // Returns the last committed NavigationItem, which may be null if there
  // are no committed entries or session restoration is in-progress.
  NavigationItemImpl* GetLastCommittedItemImpl() const;

  // Updates the pending or last committed navigation item after replaceState.
  // TODO(crbug.com/783382): This is a legacy method to maintain backward
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
  NavigationItem* GetLastCommittedItem() const final;
  int GetLastCommittedItemIndex() const final;
  NavigationItem* GetPendingItem() const final;
  NavigationItem* GetTransientItem() const final;
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override;
  void AddTransientURLRewriter(BrowserURLRewriter::URLRewriter rewriter) final;
  void GoToIndex(int index) final;
  void Reload(ReloadType reload_type, bool check_for_reposts) final;
  void ReloadWithUserAgentType(UserAgentType user_agent_type) final;
  void LoadIfNecessary() override;
  void AddRestoreCompletionCallback(base::OnceClosure callback) override;

  // Implementation for corresponding NavigationManager getters.
  virtual NavigationItemImpl* GetPendingItemInCurrentOrRestoredSession()
      const = 0;
  virtual NavigationItemImpl* GetTransientItemImpl() const = 0;
  // Unlike GetLastCommittedItem(), this method does not return null during
  // session restoration (and returns last known committed item instead).
  virtual NavigationItemImpl* GetLastCommittedItemInCurrentOrRestoredSession()
      const = 0;
  // Unlike GetLastCommittedItemIndex(), this method does not return -1 during
  // session restoration (and returns last known committed item index instead).
  virtual int GetLastCommittedItemIndexInCurrentOrRestoredSession() const = 0;

  // Identical to GetItemAtIndex() but returns the underlying NavigationItemImpl
  // instead of the public NavigationItem interface.
  virtual NavigationItemImpl* GetNavigationItemImplAtIndex(
      size_t index) const = 0;

 protected:
  // The SessionStorageBuilder functions require access to private variables of
  // NavigationManagerImpl.
  friend SessionStorageBuilder;

  // TODO(crbug.com/738020): Remove legacy code and merge
  // WKBasedNavigationManager into this class after the navigation experiment.

  // Applies the user agent override to |pending_item|, or inherits the user
  // agent of |inherit_from| if |user_agent_override_option| is INHERIT.
  static void UpdatePendingItemUserAgentType(
      UserAgentOverrideOption override_option,
      const NavigationItem* inherit_from,
      NavigationItem* pending_item);

  // Must be called by subclasses before restoring |item_count| navigation
  // items.
  void WillRestore(size_t item_count);

  // Some app-specific URLs need to be rewritten to about: scheme.
  void RewriteItemURLIfNecessary(NavigationItem* item) const;

  // Creates a NavigationItem using the given properties, where |previous_url|
  // is the URL of the navigation just prior to the current one. If
  // |url_rewriters| is not nullptr, apply them before applying the permanent
  // URL rewriters from BrowserState.
  // TODO(crbug.com/738020): Make this private when WKBasedNavigationManagerImpl
  // is merged into this class.
  std::unique_ptr<NavigationItemImpl> CreateNavigationItemWithRewriters(
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition transition,
      NavigationInitiationType initiation_type,
      const GURL& previous_url,
      const std::vector<BrowserURLRewriter::URLRewriter>* url_rewriters) const;

  // Returns the most recent NavigationItem with an URL that generates an HTTP
  // request.
  NavigationItem* GetLastCommittedItemWithUserAgentType() const;

  // Subclass specific implementation to update session state.
  virtual void FinishGoToIndex(int index,
                               NavigationInitiationType type,
                               bool has_user_gesture) = 0;
  virtual void FinishReload();
  virtual void FinishLoadURLWithParams(
      NavigationInitiationType initiation_type);

  // Returns true if the subclass uses placeholder URLs and this is such a URL.
  virtual bool IsPlaceholderUrl(const GURL& url) const;

  // The primary delegate for this manager.
  NavigationManagerDelegate* delegate_;

  // The BrowserState that is associated with this instance.
  BrowserState* browser_state_;

  // List of transient url rewriters added by |AddTransientURLRewriter()|.
  std::vector<BrowserURLRewriter::URLRewriter> transient_url_rewriters_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
