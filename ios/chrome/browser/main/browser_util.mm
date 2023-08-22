// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_util.h"

#import <memory>
#import <ostream>

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/web/public/web_state.h"

namespace {

// Moves snapshot associated with `snapshot_id` from `source_browser` to
// `destination_browser`'s snapshot cache.
void MoveSnapshot(SnapshotID snapshot_id,
                  Browser* source_browser,
                  Browser* destination_browser) {
  DCHECK(snapshot_id.valid());
  SnapshotCache* source_cache =
      SnapshotBrowserAgent::FromBrowser(source_browser)->snapshot_cache();
  SnapshotCache* destination_cache =
      SnapshotBrowserAgent::FromBrowser(destination_browser)->snapshot_cache();
  [source_cache migrateImageWithSnapshotID:snapshot_id
                           toSnapshotCache:destination_cache];
}

}  // namespace

void MoveTabFromBrowserToBrowser(Browser* source_browser,
                                 int source_tab_index,
                                 Browser* destination_browser,
                                 int destination_tab_index,
                                 WebStateList::InsertionFlags flags) {
  if (source_browser == destination_browser) {
    // This is a reorder operation within the same WebStateList.
    destination_browser->GetWebStateList()->MoveWebStateAt(
        source_tab_index, destination_tab_index);
    return;
  }
  std::unique_ptr<web::WebState> web_state =
      source_browser->GetWebStateList()->DetachWebStateAt(source_tab_index);
  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state.get());
  MoveSnapshot(snapshot_tab_helper->GetSnapshotID(), source_browser,
               destination_browser);

  int insertion_flags = flags;
  if (insertion_flags == WebStateList::InsertionFlags::INSERT_NO_FLAGS) {
    insertion_flags = WebStateList::INSERT_FORCE_INDEX;
    // TODO(crbug.com/1264451): Remove this workaround when it will not be
    // longer required to have an active WebState in the WebStateList.
    if (destination_browser->GetWebStateList()->empty()) {
      insertion_flags = WebStateList::INSERT_ACTIVATE;
    }
  }

  destination_browser->GetWebStateList()->InsertWebState(
      destination_tab_index, std::move(web_state), insertion_flags,
      WebStateOpener());
}

void MoveTabFromBrowserToBrowser(Browser* source_browser,
                                 int source_tab_index,
                                 Browser* destination_browser,
                                 int destination_tab_index) {
  MoveTabFromBrowserToBrowser(source_browser, source_tab_index,
                              destination_browser, destination_tab_index,
                              WebStateList::InsertionFlags::INSERT_NO_FLAGS);
}

void MoveTabToBrowser(NSString* tab_id,
                      Browser* destination_browser,
                      int destination_tab_index,
                      WebStateList::InsertionFlags flags) {
  DCHECK(tab_id.length);
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
                              destination_browser, destination_tab_index,
                              flags);
}

void MoveTabToBrowser(NSString* tab_id,
                      Browser* destination_browser,
                      int destination_tab_index) {
  MoveTabToBrowser(tab_id, destination_browser, destination_tab_index,
                   WebStateList::InsertionFlags::INSERT_NO_FLAGS);
}

BrowserAndIndex FindBrowserAndIndex(NSString* tab_id,
                                    const std::set<Browser*>& browsers) {
  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (int i = 0; i < web_state_list->count(); ++i) {
      web::WebState* web_state = web_state_list->GetWebStateAt(i);
      NSString* current_tab_id = web_state->GetStableIdentifier();
      if ([current_tab_id isEqualToString:tab_id]) {
        return BrowserAndIndex{
            .browser = browser,
            .tab_index = i,
        };
      }
    }
  }
  return BrowserAndIndex{};
}
