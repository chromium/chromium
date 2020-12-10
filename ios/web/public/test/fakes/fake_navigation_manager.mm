// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FakeNavigationManager::FakeNavigationManager()
    : items_index_(-1),
      pending_item_(nullptr),
      pending_item_index_(-1),
      last_committed_item_(nullptr),
      visible_item_(nullptr),
      browser_state_(nullptr),
      load_url_with_params_was_called_(false),
      load_if_necessary_was_called_(false),
      reload_was_called_(false) {}

FakeNavigationManager::~FakeNavigationManager() {}

BrowserState* FakeNavigationManager::GetBrowserState() const {
  return browser_state_;
}

WebState* FakeNavigationManager::GetWebState() const {
  NOTREACHED();
  return nullptr;
}

NavigationItem* FakeNavigationManager::GetVisibleItem() const {
  return visible_item_;
}

void FakeNavigationManager::SetVisibleItem(NavigationItem* item) {
  visible_item_ = item;
}

NavigationItem* FakeNavigationManager::GetLastCommittedItem() const {
  return last_committed_item_;
}

void FakeNavigationManager::SetLastCommittedItem(NavigationItem* item) {
  last_committed_item_ = item;
}

NavigationItem* FakeNavigationManager::GetPendingItem() const {
  return pending_item_;
}

void FakeNavigationManager::SetPendingItem(NavigationItem* item) {
  pending_item_ = item;
}

web::NavigationItem* FakeNavigationManager::GetTransientItem() const {
  NOTREACHED();
  return nullptr;
}

void FakeNavigationManager::DiscardNonCommittedItems() {
  SetPendingItem(nullptr);
}

void FakeNavigationManager::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  load_url_with_params_was_called_ = true;
}

void FakeNavigationManager::LoadIfNecessary() {
  load_if_necessary_was_called_ = true;
}

void FakeNavigationManager::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {
  NOTREACHED();
}

int FakeNavigationManager::GetItemCount() const {
  return items_.size();
}

web::NavigationItem* FakeNavigationManager::GetItemAtIndex(size_t index) const {
  return items_[index].get();
}

int FakeNavigationManager::GetIndexOfItem(
    const web::NavigationItem* item) const {
  for (size_t index = 0; index < items_.size(); ++index) {
    if (items_[index].get() == item)
      return index;
  }
  return -1;
}

void FakeNavigationManager::SetLastCommittedItemIndex(const int index) {
  DCHECK(index == -1 || index >= 0 && index < GetItemCount());
  items_index_ = index;
}

int FakeNavigationManager::GetLastCommittedItemIndex() const {
  return items_index_;
}

int FakeNavigationManager::GetPendingItemIndex() const {
  return pending_item_index_;
}

void FakeNavigationManager::SetPendingItemIndex(int index) {
  pending_item_index_ = index;
}

bool FakeNavigationManager::CanGoBack() const {
  return items_index_ > 0;
}

bool FakeNavigationManager::CanGoForward() const {
  return items_index_ < GetItemCount() - 1;
}

bool FakeNavigationManager::CanGoToOffset(int offset) const {
  NOTREACHED();
  return false;
}

void FakeNavigationManager::GoBack() {
  items_index_--;
}

void FakeNavigationManager::GoForward() {
  items_index_++;
}

void FakeNavigationManager::GoToIndex(int index) {
  NOTREACHED();
}

void FakeNavigationManager::Reload(ReloadType reload_type,
                                   bool check_for_repost) {
  reload_was_called_ = true;
}

void FakeNavigationManager::ReloadWithUserAgentType(
    UserAgentType user_agent_type) {
  NOTREACHED();
}

NavigationItemList FakeNavigationManager::GetBackwardItems() const {
  return NavigationItemList();
}

NavigationItemList FakeNavigationManager::GetForwardItems() const {
  return NavigationItemList();
}

void FakeNavigationManager::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
  NOTREACHED();
}

bool FakeNavigationManager::IsRestoreSessionInProgress() const {
  return false;
}

void FakeNavigationManager::AddRestoreCompletionCallback(
    base::OnceClosure callback) {
  NOTREACHED();
}

// Adds a new navigation item of |transition| type at the end of this
// navigation manager.
void FakeNavigationManager::AddItem(const GURL& url,
                                    ui::PageTransition transition) {
  items_.push_back(web::NavigationItem::Create());
  items_.back()->SetTransitionType(transition);
  items_.back()->SetURL(url);
  SetLastCommittedItemIndex(GetItemCount() - 1);
}

void FakeNavigationManager::SetBrowserState(web::BrowserState* browser_state) {
  browser_state_ = browser_state;
}

bool FakeNavigationManager::LoadURLWithParamsWasCalled() {
  return load_url_with_params_was_called_;
}

bool FakeNavigationManager::LoadIfNecessaryWasCalled() {
  return load_if_necessary_was_called_;
}

bool FakeNavigationManager::ReloadWasCalled() {
  return reload_was_called_;
}

}  // namespace web
