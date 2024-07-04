// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mediator.h"

#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_main_tab_grid_delegate.h"

@interface TabGroupsPanelMediator () <TabGridToolbarsGridDelegate>
@end

@implementation TabGroupsPanelMediator {
  // The regular WebStateList, to check if there are tabs to go back to when
  // pressing the Done button.
  base::WeakPtr<WebStateList> _regularWebStateList;
  // Whether this screen is disabled by policy.
  BOOL _isDisabled;
  // Whether this screen is selected in the TabGrid.
  BOOL _selectedGrid;
}

- (instancetype)initWithRegularWebStateList:(WebStateList*)regularWebStateList
                           disabledByPolicy:(BOOL)disabled {
  self = [super init];
  if (self) {
    _regularWebStateList = regularWebStateList->AsWeakPtr();
    _isDisabled = disabled;
  }
  return self;
}

- (void)setConsumer:(id<TabGroupsPanelConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    // TODO(crbug.com/329625483): Populate with real data from the
    // Tab Group Sync Service.
    [_consumer populateItems:[self fakeItems]];
  }
}

// TODO(crbug.com/329625483): Remove this method when populating with real data
// from the Tab Group Sync Service.
- (NSArray<TabGroupsPanelItem*>*)fakeItems {
  static BOOL multipleItems = false;
  multipleItems = !multipleItems;
  if (multipleItems) {
    auto ArrayByRepeatingObject = ^NSArray*(NSObject* object, NSInteger count) {
      NSMutableArray* array = [NSMutableArray arrayWithCapacity:count];
      while (count-- > 0) {
        [array addObject:object];
      }
      return array;
    };
    TabGroupsPanelItem* item1 = [[TabGroupsPanelItem alloc] init];
    item1.title = @"Vacation";
    item1.creationDate = base::Time::Now() - base::Seconds(1);
    item1.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kYellow);
    item1.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 1);
    TabGroupsPanelItem* item2 = [[TabGroupsPanelItem alloc] init];
    item2.title = @"Research paper";
    item2.creationDate = base::Time::Now() - base::Days(1);
    item2.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kPink);
    item2.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 4);
    TabGroupsPanelItem* item3 = [[TabGroupsPanelItem alloc] init];
    item3.title = @"Running shoes";
    item3.creationDate = base::Time::Now() - base::Days(300);
    item3.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kBlue);
    item3.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 10);
    TabGroupsPanelItem* item4 = [[TabGroupsPanelItem alloc] init];
    item4.title = @"2 Vacation";
    item4.creationDate = base::Time::Now() - base::Seconds(1);
    item4.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kYellow);
    item4.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 1);
    TabGroupsPanelItem* item5 = [[TabGroupsPanelItem alloc] init];
    item5.title = @"2 Research paper";
    item5.creationDate = base::Time::Now() - base::Days(1);
    item5.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kPink);
    item5.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 4);
    TabGroupsPanelItem* item6 = [[TabGroupsPanelItem alloc] init];
    item6.title = @"2 Running shoes";
    item6.creationDate = base::Time::Now() - base::Days(300);
    item6.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kBlue);
    item6.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 10);
    TabGroupsPanelItem* item7 = [[TabGroupsPanelItem alloc] init];
    item7.title = @"3 Vacation";
    item7.creationDate = base::Time::Now() - base::Seconds(1);
    item7.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kYellow);
    item7.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 1);
    TabGroupsPanelItem* item8 = [[TabGroupsPanelItem alloc] init];
    item8.title = @"3 Research paper";
    item8.creationDate = base::Time::Now() - base::Days(1);
    item8.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kPink);
    item8.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 4);
    TabGroupsPanelItem* item9 = [[TabGroupsPanelItem alloc] init];
    item9.title = @"3 Running shoes";
    item9.creationDate = base::Time::Now() - base::Days(300);
    item9.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kBlue);
    item9.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 10);
    TabGroupsPanelItem* item10 = [[TabGroupsPanelItem alloc] init];
    item10.title = @"4 Vacation";
    item10.creationDate = base::Time::Now() - base::Seconds(1);
    item10.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kYellow);
    item10.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 1);
    TabGroupsPanelItem* item11 = [[TabGroupsPanelItem alloc] init];
    item11.title = @"4 Research paper";
    item11.creationDate = base::Time::Now() - base::Days(1);
    item11.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kPink);
    item11.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 4);
    TabGroupsPanelItem* item12 = [[TabGroupsPanelItem alloc] init];
    item12.title = @"4 Running shoes";
    item12.creationDate = base::Time::Now() - base::Days(300);
    item12.color =
        TabGroup::ColorForTabGroupColorId(tab_groups::TabGroupColorId::kBlue);
    item12.favicons = ArrayByRepeatingObject([[UIImage alloc] init], 10);
    return @[
      item1, item2, item3, item4, item5, item6, item7, item8, item9, item10,
      item11, item12
    ];
  } else {
    TabGroupsPanelItem* item = [[TabGroupsPanelItem alloc] init];
    item.title = @"Vacation";
    return @[ item ];
  }
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  _selectedGrid = selected;

  if (selected) {
    base::RecordAction(base::UserMetricsAction("MobileTabGridSelectTabGroups"));

    [self configureToolbarsButtons];
  }
}

- (void)switchToMode:(TabGridMode)mode {
  CHECK(mode == TabGridModeNormal)
      << "Tab Groups panel should only support Normal mode.";
}

#pragma mark - TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)doneButtonTapped:(id)sender {
  [self.toolbarTabGridDelegate doneButtonTapped:sender];
}

- (void)newTabButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)selectAllButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)searchButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)cancelSearchButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)closeSelectedTabs:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)shareSelectedTabs:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

- (void)selectTabsButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should not be called in Tab Groups.";
}

#pragma mark - TabGroupsPanelMutator

- (void)selectTabGroupsPanelItem:(TabGroupsPanelItem*)item {
  // TODO(crbug.com/329626537): Handle opening the tab group locally.
  [_consumer populateItems:[self fakeItems]];
}

#pragma mark - Private

// Creates and send a tab grid toolbar configuration with button that should be
// displayed when Tab Groups is selected.
- (void)configureToolbarsButtons {
  if (!_selectedGrid) {
    return;
  }
  // Start to configure the delegate, so configured buttons will depend on the
  // correct delegate.
  [self.toolbarsMutator setToolbarsButtonsDelegate:self];

  if (_isDisabled) {
    [self.toolbarsMutator
        setToolbarConfiguration:
            [TabGridToolbarsConfiguration
                disabledConfigurationForPage:TabGridPageTabGroups]];
    return;
  }

  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc] initWithPage:TabGridPageTabGroups];
  toolbarsConfiguration.mode = TabGridModeNormal;
  // Done button is enabled if there is at least one Regular tab.
  toolbarsConfiguration.doneButton =
      _regularWebStateList && !_regularWebStateList->empty();
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

@end
