// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/navigation/navigation_item.h"

namespace web {

FakeNavigationManager::FakeNavigationManager() = default;

FakeNavigationManager::~FakeNavigationManager() = default;

BrowserState* FakeNavigationManager::GetBrowserState() const {
  return browser_state_;
}

WebState* FakeNavigationManager::GetWebState() const {
  NOTREACHED_IN_MIGRATION();
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

void FakeNavigationManager::DiscardNonCommittedItems() {
  SetPendingItem(nullptr);
}

void FakeNavigationManager::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  load_url_with_params_was_called_ = true;
  load_URL_params_ = params;
}

void FakeNavigationManager::LoadIfNecessary() {
  load_if_necessary_was_called_ = true;
}

void FakeNavigationManager::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
  return false;
}

void FakeNavigationManager::GoBack() {
  items_index_--;
}

void FakeNavigationManager::GoForward() {
  items_index_++;
}

void FakeNavigationManager::GoToIndex(int index) {
  NOTREACHED_IN_MIGRATION();
}

void FakeNavigationManager::Reload(ReloadType reload_type,
                                   bool check_for_repost) {
  reload_was_called_ = true;
}

void FakeNavigationManager::ReloadWithUserAgentType(
    UserAgentType user_agent_type) {
  if (user_agent_type == web::UserAgentType::MOBILE) {
    request_mobile_site_was_called_ = true;
  } else if (user_agent_type == web::UserAgentType::DESKTOP) {
    request_desktop_site_was_called_ = true;
  }
}

std::vector<NavigationItem*> FakeNavigationManager::GetBackwardItems() const {
  std::vector<NavigationItem*> back_items;
  for (int i = static_cast<int>(items_index_ - 1); i >= 0; --i) {
    back_items.push_back(items_[i].get());
  }
  return back_items;
}

std::vector<NavigationItem*> FakeNavigationManager::GetForwardItems() const {
  std::vector<NavigationItem*> forward_items;
  for (unsigned int i = items_index_ + 1; i < items_.size(); ++i) {
    forward_items.push_back(items_[i].get());
  }
  return forward_items;
}

void FakeNavigationManager::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
  NOTREACHED_IN_MIGRATION();
}

// Adds a new navigation item of `transition` type at the end of this
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

std::optional<NavigationManager::WebLoadParams>
FakeNavigationManager::GetLastLoadURLWithParams() {
  return load_URL_params_;
}

bool FakeNavigationManager::LoadIfNecessaryWasCalled() {
  return load_if_necessary_was_called_;
}

bool FakeNavigationManager::ReloadWasCalled() {
  return reload_was_called_;
}

bool FakeNavigationManager::RequestDesktopSiteWasCalled() {
  return request_desktop_site_was_called_;
}

bool FakeNavigationManager::RequestMobileSiteWasCalled() {
  return request_mobile_site_was_called_;
}

}  // namespace web
