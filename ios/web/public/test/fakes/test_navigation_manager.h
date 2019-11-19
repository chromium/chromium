// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_NAVIGATION_MANAGER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_NAVIGATION_MANAGER_H_

#include "base/callback.h"
#include "ios/web/public/deprecated/navigation_item_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ui/base/page_transition_types.h"

namespace web {

// A minimal implementation of web::NavigationManager that raises NOTREACHED()
// on most calls.
class TestNavigationManager : public NavigationManager {
 public:
  TestNavigationManager();
  ~TestNavigationManager() override;
  BrowserState* GetBrowserState() const override;
  WebState* GetWebState() const override;
  NavigationItem* GetVisibleItem() const override;
  NavigationItem* GetLastCommittedItem() const override;
  NavigationItem* GetPendingItem() const override;
  NavigationItem* GetTransientItem() const override;
  void DiscardNonCommittedItems() override;
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override;
  void LoadIfNecessary() override;
  void AddTransientURLRewriter(
      BrowserURLRewriter::URLRewriter rewriter) override;
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
  void GoToIndex(int index) override;
  void Reload(ReloadType reload_type, bool check_for_reposts) override;
  void ReloadWithUserAgentType(UserAgentType user_agent_type) override;
  NavigationItemList GetBackwardItems() const override;
  NavigationItemList GetForwardItems() const override;
  void Restore(int last_committed_item_index,
               std::vector<std::unique_ptr<NavigationItem>> items) override;
  bool IsRestoreSessionInProgress() const override;
  void AddRestoreCompletionCallback(base::OnceClosure callback) override;
  void CopyStateFromAndPrune(const NavigationManager* source) override;
  bool CanPruneAllButLastCommittedItem() const override;

  // Setters for test data.
  // Sets a value for last committed item that will be returned by
  // GetLastCommittedItem().
  void SetLastCommittedItem(NavigationItem* item);

  // Sets a value for pending item that will be returned by GetPendingItem().
  void SetPendingItem(NavigationItem* item);

  // Sets a value for the index that will be returned by GetPendingItemIndex().
  void SetPendingItemIndex(int index);

  // Sets a value for visible item that will be returned by GetVisibleItem().
  void SetVisibleItem(NavigationItem* item);

  // Adds an item to items_. Affects the return values for, GetItemCount(),
  // GetItemAtIndex(), and GetCurrentItemIndex().
  void AddItem(const GURL& url, ui::PageTransition transition);

  // Sets the index to be returned by GetLastCommittedItemIndex(). |index| must
  // be either -1 or between 0 and GetItemCount()-1, inclusively.
  void SetLastCommittedItemIndex(const int index);

  // Sets the index to be returned by GetBrowserState().
  void SetBrowserState(web::BrowserState* browser_state);

  // Returns whether LoadURLWithParams has been called.
  bool LoadURLWithParamsWasCalled();

  // Returns whether LoadIfNecessary has been called.
  bool LoadIfNecessaryWasCalled();

 private:
  // A list of items constructed by calling AddItem().
  web::ScopedNavigationItemList items_;
  int items_index_;
  // Individual backing instance variables for Set* test set up methods.
  NavigationItem* pending_item_;
  int pending_item_index_;
  NavigationItem* last_committed_item_;
  NavigationItem* visible_item_;
  web::BrowserState* browser_state_;
  bool load_url_with_params_was_called_;
  bool load_if_necessary_was_called_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_NAVIGATION_MANAGER_H_
