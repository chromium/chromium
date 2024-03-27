// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"

std::set<const TabGroup*> GetAllGroupsForBrowserState(
    ChromeBrowserState* browser_state) {
  BOOL incognito = browser_state->IsOffTheRecord();
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state);
  std::set<const TabGroup*> groups;
  std::set<Browser*> all_browsers = incognito
                                        ? browser_list->AllIncognitoBrowsers()
                                        : browser_list->AllRegularBrowsers();
  for (Browser* browser : all_browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    groups.merge(web_state_list->GetGroups());
  }

  return groups;
}

void MoveTabToGroup(web::WebStateID web_state_identifier,
                    const TabGroup* destination_group,
                    ChromeBrowserState* browser_state) {
  BOOL incognito = browser_state->IsOffTheRecord();
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state);
  std::set<Browser*> all_browsers = incognito
                                        ? browser_list->AllIncognitoBrowsers()
                                        : browser_list->AllRegularBrowsers();

  int web_state_index = WebStateList::kInvalidIndex;
  WebStateList* origin_web_state_list;
  for (Browser* browser : all_browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    int index = GetWebStateIndex(
        web_state_list,
        WebStateSearchCriteria{.identifier = web_state_identifier});
    if (index != WebStateList::kInvalidIndex) {
      if (web_state_list->ContainsGroup(destination_group)) {
        // Move in the same WebStateList.
        web_state_list->MoveToGroup({index}, destination_group);
        return;
      }
      web_state_index = index;
      origin_web_state_list = web_state_list;
      break;
    }
  }

  if (web_state_index == WebStateList::kInvalidIndex) {
    return;
  }

  for (Browser* browser : all_browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    if (web_state_list->ContainsGroup(destination_group)) {
      std::unique_ptr<web::WebState> web_state =
          origin_web_state_list->DetachWebStateAt(web_state_index);
      web_state_list->InsertWebState(
          std::move(web_state),
          WebStateList::InsertionParams::Automatic().InGroup(
              destination_group));
      return;
    }
  }
}
