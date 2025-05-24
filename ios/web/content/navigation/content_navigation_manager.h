// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_NAVIGATION_CONTENT_NAVIGATION_MANAGER_H_
#define IOS_WEB_CONTENT_NAVIGATION_CONTENT_NAVIGATION_MANAGER_H_

#import "base/memory/raw_ref.h"
#import "build/blink_buildflags.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace content {
class NavigationController;
}  // namespace content

namespace web {

class ContentWebState;

// ContentNavigationManager is a NavigationManager implementation that
// is built on top of //content's NavigationController.
class ContentNavigationManager : public NavigationManager {
 public:
  ContentNavigationManager(ContentWebState* web_state,
                           BrowserState* browser_state,
                           content::NavigationController& controller);
  ~ContentNavigationManager() override;
  BrowserState* GetBrowserState() const override;
  WebState* GetWebState() const override;
  NavigationItem* GetVisibleItem() const override;
  NavigationItem* GetLastCommittedItem() const override;
  NavigationItem* GetPendingItem() const override;
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
  bool CanGoBack() const override;
  bool CanGoForward() const override;
  bool CanGoToOffset(int offset) const override;
  void GoBack() override;
  void GoForward() override;
  void GoToIndex(int index) override;
  void Reload(ReloadType reload_type, bool check_for_reposts) override;
  void ReloadWithUserAgentType(UserAgentType user_agent_type) override;
  std::vector<NavigationItem*> GetBackwardItems() const override;
  std::vector<NavigationItem*> GetForwardItems() const override;
  void Restore(int last_committed_item_index,
               std::vector<std::unique_ptr<NavigationItem>> items) override;

 private:
  raw_ptr<ContentWebState> web_state_;
  raw_ptr<BrowserState> browser_state_;
  raw_ref<content::NavigationController> controller_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_NAVIGATION_CONTENT_NAVIGATION_MANAGER_H_
