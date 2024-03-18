// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_mediator.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_creation_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@implementation CreateTabGroupMediator {
  // Consumer of the tab group creator;
  __weak id<TabGroupCreationConsumer> _consumer;
  // List of tabs to add to the tab group.
  std::set<web::WebStateID> _identifiers;
  // Web state list where the tab group belong.
  WebStateList* _webStateList;
  // List of snapshots.
  NSMutableArray* _snapshots;
  // List of favicons.
  NSMutableArray* _favicons;
  // Tab group to edit.
  const TabGroup* _tabGroup;
}

- (instancetype)
    initTabGroupCreationWithConsumer:(id<TabGroupCreationConsumer>)consumer
                        selectedTabs:(std::set<web::WebStateID>&)identifiers
                        webStateList:(WebStateList*)webStateList {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(!identifiers.empty()) << "Cannot create an empty tab group.";
    CHECK(webStateList);
    _consumer = consumer;
    // TODO(crbug.com/1501837): Get the default color from the component.
    [_consumer setDefaultGroupColor:tab_groups::TabGroupColorId::kPink];

    _identifiers = identifiers;
    _webStateList = webStateList;

    _snapshots = [[NSMutableArray alloc] init];
    _favicons = [[NSMutableArray alloc] init];

    NSUInteger numberOfRequestedImages = 0;

    for (web::WebStateID identifier : identifiers) {
      if (numberOfRequestedImages >= 7) {
        break;
      }
      int index = GetWebStateIndex(
          webStateList, WebStateSearchCriteria{.identifier = identifier});
      CHECK_NE(index, WebStateList::kInvalidIndex);
      TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
          initWithWebState:webStateList->GetWebStateAt(index)];
      __weak CreateTabGroupMediator* weakSelf = self;

      [item fetchSnapshot:^(TabSwitcherItem* innerItem, UIImage* snapshot) {
        [innerItem fetchFavicon:^(TabSwitcherItem* faviconItem, UIImage* icon) {
          [weakSelf saveSnapshots:snapshot];
          [weakSelf saveFavicons:icon];
          [weakSelf updateConsumer];
        }];
      }];

      numberOfRequestedImages++;
    }
  }
  return self;
}

- (instancetype)initTabGroupEditionWithConsumer:
                    (id<TabGroupCreationConsumer>)consumer
                                       tabGroup:(const TabGroup*)tabGroup
                                   webStateList:(WebStateList*)webStateList {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(tabGroup);
    _consumer = consumer;
    _tabGroup = tabGroup;
    // TODO(crbug.com/1501837): Get list of web states from the group, and fetch
    // snapshots and favicons and send it to the consumer.
    [_consumer setDefaultGroupColor:_tabGroup->visual_data().color()];
    // TODO(crbug.com/1501837): Set title with current value.
  }
  return self;
}

#pragma mark - TabGroupCreationMutator

- (void)createNewGroupWithTitle:(NSString*)title
                          color:(tab_groups::TabGroupColorId)colorID
                     completion:(void (^)())completion {
  std::set<int> tabIndexes;
  for (web::WebStateID identifier : _identifiers) {
    int index = GetWebStateIndex(_webStateList, WebStateSearchCriteria{
                                                    .identifier = identifier,
                                                });
    tabIndexes.insert(index);
  }
  tab_groups::TabGroupVisualData visualData =
      tab_groups::TabGroupVisualData(base::SysNSStringToUTF16(title), colorID);
  _webStateList->CreateGroup(tabIndexes, visualData);
  completion();
}

#pragma mark - Private helpers

// Saves the given snapshot in the snapshot list. If the image is nil, add a
// null object so snapshot[i] and favicons[i] does not missmatch.
- (void)saveSnapshots:(UIImage*)snapshot {
  if (snapshot) {
    [_snapshots addObject:snapshot];
  } else {
    [_snapshots addObject:[[UIImage alloc] init]];
  }
}

// Saves the given favicon in the favicon list. If the image is nil, add a null
// object so snapshot[i] and favicons[i] does not missmatch.
- (void)saveFavicons:(UIImage*)favicon {
  if (favicon) {
    [_favicons addObject:favicon];
  } else {
    [_favicons addObject:[[UIImage alloc] init]];
  }
}

- (void)updateConsumer {
  [_consumer setSnapshots:_snapshots
                   favicons:_favicons
      numberOfSelectedItems:_identifiers.size()];
}

@end
