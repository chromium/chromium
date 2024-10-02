// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"

#import "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/saved_tab_group_tab.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/saved_tab_groups/public/types.h"
#import "components/saved_tab_groups/public/utils.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

using tab_groups::SavedTabGroupTab;

namespace tab_groups {
namespace utils {

LocalTabGroupInfo GetLocalTabGroupInfo(
    BrowserList* browser_list,
    const tab_groups::SavedTabGroup& saved_tab_group) {
  if (!saved_tab_group.local_group_id().has_value() ||
      saved_tab_group.saved_tabs().size() == 0) {
    return LocalTabGroupInfo{};
  }

  return GetLocalTabGroupInfo(browser_list,
                              saved_tab_group.local_group_id().value());
}

LocalTabGroupInfo GetLocalTabGroupInfo(
    BrowserList* browser_list,
    const tab_groups::LocalTabGroupID& tab_group_id) {
  for (Browser* browser :
       browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (const TabGroup* group : web_state_list->GetGroups()) {
      if (group->tab_group_id() == tab_group_id) {
        return LocalTabGroupInfo{
            .tab_group = group,
            .web_state_list = web_state_list,
            .browser = browser,
        };
      }
    }
  }
  return LocalTabGroupInfo{};
}

LocalTabInfo GetLocalTabInfo(BrowserList* browser_list,
                             web::WebStateID web_state_identifier) {
  for (Browser* browser :
       browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    WebStateList* web_state_list = browser->GetWebStateList();
    LocalTabInfo info = GetLocalTabInfo(web_state_list, web_state_identifier);
    if (info.tab_group) {
      return info;
    }
  }
  return LocalTabInfo{};
}

LocalTabInfo GetLocalTabInfo(WebStateList* web_state_list,
                             web::WebStateID web_state_identifier) {
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    if (web_state_identifier == web_state->GetUniqueIdentifier()) {
      const TabGroup* group = web_state_list->GetGroupOfWebStateAt(index);
      int index_in_group = group ? index - group->range().range_begin()
                                 : WebStateList::kInvalidIndex;
      return LocalTabInfo{
          .tab_group = group,
          .index_in_group = index_in_group,
      };
    }
  }
  return LocalTabInfo{};
}

void CloseTabGroupLocally(const TabGroup* tab_group,
                          WebStateList* web_state_list,
                          TabGroupSyncService* sync_service) {
  // `sync_service` is nullptr in incognito.
  if (sync_service && sync_service->GetGroup(tab_group->tab_group_id())) {
    sync_service->RemoveLocalTabGroupMapping(tab_group->tab_group_id(),
                                             ClosingSource::kClosedByUser);
  }
  CloseAllWebStatesInGroup(*web_state_list, tab_group,
                           WebStateList::CLOSE_USER_ACTION);
}

// Moves tab group across browsers.
void MoveTabGroupAcrossBrowsers(const TabGroup* source_tab_group,
                                Browser* source_browser,
                                Browser* destination_browser,
                                int destination_tab_group_index) {
  // Get and lock `source_web_state_list` and `destination_web_state_list`.
  WebStateList* source_web_state_list = source_browser->GetWebStateList();
  WebStateList* destination_web_state_list =
      destination_browser->GetWebStateList();
  auto source_lock = source_web_state_list->StartBatchOperation();
  auto destination_lock = destination_web_state_list->StartBatchOperation();

  int source_web_state_start_index = source_tab_group->range().range_begin();
  int tab_count = source_tab_group->range().count();
  CHECK(tab_count > 0);

  // Create the `TabGroupVisualData` for the new group.
  const tab_groups::TabGroupVisualData destination_visual_data(
      source_tab_group->visual_data());

  // Duplicate the `TabGroupId` for the new group.
  const tab_groups::TabGroupId destination_local_id =
      source_tab_group->tab_group_id();

  // Move tabs to the new browser.
  int moved_tab_count = 0;
  size_t source_group_count =
      source_browser->GetWebStateList()->GetGroups().size();
  for (int destination_index_offset = 0; destination_index_offset < tab_count;
       destination_index_offset++) {
    if (!source_web_state_list->ContainsIndex(source_web_state_start_index)) {
      // `source_web_state_start_index` should have been a valid index at all
      // times during the loop.
      base::debug::DumpWithoutCrashing();
      break;
    }
    if (source_web_state_list->GetGroupOfWebStateAt(
            source_web_state_start_index) != source_tab_group) {
      // The group of the tab to move does not match.
      base::debug::DumpWithoutCrashing();
      break;
    }
    MoveTabFromBrowserToBrowser(
        source_browser, source_web_state_start_index, destination_browser,
        destination_tab_group_index + destination_index_offset);
    moved_tab_count++;
  }

  // Create the new group.
  const TabGroup* destination_tab_group =
      destination_browser->GetWebStateList()->CreateGroup(
          TabGroupRange(destination_tab_group_index, moved_tab_count).AsSet(),
          destination_visual_data, destination_local_id);
  CHECK(destination_browser->GetWebStateList()->ContainsGroup(
      destination_tab_group));
  // Check that the source browser has one less group.
  CHECK_EQ(source_group_count,
           source_browser->GetWebStateList()->GetGroups().size() + 1,
           base::NotFatalUntil::M128);
}

void MoveTabGroupToBrowser(const TabGroup* source_tab_group,
                           Browser* destination_browser,
                           int destination_tab_group_index) {
  ProfileIOS* profile = destination_browser->GetProfile();
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  const BrowserList::BrowserType browser_types =
      profile->IsOffTheRecord() ? BrowserList::BrowserType::kIncognito
                                : BrowserList::BrowserType::kRegular;
  std::set<Browser*> browsers = browser_list->BrowsersOfType(browser_types);

  // Retrieve the `source_browser`.
  Browser* source_browser;
  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    if (web_state_list->ContainsGroup(source_tab_group)) {
      source_browser = browser;
      break;
    }
  }

  if (!source_browser) {
    DUMP_WILL_BE_NOTREACHED()
        << "Either the 'source_tab_group' is incorrect, or the user is "
           "attempting to move a tab group across profiles (incognito <-> "
           "regular)";
    return;
  }

  if (source_browser == destination_browser) {
    // This is a reorder operation within the same WebStateList.
    destination_browser->GetWebStateList()->MoveGroup(
        source_tab_group, destination_tab_group_index);
    return;
  }

  if (!IsTabGroupSyncEnabled()) {
    MoveTabGroupAcrossBrowsers(source_tab_group, source_browser,
                               destination_browser,
                               destination_tab_group_index);
    return;
  }

  // Lock tab group sync service observer.
  CHECK_EQ(source_browser->GetProfile(), destination_browser->GetProfile());
  auto* sync_service = tab_groups::TabGroupSyncServiceFactory::GetForProfile(
      source_browser->GetProfile());
  auto lock = sync_service->CreateScopedLocalObserverPauser();

  MoveTabGroupAcrossBrowsers(source_tab_group, source_browser,
                             destination_browser, destination_tab_group_index);
}

bool ShouldUpdateHistory(web::NavigationContext* navigation_context) {
  web::WebState* web_state = navigation_context->GetWebState();

  if (!web_state || web_state->GetBrowserState()->IsOffTheRecord()) {
    return false;
  }

  // Failed navigations and 404 errors are not saved to history.
  if (navigation_context->GetError()) {
    return false;
  }

  if (navigation_context->GetResponseHeaders() &&
      navigation_context->GetResponseHeaders()->response_code() == 404) {
    return false;
  }

  if (!navigation_context->HasCommitted() ||
      !web_state->GetNavigationManager()->GetLastCommittedItem()) {
    // Navigation was replaced or aborted.
    return false;
  }

  web::NavigationItem* last_committed_item =
      web_state->GetNavigationManager()->GetLastCommittedItem();

  // Back/forward navigations do not update history.
  const ui::PageTransition transition =
      last_committed_item->GetTransitionType();
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    return false;
  }

  return true;
}

bool IsSaveableNavigation(web::NavigationContext* navigation_context) {
  // Please keep this in sync with TabGroupSyncUtils::IsSaveableNavigation as a
  // best effort.

  ui::PageTransition page_transition = navigation_context->GetPageTransition();

  // TODO(crbug.com/359726089): Check all methods other than GET.
  if (navigation_context->IsPost()) {
    return false;
  }
  if (!ui::IsValidPageTransitionType(page_transition)) {
    return false;
  }
  if (ui::PageTransitionIsRedirect(page_transition)) {
    return false;
  }

  if (!ui::PageTransitionIsMainFrame(page_transition)) {
    return false;
  }

  if (!navigation_context->HasCommitted()) {
    return false;
  }

  if (!ShouldUpdateHistory(navigation_context)) {
    return false;
  }

  // For renderer initiated navigation, in most cases these navigations will be
  // auto triggered on restoration. So there is no need to save them.
  if (navigation_context->IsRendererInitiated() &&
      !navigation_context->HasUserGesture()) {
    return false;
  }

  return IsURLValidForSavedTabGroups(navigation_context->GetUrl());
}

}  // namespace utils
}  // namespace tab_groups
