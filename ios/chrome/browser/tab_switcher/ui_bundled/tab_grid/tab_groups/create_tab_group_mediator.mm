// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_tab_group_mediator.h"

#import <memory>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_multi_source_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_tab_group_mediator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_creation_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon_configurator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/web_state_tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@interface CreateTabGroupMediator () <WebStateListObserving>
@end

@implementation CreateTabGroupMediator {
  // Consumer of the tab group creator;
  __weak id<TabGroupCreationConsumer> _consumer;
  // List of tabs to add to the tab group.
  std::set<web::WebStateID> _identifiers;
  // Web state list where the tab group belong.
  raw_ptr<WebStateList> _webStateList;
  // Tab group to edit.
  raw_ptr<const TabGroup> _tabGroup;
  // Array of all snapshots and favicons of the group.
  NSMutableArray<TabSnapshotAndFavicon*>* _tabSnapshotsAndFavicons;
  // Item to fetch pictures.
  TabGroupItem* _groupItem;
  // Helper class to configure tab item images.
  std::unique_ptr<TabSnapshotAndFaviconConfigurator> _tabImagesConfigurator;
  // Source browser. Only set when creating a new group, not when editing an
  // existing one.
  raw_ptr<Browser> _browser;
  // Observers for WebStateList. Only set when editing an existing group,
  // when creating a new one.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObservation;
}

- (instancetype)
    initTabGroupCreationWithConsumer:(id<TabGroupCreationConsumer>)consumer
                        selectedTabs:(std::set<web::WebStateID>&)identifiers
                             browser:(Browser*)browser
                       faviconLoader:(FaviconLoader*)faviconLoader {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(!identifiers.empty()) << "Cannot create an empty tab group.";
    CHECK(browser);
    _identifiers = identifiers;

    _browser = browser;
    _webStateList = browser->GetWebStateList();
    _consumer = consumer;
    [_consumer setDefaultGroupColor:TabGroup::DefaultColorForNewTabGroup(
                                        _webStateList)];
    _tabImagesConfigurator =
        std::make_unique<TabSnapshotAndFaviconConfigurator>(faviconLoader);
    ProfileIOS* profile = browser->GetProfile();
    BrowserList* browserList = BrowserListFactory::GetForProfile(profile);

    _tabSnapshotsAndFavicons = [[NSMutableArray alloc] init];

    NSUInteger numberOfRequestedImages = 0;
    for (web::WebStateID identifier : identifiers) {
      if (numberOfRequestedImages >= 7) {
        break;
      }
      WebStateList* currentWebStateList = _webStateList;
      // TODO(crbug.com/333032676): Replace this with the appropriate helper
      // once it exists.
      int index = GetWebStateIndex(
          _webStateList, WebStateSearchCriteria{.identifier = identifier});
      if (index == WebStateList::kInvalidIndex) {
        // The user is creating a group from a long press on search result. Tab
        // search can display all tabs from the same profile at the same time.
        // The selected tab is currently in a different web state list (inactive
        // tab, or tab from another window).
        Browser* selectedTabBrowser = GetBrowserForTabWithCriteria(
            browserList, WebStateSearchCriteria{.identifier = identifier},
            profile->IsOffTheRecord());
        CHECK(selectedTabBrowser);
        currentWebStateList = selectedTabBrowser->GetWebStateList();
        index =
            GetWebStateIndex(currentWebStateList,
                             WebStateSearchCriteria{.identifier = identifier});
      }

      __weak CreateTabGroupMediator* weakSelf = self;
      _tabImagesConfigurator->FetchSingleSnapshotAndFaviconFromWebState(
          currentWebStateList->GetWebStateAt(index),
          ^(TabSnapshotAndFavicon* tabSnapshotAndFavicon) {
            [weakSelf addTabSnapshotAndFavicon:tabSnapshotAndFavicon];
            [weakSelf updateConsumer];
          });
      numberOfRequestedImages++;
    }
  }
  return self;
}

- (instancetype)initTabGroupEditionWithConsumer:
                    (id<TabGroupCreationConsumer>)consumer
                                       tabGroup:(const TabGroup*)tabGroup
                                   webStateList:(WebStateList*)webStateList
                                  faviconLoader:(FaviconLoader*)faviconLoader {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(tabGroup);
    CHECK(webStateList);
    _consumer = consumer;
    _tabGroup = tabGroup;
    _webStateList = webStateList;
    // Observe the WebStateList in the case the group disappears.
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation = std::make_unique<
        base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>(
        _webStateListObserverBridge.get());
    _scopedWebStateListObservation->AddObservation(_webStateList);
    _groupItem = [[TabGroupItem alloc] initWithTabGroup:_tabGroup];
    _tabImagesConfigurator =
        std::make_unique<TabSnapshotAndFaviconConfigurator>(faviconLoader);

    __weak CreateTabGroupMediator* weakSelf = self;
    _tabImagesConfigurator->FetchSnapshotAndFaviconForTabGroupItem(
        _groupItem, _webStateList,
        ^(TabGroupItem* item,
          NSArray<TabSnapshotAndFavicon*>* tabSnapshotsAndFavicons) {
          [weakSelf setTabSnapshotsAndFavicons:tabSnapshotsAndFavicons];
          [weakSelf updateConsumer];
        });

    // Do not use the helper to get the following values as the title helper do
    // not return nil but the number of tabs. In this case, we want nil so it do
    // not display anything.
    tab_groups::TabGroupVisualData visualData = _tabGroup->visual_data();
    [_consumer setDefaultGroupColor:visualData.color()];
    [_consumer setGroupTitle:base::SysUTF16ToNSString(visualData.title())];
  }
  return self;
}

- (void)disconnect {
  if (_tabGroup) {
    _scopedWebStateListObservation->RemoveAllObservations();
    _scopedWebStateListObservation.reset();
    _webStateListObserverBridge.reset();
    _webStateList = nullptr;
  }
}

#pragma mark - TabGroupCreationMutator

// TODO(crbug.com/40942154): Rename the function to better match what it does.
- (void)createNewGroupWithTitle:(NSString*)title
                          color:(tab_groups::TabGroupColorId)colorID
                     completion:(void (^)())completion {
  tab_groups::TabGroupVisualData visualData =
      tab_groups::TabGroupVisualData(base::SysNSStringToUTF16(title), colorID);
  if (_tabGroup) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGroupUserUpdatedGroup"));
    if (![_tabGroup->GetRawTitle() isEqualToString:title]) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGroupUserUpdatedGroupName"));
    }
    if (_tabGroup->GetColor() != colorID) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGroupUserUpdatedGroupColor"));
    }
    _webStateList->UpdateGroupVisualData(_tabGroup, visualData);
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGroupUserCreatedNewGroup"));
    WebStateList::ScopedBatchOperation lock =
        _webStateList->StartBatchOperation();
    std::set<int> tabIndexes;
    for (web::WebStateID identifier : _identifiers) {
      int index = GetWebStateIndex(_webStateList, WebStateSearchCriteria{
                                                      .identifier = identifier,
                                                  });
      if (index == WebStateList::kInvalidIndex) {
        index = _webStateList->count();
        MoveTabToBrowser(identifier, _browser, index);
      }
      tabIndexes.insert(index);
    }
    if (!tabIndexes.empty()) {
      _webStateList->CreateGroup(tabIndexes, visualData,
                                 tab_groups::TabGroupId::GenerateNew());
    }
  }
  completion();
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  CHECK_EQ(_webStateList, webStateList);
  switch (change.type()) {
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visualDataUpdate =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      if (_tabGroup == visualDataUpdate.updated_group()) {
        // Dismiss the editor.
        [self.delegate
            createTabGroupMediatorEditedGroupWasExternallyMutated:self];
      }
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& deletion =
          change.As<WebStateListChangeGroupDelete>();
      if (_tabGroup == deletion.deleted_group()) {
        // Dismiss the editor.
        [self.delegate
            createTabGroupMediatorEditedGroupWasExternallyMutated:self];
      }
      break;
    }
    default:
      // No-op.
      break;
  }
}

#pragma mark - Private helpers

// Adds the given `tabSnapshotAndFavicon` to the GroupTabInfo array.
- (void)addTabSnapshotAndFavicon:(TabSnapshotAndFavicon*)tabSnapshotAndFavicon {
  [_tabSnapshotsAndFavicons addObject:tabSnapshotAndFavicon];
}

// Sets the _tabSnapshotsAndFavicons array with `tabSnapshotsAndFavicons`.
- (void)setTabSnapshotsAndFavicons:
    (NSArray<TabSnapshotAndFavicon*>*)tabSnapshotsAndFavicons {
  _tabSnapshotsAndFavicons =
      [[NSMutableArray alloc] initWithArray:tabSnapshotsAndFavicons];
}

// Sends to the consumer the needed pictures and the number of items to display
// it properly.
- (void)updateConsumer {
  NSInteger numberOfItem =
      _tabGroup ? _tabGroup->range().count() : _identifiers.size();
  [_consumer setTabSnapshotsAndFavicons:_tabSnapshotsAndFavicons
                  numberOfSelectedItems:numberOfItem];
}

@end
