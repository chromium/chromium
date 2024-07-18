// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/saved_tab_groups/saved_tab_group.h"
#import "components/saved_tab_groups/string_utils.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item_data.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_main_tab_grid_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/image/image.h"

namespace {

using ScopedTabGroupSyncObservation =
    base::ScopedObservation<tab_groups::TabGroupSyncService,
                            tab_groups::TabGroupSyncService::Observer>;

// Converts a vector of `SavedTabGroup`s into an array of `TabGroupsPanelItem`s.
NSArray<TabGroupsPanelItem*>* CreateItems(
    std::vector<tab_groups::SavedTabGroup> groups) {
  NSMutableArray<TabGroupsPanelItem*>* items = [[NSMutableArray alloc] init];
  for (const auto& group : groups) {
    TabGroupsPanelItem* item = [[TabGroupsPanelItem alloc] init];
    item.savedTabGroupID = group.saved_guid();
    [items addObject:item];
  }
  // TODO(crbug.com/353479022): Sort by creation date.
  return items;
}

// Returns a user-friendly localized string representing the duration since the
// creation date.
NSString* CreationText(base::Time creation_date) {
  return base::SysUTF16ToNSString(tab_groups::LocalizedElapsedTimeSinceCreation(
      base::Time::Now() - creation_date));
}

}  // namespace

@interface TabGroupsPanelMediator () <TabGridToolbarsGridDelegate,
                                      TabGroupSyncServiceObserverDelegate>
@end

@implementation TabGroupsPanelMediator {
  // The service to observe.
  raw_ptr<tab_groups::TabGroupSyncService> _tabGroupSyncService;
  // The bridge between the service C++ observer and this Objective-C class.
  std::unique_ptr<TabGroupSyncServiceObserverBridge> _syncServiceObserver;
  std::unique_ptr<ScopedTabGroupSyncObservation> _scopedSyncServiceObservation;
  // Whether the service was fully initialized.
  bool _serviceInitialized;
  // The regular WebStateList, to check if there are tabs to go back to when
  // pressing the Done button.
  base::WeakPtr<WebStateList> _regularWebStateList;
  // Whether this screen is disabled by policy.
  BOOL _isDisabled;
  // Whether this screen is selected in the TabGrid.
  BOOL _selectedGrid;
}

- (instancetype)initWithTabGroupSyncService:
                    (tab_groups::TabGroupSyncService*)tabGroupSyncService
                        regularWebStateList:(WebStateList*)regularWebStateList
                           disabledByPolicy:(BOOL)disabled {
  self = [super init];
  if (self) {
    _tabGroupSyncService = tabGroupSyncService;
    _syncServiceObserver =
        std::make_unique<TabGroupSyncServiceObserverBridge>(self);
    _scopedSyncServiceObservation =
        std::make_unique<ScopedTabGroupSyncObservation>(
            _syncServiceObserver.get());
    _scopedSyncServiceObservation->Observe(_tabGroupSyncService);
    _regularWebStateList = regularWebStateList->AsWeakPtr();
    _isDisabled = disabled;
  }
  return self;
}

- (void)setConsumer:(id<TabGroupsPanelConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self populateItemsFromService];
  }
}

- (void)disconnect {
  _consumer = nil;
  _scopedSyncServiceObservation.reset();
  _syncServiceObserver.reset();
  _tabGroupSyncService = nullptr;
  _regularWebStateList = nullptr;
}

#pragma mark TabGridPageMutator

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

#pragma mark TabGridToolbarsGridDelegate

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

#pragma mark TabGroupsPanelItemDataSource

- (TabGroupsPanelItemData*)dataForItem:(TabGroupsPanelItem*)item
           withFaviconsFetchCompletion:
               (void (^)(NSArray<UIImage*>*))completion {
  const auto group = _tabGroupSyncService->GetGroup(item.savedTabGroupID);
  if (!group) {
    return nil;
  }

  // Gather the item data.
  TabGroupsPanelItemData* itemData = [[TabGroupsPanelItemData alloc] init];
  const auto title = group->title();
  const auto numberOfTabs = group->saved_tabs().size();
  if (title.length() > 0) {
    itemData.title = base::SysUTF16ToNSString(title);
  } else {
    itemData.title = l10n_util::GetPluralNSStringF(
        IDS_IOS_TAB_GROUP_TABS_NUMBER, numberOfTabs);
  }
  itemData.color = TabGroup::ColorForTabGroupColorId(group->color());
  itemData.creationText =
      CreationText(group->creation_time_windows_epoch_micros());
  itemData.numberOfTabs = static_cast<NSUInteger>(numberOfTabs);

  // Fetch the favicons.
  NSMutableArray<UIImage*>* favicons = [[NSMutableArray alloc] init];
  const auto saved_tabs = group->saved_tabs();
  // Query up to 4 favicons…
  NSUInteger maxFaviconsCount = MIN(numberOfTabs, 4);
  // … but only 3 if there is an overflow. The 4th spot is replaced with the
  // count of remaining tabs.
  if (maxFaviconsCount > 4) {
    maxFaviconsCount = 3;
  }
  for (size_t i = 0; i < saved_tabs.size() && favicons.count < maxFaviconsCount;
       i++) {
    // TODO(crbug.com/351110394): Fetch favicons from the URL.
    const auto favicon = saved_tabs[i].favicon();
    if (favicon) {
      [favicons addObject:favicon->ToUIImage()];
    } else {
      [favicons addObject:[UIImage imageNamed:@"ic_close"]];
    }
  }
  // TODO(crbug.com/351110394): Remove this simulated asynchronous fetch once
  // actually fetching favicons asynchronously.
  dispatch_async(dispatch_get_main_queue(), ^{
    completion(favicons);
  });

  return itemData;
}

#pragma mark TabGroupsPanelMutator

- (void)selectTabGroupsPanelItem:(TabGroupsPanelItem*)item {
  _tabGroupSyncService->OpenTabGroup(
      item.savedTabGroupID,
      std::make_unique<tab_groups::TabGroupActionContext>());
}

#pragma mark TabGroupSyncServiceObserverDelegate

- (void)tabGroupSyncServiceInitialized {
  _serviceInitialized = true;
  [self populateItemsFromService];
}

- (void)tabGroupSyncServiceTabGroupAdded:(const tab_groups::SavedTabGroup&)group
                              fromSource:(tab_groups::TriggerSource)source {
  [self populateItemsFromService];
}

- (void)tabGroupSyncServiceTabGroupUpdated:
            (const tab_groups::SavedTabGroup&)group
                                fromSource:(tab_groups::TriggerSource)source {
  // TODO(crbug.com/353479040): Just reconfigure the group that changed.
  [self populateItemsFromService];
}

- (void)tabGroupSyncServiceLocalTabGroupRemoved:
            (const tab_groups::LocalTabGroupID&)localID
                                     fromSource:
                                         (tab_groups::TriggerSource)source {
  // No-op. Only respond to Saved Tab Group Removed event.
}

- (void)tabGroupSyncServiceSavedTabGroupRemoved:(const base::Uuid&)syncID
                                     fromSource:
                                         (tab_groups::TriggerSource)source {
  [self populateItemsFromService];
}

#pragma mark Private

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

// Reads the TabGroupSync
- (void)populateItemsFromService {
  if (_serviceInitialized) {
    [_consumer populateItems:CreateItems(_tabGroupSyncService->GetAllGroups())];
  }
}

@end
