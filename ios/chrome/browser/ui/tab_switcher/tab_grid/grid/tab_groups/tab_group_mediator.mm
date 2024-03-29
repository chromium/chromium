// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

@implementation TabGroupMediator {
  // Tab group consumer.
  __weak id<TabGroupConsumer> _groupConsumer;
  // Current group.
  const TabGroup* _tabGroup;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            tabGroup:(const TabGroup*)tabGroup
                            consumer:(id<TabGroupConsumer>)groupConsumer
                        gridConsumer:(id<TabCollectionConsumer>)gridConsumer {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group mediator outside the "
         "Tab Groups experiment.";
  CHECK(webStateList);
  CHECK(groupConsumer);
  CHECK(tabGroup);
  if (self = [super init]) {
    self.webStateList = webStateList;
    _groupConsumer = groupConsumer;
    self.consumer = gridConsumer;

    _tabGroup = tabGroup;

    [_groupConsumer setGroupTitle:tabGroup->GetTitle()];
    [_groupConsumer setGroupColor:tabGroup->GetColor()];

    [self populateConsumerItems];
  }
  return self;
}

#pragma mark - TabGroupMutator

- (BOOL)addNewItemInGroup {
  if (!self.browser) {
    return NO;
  }
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (!browserState ||
      !IsAddNewTabAllowedByPolicy(browserState->GetPrefs(),
                                  browserState->IsOffTheRecord())) {
    return NO;
  }

  web::WebState::CreateParams params(browserState);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  web::NavigationManager::WebLoadParams loadParams((GURL(kChromeUINewTabURL)));
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  self.webStateList->InsertWebState(
      std::move(webState),
      WebStateList::InsertionParams::Automatic().InGroup(_tabGroup).Activate());

  return YES;
}

#pragma mark - Parent's functions

- (void)configureToolbarsButtons {
  // No-op
}

- (void)populateConsumerItems {
  if (!self.webStateList || !_tabGroup) {
    return;
  }

  GridItemIdentifier* identifier = nil;
  int webStateIndex = self.webStateList->active_index();
  if (webStateIndex != WebStateList::kInvalidIndex &&
      self.webStateList->GetGroupOfWebStateAt(webStateIndex) == _tabGroup) {
    web::WebState* webState = self.webStateList->GetWebStateAt(webStateIndex);
    identifier = [GridItemIdentifier tabIdentifier:webState];
  }

  [self.consumer populateItems:CreateTabItems(
                                   self.webStateList,
                                   self.webStateList->GetGroupRange(_tabGroup))
        selectedItemIdentifier:identifier];
}

@end
