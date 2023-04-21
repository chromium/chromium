// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_util.h"

#import <memory>
#import <ostream>
#import <set>

#import "base/check_op.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Given a set of `browsers`, finds the one with `tab_id`. Returns the browser
// and `tab_index` of the tab within the returned browser’s WebStateList.
Browser* FindBrowser(NSString* tab_id,
                     const std::set<Browser*>& browsers,
                     int& tab_index) {
  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (int i = 0; i < web_state_list->count(); ++i) {
      web::WebState* web_state = web_state_list->GetWebStateAt(i);
      NSString* current_tab_id = web_state->GetStableIdentifier();
      if ([current_tab_id isEqualToString:tab_id]) {
        tab_index = i;
        return browser;
      }
    }
  }
  tab_index = WebStateList::kInvalidIndex;
  return nullptr;
}

// Finds the browser in `browser_list` containing a tab with `tab_id`. Searches
// incognito browsers if `incognito` is true, otherwise searches regular
// browsers. Returns the browser and `tab_index` of the tab within the returned
// browser's WebStateList.
Browser* FindBrowser(NSString* tab_id,
                     BrowserList* browser_list,
                     bool incognito,
                     int& tab_index) {
  return incognito ? FindBrowser(tab_id, browser_list->AllIncognitoBrowsers(),
                                 tab_index)
                   : FindBrowser(tab_id, browser_list->AllRegularBrowsers(),
                                 tab_index);
}

// Moves snapshot associated with `snapshot_id` from `source_browser` to
// `destination_browser`'s snapshot cache.
void MoveSnapshot(NSString* snapshot_id,
                  Browser* source_browser,
                  Browser* destination_browser) {
  DCHECK(snapshot_id.length);
  SnapshotCache* source_cache =
      SnapshotBrowserAgent::FromBrowser(source_browser)->snapshot_cache();
  SnapshotCache* destination_cache =
      SnapshotBrowserAgent::FromBrowser(destination_browser)->snapshot_cache();
  [source_cache
      retrieveImageForSnapshotID:snapshot_id
                        callback:^(UIImage* snapshot) {
                          if (snapshot) {
                            [destination_cache setImage:snapshot
                                         withSnapshotID:snapshot_id];
                            [source_cache
                                removeImageWithSnapshotID:snapshot_id];
                          }
                        }];
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
  MoveSnapshot(snapshot_tab_helper->GetSnapshotIdentifier(), source_browser,
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
  int source_tab_index = WebStateList::kInvalidIndex;
  Browser* source_browser = FindBrowser(
      tab_id, browser_list, browser_state->IsOffTheRecord(), source_tab_index);
  if (!source_browser) {
    NOTREACHED() << "Either the tab_id is incorrect, or the user is attempting "
                    "to move a tab across profiles (incognito <-> regular)";
    return;
  }
  MoveTabFromBrowserToBrowser(source_browser, source_tab_index,
                              destination_browser, destination_tab_index,
                              flags);
}

void MoveTabToBrowser(NSString* tab_id,
                      Browser* destination_browser,
                      int destination_tab_index) {
  MoveTabToBrowser(tab_id, destination_browser, destination_tab_index,
                   WebStateList::InsertionFlags::INSERT_NO_FLAGS);
}
