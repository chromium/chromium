// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/legacy_navigation_manager_impl.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_item_impl_list.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/web_client.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

LegacyNavigationManagerImpl::LegacyNavigationManagerImpl() = default;

LegacyNavigationManagerImpl::~LegacyNavigationManagerImpl() {
  [session_controller_ setNavigationManager:nullptr];
}

void LegacyNavigationManagerImpl::SetBrowserState(BrowserState* browser_state) {
  NavigationManagerImpl::SetBrowserState(browser_state);
  [session_controller_ setBrowserState:browser_state];
}

void LegacyNavigationManagerImpl::SetSessionController(
    CRWSessionController* session_controller) {
  session_controller_ = session_controller;
  [session_controller_ setNavigationManager:this];
}

void LegacyNavigationManagerImpl::InitializeSession() {
  SetSessionController(
      [[CRWSessionController alloc] initWithBrowserState:browser_state_]);
}

void LegacyNavigationManagerImpl::OnNavigationItemsPruned(
    size_t pruned_item_count) {
  delegate_->OnNavigationItemsPruned(pruned_item_count);
}

void LegacyNavigationManagerImpl::OnNavigationItemCommitted() {
  web::NavigationItem* item = GetLastCommittedItemInCurrentOrRestoredSession();
  DCHECK(item);
  delegate_->OnNavigationItemCommitted(item);
}

void LegacyNavigationManagerImpl::OnNavigationStarted(const GURL& url) {}

CRWSessionController* LegacyNavigationManagerImpl::GetSessionController()
    const {
  return session_controller_;
}

void LegacyNavigationManagerImpl::AddTransientItem(const GURL& url) {
  [session_controller_ addTransientItemWithURL:url];

  // TODO(crbug.com/676129): Transient item is only supposed to be added for
  // pending non-app-specific loads, but pending item can be null because of the
  // bug. The workaround should be removed once the bug is fixed.
  NavigationItem* item = GetPendingItem();
  if (!item)
    item = GetLastCommittedItemWithUserAgentType();
  // |item| may still be nullptr if NTP is the only entry in the session.
  // See https://crbug.com/822908 for details.
  if (item) {
    DCHECK(item->GetUserAgentType() != UserAgentType::NONE);
    GetTransientItem()->SetUserAgentType(item->GetUserAgentType());
  }
}

void LegacyNavigationManagerImpl::AddPendingItem(
    const GURL& url,
    const web::Referrer& referrer,
    ui::PageTransition navigation_type,
    NavigationInitiationType initiation_type,
    UserAgentOverrideOption user_agent_override_option) {
  [session_controller_ addPendingItem:url
                             referrer:referrer
                           transition:navigation_type
                       initiationType:initiation_type
              userAgentOverrideOption:user_agent_override_option];

  if (!GetPendingItemInCurrentOrRestoredSession()) {
    return;
  }

  UpdatePendingItemUserAgentType(user_agent_override_option,
                                 GetLastCommittedItemWithUserAgentType(),
                                 GetPendingItemInCurrentOrRestoredSession());
}

void LegacyNavigationManagerImpl::CommitPendingItem() {
  [session_controller_ commitPendingItem];
}

void LegacyNavigationManagerImpl::CommitPendingItem(
    std::unique_ptr<NavigationItemImpl> item) {
  if (item) {
    [session_controller_ commitPendingItem:std::move(item)];
  } else {
    CommitPendingItem();
  }
}

std::unique_ptr<web::NavigationItemImpl>
LegacyNavigationManagerImpl::ReleasePendingItem() {
  return [session_controller_ releasePendingItem];
}

void LegacyNavigationManagerImpl::SetPendingItem(
    std::unique_ptr<web::NavigationItemImpl> item) {
  [session_controller_ setPendingItem:std::move(item)];
}

BrowserState* LegacyNavigationManagerImpl::GetBrowserState() const {
  return browser_state_;
}

WebState* LegacyNavigationManagerImpl::GetWebState() const {
  return delegate_->GetWebState();
}

NavigationItem* LegacyNavigationManagerImpl::GetVisibleItem() const {
  return [session_controller_ visibleItem];
}

void LegacyNavigationManagerImpl::DiscardNonCommittedItems() {
  [session_controller_ discardNonCommittedItems];
}

int LegacyNavigationManagerImpl::GetItemCount() const {
  return session_controller_ ? [session_controller_ items].size() : 0;
}

NavigationItem* LegacyNavigationManagerImpl::GetItemAtIndex(
    size_t index) const {
  return GetNavigationItemImplAtIndex(index);
}

NavigationItemImpl* LegacyNavigationManagerImpl::GetNavigationItemImplAtIndex(
    size_t index) const {
  return [session_controller_ itemAtIndex:index];
}

int LegacyNavigationManagerImpl::GetIndexOfItem(
    const web::NavigationItem* item) const {
  return [session_controller_ indexOfItem:item];
}

int LegacyNavigationManagerImpl::GetPendingItemIndex() const {
  return [session_controller_ pendingItemIndex];
}

int LegacyNavigationManagerImpl::
    GetLastCommittedItemIndexInCurrentOrRestoredSession() const {
  if (GetItemCount() == 0)
    return -1;
  return [session_controller_ lastCommittedItemIndex];
}

bool LegacyNavigationManagerImpl::RemoveItemAtIndex(int index) {
  if (index == GetLastCommittedItemIndex() || index == GetPendingItemIndex())
    return false;

  if (index < 0 || index >= GetItemCount())
    return false;

  [session_controller_ removeItemAtIndex:index];
  return true;
}

bool LegacyNavigationManagerImpl::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool LegacyNavigationManagerImpl::CanGoForward() const {
  return CanGoToOffset(1);
}

bool LegacyNavigationManagerImpl::CanGoToOffset(int offset) const {
  int index = GetIndexForOffset(offset);
  return 0 <= index && index < GetItemCount();
}

void LegacyNavigationManagerImpl::GoBack() {
  GoToIndex(GetIndexForOffset(-1));
}

void LegacyNavigationManagerImpl::GoForward() {
  GoToIndex(GetIndexForOffset(1));
}

NavigationItemList LegacyNavigationManagerImpl::GetBackwardItems() const {
  return [session_controller_ backwardItems];
}

NavigationItemList LegacyNavigationManagerImpl::GetForwardItems() const {
  return [session_controller_ forwardItems];
}

void LegacyNavigationManagerImpl::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
  WillRestore(items.size());
  for (size_t index = 0; index < items.size(); ++index) {
    RewriteItemURLIfNecessary(items[index].get());
  }

  DCHECK(GetItemCount() == 0 && !GetPendingItem());
  DCHECK_LT(last_committed_item_index, static_cast<int>(items.size()));
  DCHECK(items.empty() || last_committed_item_index >= 0);
  SetSessionController([[CRWSessionController alloc]
        initWithBrowserState:browser_state_
             navigationItems:std::move(items)
      lastCommittedItemIndex:last_committed_item_index]);
}

void LegacyNavigationManagerImpl::CopyStateFromAndPrune(
    const NavigationManager* manager) {
  DCHECK(manager);
  CRWSessionController* other_session =
      static_cast<const NavigationManagerImpl*>(manager)
          ->GetSessionController();
  [session_controller_ copyStateFromSessionControllerAndPrune:other_session];
}

bool LegacyNavigationManagerImpl::CanPruneAllButLastCommittedItem() const {
  return [session_controller_ canPruneAllButLastCommittedItem];
}

int LegacyNavigationManagerImpl::GetIndexForOffset(int offset) const {
  int result = [session_controller_ pendingItemIndex] == -1
                   ? GetLastCommittedItemIndex()
                   : static_cast<int>([session_controller_ pendingItemIndex]);

  if (offset < 0) {
    if (GetTransientItem() && [session_controller_ pendingItemIndex] == -1) {
      // Going back from transient item that added to the end navigation stack
      // is a matter of discarding it as there is no need to move navigation
      // index back.
      offset++;
    }

    while (offset < 0 && result > 0) {
      // To stop the user getting 'stuck' on redirecting pages they weren't
      // even aware existed, it is necessary to pass over pages that would
      // immediately result in a redirect (the item *before* the redirected
      // page).
      while (result > 0) {
        const NavigationItem* item = GetItemAtIndex(result);
        if (!ui::PageTransitionIsRedirect(item->GetTransitionType()))
          break;
        --result;
      }
      --result;
      ++offset;
    }
    // Result may be out of bounds, so stop trying to skip redirect items and
    // simply add the remainder.
    result += offset;
    if (result > GetItemCount() /* overflow */)
      result = INT_MIN;
  } else if (offset > 0) {
    while (offset > 0 && result < GetItemCount()) {
      ++result;
      --offset;
      // As with going back, skip over redirects.
      while (result + 1 < GetItemCount()) {
        const NavigationItem* item = GetItemAtIndex(result + 1);
        if (!ui::PageTransitionIsRedirect(item->GetTransitionType()))
          break;
        ++result;
      }
    }
    // Result may be out of bounds, so stop trying to skip redirect items and
    // simply add the remainder.
    result += offset;
    if (result < 0 /* overflow */)
      result = INT_MAX;
  }

  return result;
}

NavigationItemImpl*
LegacyNavigationManagerImpl::GetLastCommittedItemInCurrentOrRestoredSession()
    const {
  return [session_controller_ lastCommittedItem];
}

NavigationItemImpl*
LegacyNavigationManagerImpl::GetPendingItemInCurrentOrRestoredSession() const {
  return [session_controller_ pendingItem];
}

NavigationItemImpl* LegacyNavigationManagerImpl::GetTransientItemImpl() const {
  return [session_controller_ transientItem];
}

void LegacyNavigationManagerImpl::FinishGoToIndex(int index,
                                                  NavigationInitiationType type,
                                                  bool has_user_gesture) {
  const ScopedNavigationItemImplList& items = [session_controller_ items];
  NavigationItem* to_item = items[index].get();
  NavigationItem* previous_item = GetLastCommittedItem();

  to_item->SetTransitionType(ui::PageTransitionFromInt(
      to_item->GetTransitionType() | ui::PAGE_TRANSITION_FORWARD_BACK));

  bool same_document_navigation =
      [session_controller_ isSameDocumentNavigationBetweenItem:previous_item
                                                       andItem:to_item];
  if (same_document_navigation) {
    [session_controller_ goToItemAtIndex:index discardNonCommittedItems:YES];
    delegate_->OnGoToIndexSameDocumentNavigation(type, has_user_gesture);
  } else {
    [session_controller_ discardNonCommittedItems];
    [session_controller_ setPendingItemIndex:index];
    delegate_->LoadCurrentItem(type);
  }
}

int LegacyNavigationManagerImpl::GetPreviousItemIndex() const {
  return base::checked_cast<int>([session_controller_ previousItemIndex]);
}

void LegacyNavigationManagerImpl::SetPreviousItemIndex(
    int previous_item_index) {
  [session_controller_ setPreviousItemIndex:previous_item_index];
}

void LegacyNavigationManagerImpl::AddPushStateItemIfNecessary(
    const GURL& url,
    NSString* state_object,
    ui::PageTransition transition) {
  [session_controller_ pushNewItemWithURL:url
                              stateObject:state_object
                               transition:transition];
}

bool LegacyNavigationManagerImpl::IsRestoreSessionInProgress() const {
  return false;  // Session restoration is synchronous.
}

bool LegacyNavigationManagerImpl::ShouldBlockUrlDuringRestore(const GURL& url) {
  return false;
}

void LegacyNavigationManagerImpl::SetPendingItemIndex(int index) {
  session_controller_.pendingItemIndex = index;
}

}  // namespace web
