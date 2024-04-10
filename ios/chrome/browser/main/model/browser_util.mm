// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/model/browser_util.h"

#import <memory>
#import <ostream>

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/public/web_state.h"

namespace {

// Moves snapshot associated with `snapshot_id` from `source_browser` to
// `destination_browser`'s snapshot storage.
void MoveSnapshot(SnapshotID snapshot_id,
                  Browser* source_browser,
                  Browser* destination_browser) {
  DCHECK(snapshot_id.valid());
  SnapshotStorageWrapper* source_storage =
      SnapshotBrowserAgent::FromBrowser(source_browser)->snapshot_storage();
  SnapshotStorageWrapper* destination_storage =
      SnapshotBrowserAgent::FromBrowser(destination_browser)
          ->snapshot_storage();
  [source_storage migrateImageWithSnapshotID:snapshot_id
                           toSnapshotStorage:destination_storage];
}

}  // namespace

void MoveTabFromBrowserToBrowser(Browser* source_browser,
                                 int source_tab_index,
                                 Browser* destination_browser,
                                 WebStateList::InsertionParams params) {
  if (source_browser == destination_browser) {
    // This is a reorder operation within the same WebStateList.
    destination_browser->GetWebStateList()->MoveWebStateAt(
        source_tab_index, params.desired_index);
    return;
  }
  std::unique_ptr<web::WebState> web_state =
      source_browser->GetWebStateList()->DetachWebStateAt(source_tab_index);
  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state.get());
  MoveSnapshot(snapshot_tab_helper->GetSnapshotID(), source_browser,
               destination_browser);

  // TODO(crbug.com/1264451): Remove this workaround when it will no longer be
  // required to have an active WebState in the WebStateList.
  if (destination_browser->GetWebStateList()->empty()) {
    params.Activate();
  }

  destination_browser->GetWebStateList()->InsertWebState(std::move(web_state),
                                                         params);
}

void MoveTabFromBrowserToBrowser(Browser* source_browser,
                                 int source_tab_index,
                                 Browser* destination_browser,
                                 int destination_tab_index) {
  MoveTabFromBrowserToBrowser(
      source_browser, source_tab_index, destination_browser,
      WebStateList::InsertionParams::AtIndex(destination_tab_index));
}

void MoveTabToBrowser(web::WebStateID tab_id,
                      Browser* destination_browser,
                      WebStateList::InsertionParams params) {
  DCHECK(tab_id.valid());
  ChromeBrowserState* browser_state = destination_browser->GetBrowserState();
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state);
  const std::set<Browser*>& browsers =
      browser_state->IsOffTheRecord() ? browser_list->AllIncognitoBrowsers()
                                      : browser_list->AllRegularBrowsers();

  BrowserAndIndex tab_info = FindBrowserAndIndex(tab_id, browsers);

  if (!tab_info.browser) {
    NOTREACHED() << "Either the tab_id is incorrect, or the user is attempting "
                    "to move a tab across profiles (incognito <-> regular)";
    return;
  }
  MoveTabFromBrowserToBrowser(tab_info.browser, tab_info.tab_index,
                              destination_browser, params);
}

void MoveTabToBrowser(web::WebStateID tab_id,
                      Browser* destination_browser,
                      int destination_tab_index) {
  MoveTabToBrowser(
      tab_id, destination_browser,
      WebStateList::InsertionParams::AtIndex(destination_tab_index));
}

BrowserAndIndex FindBrowserAndIndex(web::WebStateID tab_id,
                                    const std::set<Browser*>& browsers) {
  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (int i = 0; i < web_state_list->count(); ++i) {
      web::WebState* web_state = web_state_list->GetWebStateAt(i);
      web::WebStateID current_tab_id = web_state->GetUniqueIdentifier();
      if (current_tab_id == tab_id) {
        return BrowserAndIndex{
            .browser = browser,
            .tab_index = i,
        };
      }
    }
  }
  return BrowserAndIndex{};
}

void MoveTabGroupToBrowser(const TabGroup* source_tab_group,
                           Browser* destination_browser,
                           int destination_tab_group_index) {
  ChromeBrowserState* browser_state = destination_browser->GetBrowserState();
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state);
  const std::set<Browser*>& browsers =
      browser_state->IsOffTheRecord() ? browser_list->AllIncognitoBrowsers()
                                      : browser_list->AllRegularBrowsers();

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
    NOTREACHED()
        << "Either the 'source_tab_group' is incorrect, or the user is "
           "attempting to move a tab group across profiles (incognito <-> "
           "regular)";
    return;
  }

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

  // Move tabs to the new browser.
  for (int destination_index_offset = 0; destination_index_offset < tab_count;
       destination_index_offset++) {
    CHECK_EQ(source_web_state_list->GetGroupOfWebStateAt(
                 source_web_state_start_index),
             source_tab_group);
    MoveTabFromBrowserToBrowser(
        source_browser, source_web_state_start_index, destination_browser,
        destination_tab_group_index + destination_index_offset);
  }

  // Create the new group.
  const TabGroup* destination_tab_group =
      destination_browser->GetWebStateList()->CreateGroup(
          TabGroupRange(destination_tab_group_index, tab_count).AsSet(),
          destination_visual_data);
  CHECK(destination_browser->GetWebStateList()->ContainsGroup(
      destination_tab_group));
}
