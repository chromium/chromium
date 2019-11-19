// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_LEGACY_NAVIGATION_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_LEGACY_NAVIGATION_MANAGER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/deprecated/navigation_item_list.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class CRWSessionController;

namespace web {
class BrowserState;
class NavigationItem;
struct Referrer;
class SessionStorageBuilder;

// Implementation of NavigationManagerImpl.
class LegacyNavigationManagerImpl : public NavigationManagerImpl {
 public:
  LegacyNavigationManagerImpl();
  ~LegacyNavigationManagerImpl() override;

  // NavigationManagerImpl:
  void SetBrowserState(BrowserState* browser_state) override;
  void SetSessionController(CRWSessionController* session_controller) override;
  void InitializeSession() override;
  void OnNavigationItemsPruned(size_t pruned_item_count) override;
  void OnNavigationStarted(const GURL& url) override;
  void OnNavigationItemCommitted() override;
  CRWSessionController* GetSessionController() const override;
  void AddTransientItem(const GURL& url) override;
  void AddPendingItem(
      const GURL& url,
      const web::Referrer& referrer,
      ui::PageTransition navigation_type,
      NavigationInitiationType initiation_type,
      UserAgentOverrideOption user_agent_override_option) override;
  void CommitPendingItem() override;
  void CommitPendingItem(std::unique_ptr<NavigationItemImpl> item) override;
  std::unique_ptr<web::NavigationItemImpl> ReleasePendingItem() override;
  void SetPendingItem(std::unique_ptr<web::NavigationItemImpl> item) override;
  int GetIndexForOffset(int offset) const override;
  int GetPreviousItemIndex() const override;
  void SetPreviousItemIndex(int previous_item_index) override;
  void AddPushStateItemIfNecessary(const GURL& url,
                                   NSString* state_object,
                                   ui::PageTransition transition) override;
  bool IsRestoreSessionInProgress() const override;
  bool ShouldBlockUrlDuringRestore(const GURL& url) override;
  void SetPendingItemIndex(int index) override;

  // NavigationManager:
  BrowserState* GetBrowserState() const override;
  WebState* GetWebState() const override;
  NavigationItem* GetVisibleItem() const override;
  void DiscardNonCommittedItems() override;
  int GetItemCount() const override;
  NavigationItem* GetItemAtIndex(size_t index) const override;
  int GetIndexOfItem(const NavigationItem* item) const override;
  int GetPendingItemIndex() const override;
  int GetLastCommittedItemIndexInCurrentOrRestoredSession() const override;
  bool RemoveItemAtIndex(int index) override;
  bool CanGoBack() const override;
  bool CanGoForward() const override;
  bool CanGoToOffset(int offset) const override;
  void GoBack() override;
  void GoForward() override;
  NavigationItemList GetBackwardItems() const override;
  NavigationItemList GetForwardItems() const override;
  void Restore(int last_committed_item_index,
               std::vector<std::unique_ptr<NavigationItem>> items) override;
  void CopyStateFromAndPrune(const NavigationManager* source) override;
  bool CanPruneAllButLastCommittedItem() const override;

 private:
  // The SessionStorageBuilder functions require access to private variables of
  // NavigationManagerImpl.
  friend SessionStorageBuilder;

  // NavigationManagerImpl:
  NavigationItemImpl* GetNavigationItemImplAtIndex(size_t index) const override;
  NavigationItemImpl* GetLastCommittedItemInCurrentOrRestoredSession()
      const override;
  NavigationItemImpl* GetPendingItemInCurrentOrRestoredSession() const override;
  NavigationItemImpl* GetTransientItemImpl() const override;
  void FinishGoToIndex(int index,
                       NavigationInitiationType type,
                       bool has_user_gesture) override;

  // CRWSessionController that backs this instance.
  // TODO(stuartmorgan): Fold CRWSessionController into this class.
  CRWSessionController* session_controller_;

  DISALLOW_COPY_AND_ASSIGN(LegacyNavigationManagerImpl);
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_LEGACY_NAVIGATION_MANAGER_IMPL_H_
