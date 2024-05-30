// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"

#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

OrderControllerSourceFromWebStateList::OrderControllerSourceFromWebStateList(
    const WebStateList& web_state_list)
    : web_state_list_(web_state_list) {}

int OrderControllerSourceFromWebStateList::GetCount() const {
  return web_state_list_->count();
}

int OrderControllerSourceFromWebStateList::GetPinnedCount() const {
  return web_state_list_->pinned_tabs_count();
}

int OrderControllerSourceFromWebStateList::GetOpenerOfItemAt(int index) const {
  DCHECK(web_state_list_->ContainsIndex(index));
  const WebStateOpener opener = web_state_list_->GetOpenerOfWebStateAt(index);
  if (!opener.opener) {
    return WebStateList::kInvalidIndex;
  }

  return web_state_list_->GetIndexOfWebState(opener.opener);
}

bool OrderControllerSourceFromWebStateList::IsOpenerOfItemAt(
    int index,
    int opener_index,
    bool check_navigation_index) const {
  const web::WebState* web_state = web_state_list_->GetWebStateAt(opener_index);
  const WebStateOpener opener = web_state_list_->GetOpenerOfWebStateAt(index);
  if (opener.opener != web_state) {
    return false;
  }

  if (!check_navigation_index) {
    return true;
  }

  // We can't check the last committed item index on an "unrealized" WebState.
  // Assume it is equal to the last item in the navigation list (in the worse
  // case we would select the "wrong" insertion index).
  if (!web_state->IsRealized()) {
    return opener.navigation_index == web_state->GetNavigationItemCount();
  }

  const int navigation_index =
      web_state->GetNavigationManager()->GetLastCommittedItemIndex();
  return opener.navigation_index == navigation_index;
}

TabGroupRange OrderControllerSourceFromWebStateList::GetGroupRangeOfItemAt(
    int index) const {
  const TabGroup* group = web_state_list_->GetGroupOfWebStateAt(index);
  if (group) {
    return TabGroupRange(group->range());
  }
  return TabGroupRange::InvalidRange();
}

std::set<int> OrderControllerSourceFromWebStateList::GetCollapsedGroupIndexes()
    const {
  std::set<const TabGroup*> groups = web_state_list_->GetGroups();
  std::set<int> collapsed_indexes;

  for (const auto& group : groups) {
    if (group->visual_data().is_collapsed()) {
      std::set<int> group_indexes = TabGroupRange(group->range()).AsSet();
      collapsed_indexes.insert(group_indexes.begin(), group_indexes.end());
    }
  }
  return collapsed_indexes;
}
