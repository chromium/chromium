// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/navigation/content_navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ContentNavigationManager::ContentNavigationManager(BrowserState* browser_state)
    : browser_state_(browser_state) {}

ContentNavigationManager::~ContentNavigationManager() {}

BrowserState* ContentNavigationManager::GetBrowserState() const {
  return browser_state_;
}

WebState* ContentNavigationManager::GetWebState() const {
  return nullptr;
}

NavigationItem* ContentNavigationManager::GetVisibleItem() const {
  return nullptr;
}

NavigationItem* ContentNavigationManager::GetLastCommittedItem() const {
  return nullptr;
}

NavigationItem* ContentNavigationManager::GetPendingItem() const {
  return nullptr;
}

void ContentNavigationManager::DiscardNonCommittedItems() {}

void ContentNavigationManager::LoadURLWithParams(
    const NavigationManager::WebLoadParams&) {}

void ContentNavigationManager::LoadIfNecessary() {}

void ContentNavigationManager::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {}

int ContentNavigationManager::GetItemCount() const {
  return 0;
}

NavigationItem* ContentNavigationManager::GetItemAtIndex(size_t index) const {
  return nullptr;
}

int ContentNavigationManager::GetIndexOfItem(const NavigationItem* item) const {
  return 0;
}

int ContentNavigationManager::GetPendingItemIndex() const {
  return 0;
}

int ContentNavigationManager::GetLastCommittedItemIndex() const {
  return 0;
}

bool ContentNavigationManager::CanGoBack() const {
  return false;
}

bool ContentNavigationManager::CanGoForward() const {
  return false;
}

bool ContentNavigationManager::CanGoToOffset(int offset) const {
  return false;
}

void ContentNavigationManager::GoBack() {}

void ContentNavigationManager::GoForward() {}

void ContentNavigationManager::GoToIndex(int index) {}

void ContentNavigationManager::Reload(ReloadType reload_type,
                                      bool check_for_reposts) {}

void ContentNavigationManager::ReloadWithUserAgentType(
    UserAgentType user_agent_type) {}

std::vector<NavigationItem*> ContentNavigationManager::GetBackwardItems()
    const {
  return std::vector<NavigationItem*>();
}

std::vector<NavigationItem*> ContentNavigationManager::GetForwardItems() const {
  return std::vector<NavigationItem*>();
}

void ContentNavigationManager::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {}

bool ContentNavigationManager::IsRestoreSessionInProgress() const {
  return false;
}

void ContentNavigationManager::AddRestoreCompletionCallback(
    base::OnceClosure callback) {}

}  // namespace web
