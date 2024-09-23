// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"

#import <memory>
#import <ostream>

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
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

  // TODO(crbug.com/40203375): Remove this workaround when it will no longer be
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
  ProfileIOS* profile = destination_browser->GetProfile();
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  const BrowserList::BrowserType browser_types =
      profile->IsOffTheRecord() ? BrowserList::BrowserType::kIncognito
                                : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browser_list->BrowsersOfType(browser_types);

  BrowserAndIndex tab_info = FindBrowserAndIndex(tab_id, browsers);

  if (!tab_info.browser) {
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
