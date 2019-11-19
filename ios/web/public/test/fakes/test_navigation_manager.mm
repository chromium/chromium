// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

TestNavigationManager::TestNavigationManager()
    : items_index_(-1),
      pending_item_(nullptr),
      pending_item_index_(-1),
      last_committed_item_(nullptr),
      visible_item_(nullptr),
      browser_state_(nullptr),
      load_url_with_params_was_called_(false),
      load_if_necessary_was_called_(false) {}

TestNavigationManager::~TestNavigationManager() {}

BrowserState* TestNavigationManager::GetBrowserState() const {
  return browser_state_;
}

WebState* TestNavigationManager::GetWebState() const {
  NOTREACHED();
  return nullptr;
}

NavigationItem* TestNavigationManager::GetVisibleItem() const {
  return visible_item_;
}

void TestNavigationManager::SetVisibleItem(NavigationItem* item) {
  visible_item_ = item;
}

NavigationItem* TestNavigationManager::GetLastCommittedItem() const {
  return last_committed_item_;
}

void TestNavigationManager::SetLastCommittedItem(NavigationItem* item) {
  last_committed_item_ = item;
}

NavigationItem* TestNavigationManager::GetPendingItem() const {
  return pending_item_;
}

void TestNavigationManager::SetPendingItem(NavigationItem* item) {
  pending_item_ = item;
}

web::NavigationItem* TestNavigationManager::GetTransientItem() const {
  NOTREACHED();
  return nullptr;
}

void TestNavigationManager::DiscardNonCommittedItems() {
  NOTREACHED();
}

void TestNavigationManager::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  load_url_with_params_was_called_ = true;
}

void TestNavigationManager::LoadIfNecessary() {
  load_if_necessary_was_called_ = true;
}

void TestNavigationManager::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {
  NOTREACHED();
}

int TestNavigationManager::GetItemCount() const {
  return items_.size();
}

web::NavigationItem* TestNavigationManager::GetItemAtIndex(size_t index) const {
  return items_[index].get();
}

int TestNavigationManager::GetIndexOfItem(
    const web::NavigationItem* item) const {
  for (size_t index = 0; index < items_.size(); ++index) {
    if (items_[index].get() == item)
      return index;
  }
  return -1;
}

void TestNavigationManager::SetLastCommittedItemIndex(const int index) {
  DCHECK(index == -1 || index >= 0 && index < GetItemCount());
  items_index_ = index;
}

int TestNavigationManager::GetLastCommittedItemIndex() const {
  return items_index_;
}

int TestNavigationManager::GetPendingItemIndex() const {
  return pending_item_index_;
}

void TestNavigationManager::SetPendingItemIndex(int index) {
  pending_item_index_ = index;
}

bool TestNavigationManager::RemoveItemAtIndex(int index) {
  if (index < 0 || index >= GetItemCount())
    return false;
  DCHECK(items_index_ != index);
  items_.erase(items_.begin() + index);
  if (items_index_ > index)
    --items_index_;
  return true;
}

bool TestNavigationManager::CanGoBack() const {
  return items_index_ > 0;
}

bool TestNavigationManager::CanGoForward() const {
  return items_index_ < GetItemCount() - 1;
}

bool TestNavigationManager::CanGoToOffset(int offset) const {
  NOTREACHED();
  return false;
}

void TestNavigationManager::GoBack() {
  items_index_--;
}

void TestNavigationManager::GoForward() {
  items_index_++;
}

void TestNavigationManager::GoToIndex(int index) {
  NOTREACHED();
}

void TestNavigationManager::Reload(ReloadType reload_type,
                                   bool check_for_repost) {
  NOTREACHED();
}

void TestNavigationManager::ReloadWithUserAgentType(
    UserAgentType user_agent_type) {
  NOTREACHED();
}

NavigationItemList TestNavigationManager::GetBackwardItems() const {
  return NavigationItemList();
}

NavigationItemList TestNavigationManager::GetForwardItems() const {
  return NavigationItemList();
}

void TestNavigationManager::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
  NOTREACHED();
}

bool TestNavigationManager::IsRestoreSessionInProgress() const {
  return false;
}

void TestNavigationManager::AddRestoreCompletionCallback(
    base::OnceClosure callback) {
  NOTREACHED();
}

void TestNavigationManager::CopyStateFromAndPrune(
    const NavigationManager* source) {
  NOTREACHED();
}

bool TestNavigationManager::CanPruneAllButLastCommittedItem() const {
  NOTREACHED();
  return false;
}

// Adds a new navigation item of |transition| type at the end of this
// navigation manager.
void TestNavigationManager::AddItem(const GURL& url,
                                    ui::PageTransition transition) {
  items_.push_back(web::NavigationItem::Create());
  items_.back()->SetTransitionType(transition);
  items_.back()->SetURL(url);
  SetLastCommittedItemIndex(GetItemCount() - 1);
}

void TestNavigationManager::SetBrowserState(web::BrowserState* browser_state) {
  browser_state_ = browser_state;
}

bool TestNavigationManager::LoadURLWithParamsWasCalled() {
  return load_url_with_params_was_called_;
}

bool TestNavigationManager::LoadIfNecessaryWasCalled() {
  return load_if_necessary_was_called_;
}

}  // namespace web
