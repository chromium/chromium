// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/group_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_consumer.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

@implementation TabGroupMediator {
  // Tab group consumer.
  __weak id<TabGroupConsumer> _groupConsumer;
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
  if (self = [super init]) {
    self.webStateList = webStateList;
    _groupConsumer = groupConsumer;
    self.consumer = gridConsumer;

    const tab_groups::TabGroupVisualData& groupInformations =
        tabGroup->visual_data();
    [_groupConsumer
        setGroupTitle:base::SysUTF16ToNSString(groupInformations.title())];
    [_groupConsumer
        setGroupColor:ColorForTabGroupColorId(groupInformations.color())];

    web::WebStateID activeWebStateID;
    int webStateIndex = self.webStateList->active_index();
    if (webStateIndex == WebStateList::kInvalidIndex) {
      activeWebStateID = web::WebStateID();
    } else {
      web::WebState* webState = self.webStateList->GetWebStateAt(webStateIndex);
      activeWebStateID = webState->GetUniqueIdentifier();
    }

    [self.consumer populateItems:CreateTabItems(
                                     self.webStateList,
                                     self.webStateList->GetGroupRange(tabGroup))
                  selectedItemID:activeWebStateID];
  }
  return self;
}

#pragma mark - TabGroupMutator

- (BOOL)addNewItemInGroup {
  // TODO(crbug.com/1501837): Call the appropriate function. Ensure to add new
  // tab only if policies allows it.
  return NO;
}

#pragma mark - Parent's functions

- (void)configureToolbarsButtons {
  // No-op
}

@end
