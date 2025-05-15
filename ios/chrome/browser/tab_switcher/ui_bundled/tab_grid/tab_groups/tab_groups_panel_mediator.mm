// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/collaboration/public/messaging/message.h"
#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "components/collaboration/public/messaging/util.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/string_utils.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_bridge.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_observer_bridge.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/coordinator/tab_group_favicons_grid_configurator.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"
#import "ios/chrome/browser/share_kit/model/share_kit_face_pile_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/sharing_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_cell.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_item_data.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_mediator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/favicon_size.h"
#import "ui/gfx/image/image.h"

using PeopleGroupActionOutcome =
    data_sharing::DataSharingService::PeopleGroupActionOutcome;
using ScopedDataSharingSyncObservation =
    base::ScopedObservation<data_sharing::DataSharingService,
                            data_sharing::DataSharingService::Observer>;

using collaboration::messaging::TabGroupMessageMetadata;
using tab_groups::SharingState;
using tab_groups::utils::GetLocalTabGroupInfo;
using tab_groups::utils::LocalTabGroupInfo;

namespace {

// The preferred size in points for the avatar icons.
constexpr CGFloat kFacePileAvatarSize = 24;

using ScopedTabGroupSyncObservation =
    base::ScopedObservation<tab_groups::TabGroupSyncService,
                            tab_groups::TabGroupSyncService::Observer>;

// Comparator for groups by creation date.
bool CompareGroupByCreationDate(const tab_groups::SavedTabGroup& a,
                                const tab_groups::SavedTabGroup& b) {
  return a.creation_time() > b.creation_time();
}

// Converts deletion notifications from the messaging service into a
// notification `TabGroupsPanelItem`.
TabGroupsPanelItem* CreateNotificationItem(
    std::vector<collaboration::messaging::PersistentMessage> messages) {
  std::optional<std::string> summary =
      collaboration::messaging::GetRemovedCollaborationsSummary(messages);
  if (!summary.has_value()) {
    return nil;
  }
  NSString* text = base::SysUTF8ToNSString(summary.value());
  return [[TabGroupsPanelItem alloc] initWithNotificationText:text];
}

// Returns a user-friendly localized string representing the duration since the
// creation date.
NSString* CreationText(base::Time creation_date) {
  return base::SysUTF16ToNSString(tab_groups::LocalizedElapsedTimeSinceCreation(
      base::Time::Now() - creation_date));
}

}  // namespace

@interface TabGroupsPanelMediator () <DataSharingServiceObserverDelegate,
                                      MessagingBackendServiceObserving,
                                      TabGridToolbarsGridDelegate,
                                      TabGroupSyncServiceObserverDelegate>
@end

@implementation TabGroupsPanelMediator {
  // The service to observe.
  raw_ptr<tab_groups::TabGroupSyncService> _tabGroupSyncService;
  // The share kit service.
  raw_ptr<ShareKitService> _shareKitService;
  // The collaboration service.
  raw_ptr<collaboration::CollaborationService> _collaborationService;
  // The service to get activity messages for shared tab groups.
  raw_ptr<collaboration::messaging::MessagingBackendService> _messagingService;
  // The data sharing service for shared tab groups.
  raw_ptr<data_sharing::DataSharingService> _dataSharingService;
  // The bridge between the service C++ observer and this Objective-C class.
  std::unique_ptr<TabGroupSyncServiceObserverBridge> _syncServiceObserver;
  std::unique_ptr<ScopedTabGroupSyncObservation> _scopedSyncServiceObservation;
  // The bridge between the C++ MessagingBackendService observer and this
  // Objective-C class.
  std::unique_ptr<MessagingBackendServiceBridge> _messagingBackendServiceBridge;
  // The bridge between the C++ DataSharingService observer and this Objective-C
  // class.
  std::unique_ptr<DataSharingServiceObserverBridge> _dataSharingServiceObserver;
  std::unique_ptr<ScopedDataSharingSyncObservation>
      _scopedDataSharingServiceObservation;
  // Whether the service was fully initialized.
  bool _tabGroupSyncServiceInitialized;
  // The regular WebStateList, to check if there are tabs to go back to when
  // pressing the Done button.
  base::WeakPtr<WebStateList> _regularWebStateList;
  // Whether this screen is disabled by policy.
  BOOL _isDisabled;
  // Whether this screen is selected in the TabGrid.
  BOOL _selectedGrid;
  // A list of Browsers.
  raw_ptr<BrowserList> _browserList;
  // Configures favicons for TabGroupFaviconsGrid objects.
  std::unique_ptr<TabGroupFaviconsGridConfigurator> _faviconsGridConfigurator;
}

- (instancetype)
    initWithTabGroupSyncService:
        (tab_groups::TabGroupSyncService*)tabGroupSyncService
                shareKitService:(ShareKitService*)shareKitService
           collaborationService:
               (collaboration::CollaborationService*)collaborationService
               messagingService:
                   (collaboration::messaging::MessagingBackendService*)
                       messagingService
             dataSharingService:
                 (data_sharing::DataSharingService*)dataSharingService
            regularWebStateList:(WebStateList*)regularWebStateList
                  faviconLoader:(FaviconLoader*)faviconLoader
               disabledByPolicy:(BOOL)disabled
                    browserList:(BrowserList*)browserList {
  self = [super init];
  if (self) {
    _tabGroupSyncService = tabGroupSyncService;
    _shareKitService = shareKitService;
    _collaborationService = collaborationService;
    _messagingService = messagingService;
    _dataSharingService = dataSharingService;
    _syncServiceObserver =
        std::make_unique<TabGroupSyncServiceObserverBridge>(self);
    _scopedSyncServiceObservation =
        std::make_unique<ScopedTabGroupSyncObservation>(
            _syncServiceObserver.get());
    _scopedSyncServiceObservation->Observe(_tabGroupSyncService);
    if (_messagingService) {
      _messagingBackendServiceBridge =
          std::make_unique<MessagingBackendServiceBridge>(self);
      _messagingService->AddPersistentMessageObserver(
          _messagingBackendServiceBridge.get());
    }
    if (dataSharingService) {
      _dataSharingServiceObserver =
          std::make_unique<DataSharingServiceObserverBridge>(self);
      _scopedDataSharingServiceObservation =
          std::make_unique<ScopedDataSharingSyncObservation>(
              _dataSharingServiceObserver.get());
      _scopedDataSharingServiceObservation->Observe(_dataSharingService);
    }
    _regularWebStateList = regularWebStateList->AsWeakPtr();
    _isDisabled = disabled;
    _browserList = browserList;
    _faviconsGridConfigurator =
        std::make_unique<TabGroupFaviconsGridConfigurator>(_tabGroupSyncService,
                                                           faviconLoader);
  }
  return self;
}

- (void)setConsumer:(id<TabGroupsPanelConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self populateItemsFromServices];
  }
}

- (void)deleteSyncedTabGroup:(const base::Uuid&)syncID {
  std::optional<tab_groups::SavedTabGroup> group =
      _tabGroupSyncService->GetGroup(syncID);
  if (!group) {
    return;
  }

  LocalTabGroupInfo tabGroupInfo = GetLocalTabGroupInfo(_browserList, *group);
  if (tabGroupInfo.tab_group) {
    // Delete the group and tabs in the group locally. It automatically updates
    // the tab group sync service.
    CloseAllWebStatesInGroup(*tabGroupInfo.web_state_list,
                             tabGroupInfo.tab_group,
                             WebStateList::CLOSE_USER_ACTION);
  } else {
    // The group doesn't exist locally. Delete the group from the tab group
    // sync service.
    _tabGroupSyncService->RemoveGroup(syncID);
  }
}

- (void)deleteSharedTabGroup:(const base::Uuid&)syncID {
  [self takeActionForActionType:TabGroupActionType::kDeleteSharedTabGroup
                 sharedTabGroup:syncID];
}

- (void)leaveSharedTabGroup:(const base::Uuid&)syncID {
  [self takeActionForActionType:TabGroupActionType::kLeaveSharedTabGroup
                 sharedTabGroup:syncID];
}

- (void)disconnect {
  if (_messagingService) {
    _messagingService->RemovePersistentMessageObserver(
        _messagingBackendServiceBridge.get());
    _messagingBackendServiceBridge.reset();
    _messagingService = nullptr;
  }
  _scopedDataSharingServiceObservation.reset();
  _dataSharingServiceObserver.reset();
  _consumer = nil;
  _scopedSyncServiceObservation.reset();
  _syncServiceObserver.reset();
  _tabGroupSyncService = nullptr;
  _shareKitService = nullptr;
  _collaborationService = nullptr;
  _dataSharingService = nullptr;
  _regularWebStateList = nullptr;
  _faviconsGridConfigurator = nullptr;
}

#pragma mark MessagingBackendServiceObserving

- (void)onMessagingBackendServiceInitialized {
  [self populateItemsFromServices];
}

- (void)displayPersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());
  [self populateItemsFromServices];
}

- (void)hidePersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());
  [self populateItemsFromServices];
}

#pragma mark DataSharingServiceObserverDelegate

- (void)dataSharingServiceDidAddGroup:(const data_sharing::GroupData&)groupData
                               atTime:(base::Time)eventTime {
  [self reconfigureGroupForGroupId:groupData.group_token.group_id];
}

- (void)dataSharingServiceDidRemoveGroup:(const data_sharing::GroupId&)groupId
                                  atTime:(base::Time)eventTime {
  [self reconfigureGroupForGroupId:groupId];
}

- (void)dataSharingServiceDidAddMember:(const GaiaId&)memberId
                               toGroup:(const data_sharing::GroupId&)groupId
                                atTime:(base::Time)eventTime {
  [self reconfigureGroupForGroupId:groupId];
}

- (void)dataSharingServiceDidRemoveMember:(const GaiaId&)memberId
                                  toGroup:(const data_sharing::GroupId&)groupId
                                   atTime:(base::Time)eventTime {
  [self reconfigureGroupForGroupId:groupId];
}

#pragma mark TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  _selectedGrid = selected;

  if (selected) {
    base::RecordAction(base::UserMetricsAction("MobileTabGridSelectTabGroups"));

    [self configureToolbarsButtons];
  }
}

- (void)setPageAsActive {
  NOTREACHED() << "Should not be called in Tab Groups.";
}

#pragma mark TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in Tab Groups.";
}

- (void)doneButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridDone"));
  [self.tabGridHandler exitTabGrid];
}

- (void)newTabButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in Tab Groups.";
}

- (void)selectAllButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in Tab Groups.";
}

- (void)searchButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in Tab Groups.";
}

- (void)cancelSearchButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in Tab Groups.";
}

- (void)closeSelectedTabs:(id)sender {
  NOTREACHED() << "Should not be called in Tab Groups.";
}

- (void)shareSelectedTabs:(id)sender {
  NOTREACHED() << "Should not be called in Tab Groups.";
}

- (void)selectTabsButtonTapped:(id)sender {
  NOTREACHED() << "Should not be called in Tab Groups.";
}

#pragma mark TabGroupsPanelItemDataSource

- (TabGroupsPanelItemData*)dataForItem:(TabGroupsPanelItem*)item {
  std::optional<tab_groups::SavedTabGroup> group =
      _tabGroupSyncService->GetGroup(item.savedTabGroupID);
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
  itemData.color = tab_groups::ColorForTabGroupColorId(group->color());
  itemData.creationText = CreationText(group->creation_time());
  itemData.numberOfTabs = static_cast<NSUInteger>(numberOfTabs);

  return itemData;
}

- (void)fetchFaviconsForCell:(TabGroupsPanelCell*)cell {
  _faviconsGridConfigurator->ConfigureFaviconsGrid(cell.faviconsGrid,
                                                   cell.item.savedTabGroupID);
}

- (UIView*)facePileViewForItem:(TabGroupsPanelItem*)item {
  if (!_shareKitService || !_shareKitService->IsSupported() ||
      !_collaborationService || !_tabGroupSyncService) {
    return nil;
  }

  std::optional<tab_groups::SavedTabGroup> group =
      _tabGroupSyncService->GetGroup(item.savedTabGroupID);
  if (!group.has_value() || !group->collaboration_id().has_value()) {
    return nil;
  }
  NSString* savedCollabID =
      base::SysUTF8ToNSString(group->collaboration_id()->value());

  // Configure the face pile.
  ShareKitFacePileConfiguration* config =
      [[ShareKitFacePileConfiguration alloc] init];
  config.collabID = savedCollabID;
  config.showsEmptyState = NO;
  config.avatarSize = kFacePileAvatarSize;

  return _shareKitService->FacePileView(config);
}

#pragma mark TabGroupsPanelMutator

- (void)selectTabGroupsPanelItem:(TabGroupsPanelItem*)item {
  [self.delegate tabGroupsPanelMediator:self
                    openGroupWithSyncID:item.savedTabGroupID];
}

- (void)deleteTabGroupsPanelItem:(TabGroupsPanelItem*)item
                      sourceView:(UIView*)sourceView {
  [self.delegate tabGroupsPanelMediator:self
      showDeleteGroupConfirmationWithSyncID:item.savedTabGroupID
                                 sourceView:sourceView];
}

- (void)leaveSharedTabGroupsPanelItem:(TabGroupsPanelItem*)item
                           sourceView:(UIView*)sourceView {
  std::optional<tab_groups::SavedTabGroup> group =
      _tabGroupSyncService->GetGroup(item.savedTabGroupID);
  if (!group) {
    return;
  }

  [self.delegate tabGroupsPanelMediator:self
      startLeaveOrDeleteSharedGroupWithSyncID:item.savedTabGroupID
                                   groupTitle:base::SysUTF16ToNSString(
                                                  group->title())
                                    forAction:TabGroupActionType::
                                                  kLeaveSharedTabGroup
                                   sourceView:sourceView];
}

- (void)deleteSharedTabGroupsPanelItem:(TabGroupsPanelItem*)item
                            sourceView:(UIView*)sourceView {
  std::optional<tab_groups::SavedTabGroup> group =
      _tabGroupSyncService->GetGroup(item.savedTabGroupID);
  if (!group) {
    return;
  }

  [self.delegate tabGroupsPanelMediator:self
      startLeaveOrDeleteSharedGroupWithSyncID:item.savedTabGroupID
                                   groupTitle:base::SysUTF16ToNSString(
                                                  group->title())
                                    forAction:TabGroupActionType::
                                                  kDeleteSharedTabGroup
                                   sourceView:sourceView];
}

- (void)deleteNotificationItem:(TabGroupsPanelItem*)item {
  // The user has dismissed the summary card displaying all the "group removed"
  // messages. Dismissing it should clear all messages on the backend.
  std::vector<collaboration::messaging::PersistentMessage> messages =
      _messagingService->GetMessages(
          collaboration::messaging::PersistentNotificationType::TOMBSTONED);
  for (const collaboration::messaging::PersistentMessage& message : messages) {
    if (!message.attribution.id.has_value()) {
      continue;
    }
    _messagingService->ClearPersistentMessage(
        message.attribution.id.value(),
        collaboration::messaging::PersistentNotificationType::TOMBSTONED);
  }

  [self populateItemsFromServices];
}

#pragma mark TabGroupSyncServiceObserverDelegate

- (void)tabGroupSyncServiceInitialized {
  _tabGroupSyncServiceInitialized = true;
  [self populateItemsFromServices];
}

- (void)tabGroupSyncServiceTabGroupAdded:(const tab_groups::SavedTabGroup&)group
                              fromSource:(tab_groups::TriggerSource)source {
  [self populateItemsFromServices];
}

- (void)tabGroupSyncServiceTabGroupUpdated:
            (const tab_groups::SavedTabGroup&)group
                                fromSource:(tab_groups::TriggerSource)source {
  [self reconfigureGroup:group];
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
  [self populateItemsFromServices];
}

- (void)tabGroupSyncServiceTabGroupMigrated:
            (const tab_groups::SavedTabGroup&)newGroup
                                  oldSyncID:(const base::Uuid&)oldSync
                                 fromSource:(tab_groups::TriggerSource)source {
  [self populateItemsFromServices];
}

- (void)tabGroupSyncServiceSavedTabGroupLocalIdChanged:(const base::Uuid&)syncID
                                               localID:
                                                   (const std::optional<
                                                       tab_groups::
                                                           LocalTabGroupID>&)
                                                       localID {
  [self populateItemsFromServices];
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
  // Done button is enabled if there is at least one Regular tab.
  toolbarsConfiguration.doneButton =
      _regularWebStateList && !_regularWebStateList->empty();
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

// Reads the TabGroupSyncService data, prepares it, and feeds it to the
// consumer.
- (void)populateItemsFromServices {
  if (_tabGroupSyncServiceInitialized) {
    std::vector<collaboration::messaging::PersistentMessage> messages;
    if (_messagingService && _messagingService->IsInitialized()) {
      messages = _messagingService->GetMessages(
          collaboration::messaging::PersistentNotificationType::TOMBSTONED);
    }
    NSArray<TabGroupsPanelItem*>* tabGroupItems = [self createTabGroupItems];
    [_consumer populateNotificationItem:CreateNotificationItem(messages)
                          tabGroupItems:tabGroupItems];
  }
}

// Tells the consumer to reconfigure the group that matches the given `groupId`.
- (void)reconfigureGroupForGroupId:(const data_sharing::GroupId&)groupId {
  if (!_tabGroupSyncServiceInitialized) {
    return;
  }

  tab_groups::CollaborationId collaborationId =
      tab_groups::CollaborationId(groupId.value());
  std::vector<tab_groups::SavedTabGroup> groups =
      _tabGroupSyncService->GetAllGroups();

  // Find the matching group and reconfigure the item.
  for (const auto& group : groups) {
    if (!group.collaboration_id().has_value() ||
        group.collaboration_id().value() != collaborationId) {
      continue;
    }
    [self reconfigureGroup:group];
    break;
  }
}

// Tells the consumer to reload the given group.
- (void)reconfigureGroup:(const tab_groups::SavedTabGroup&)group {
  if (!_tabGroupSyncServiceInitialized) {
    return;
  }
  TabGroupsPanelItem* item = [[TabGroupsPanelItem alloc]
      initWithSavedTabGroupID:group.saved_guid()
                 sharingState:[self sharingStateForGroup:group]];
  [_consumer reconfigureItem:item];
}

// Returns an array of `TabGroupsPanelItem`.
- (NSArray<TabGroupsPanelItem*>*)createTabGroupItems {
  std::vector<tab_groups::SavedTabGroup> groups =
      _tabGroupSyncService->GetAllGroups();
  // Sort groups by creation date.
  std::sort(groups.begin(), groups.end(), CompareGroupByCreationDate);

  NSMutableArray<TabGroupsPanelItem*>* items = [[NSMutableArray alloc] init];
  for (const auto& group : groups) {
    TabGroupsPanelItem* item = [[TabGroupsPanelItem alloc]
        initWithSavedTabGroupID:group.saved_guid()
                   sharingState:[self sharingStateForGroup:group]];
    [items addObject:item];
  }
  return items;
}

// Returns the `SharingState` for the given `group`.
- (SharingState)sharingStateForGroup:(const tab_groups::SavedTabGroup&)group {
  BOOL isSharedTabGroupSupported =
      _shareKitService && _shareKitService->IsSupported();

  if (!isSharedTabGroupSupported || !group.collaboration_id().has_value()) {
    return SharingState::kNotShared;
  }

  data_sharing::GroupId groupId =
      data_sharing::GroupId(group.collaboration_id().value().value());
  data_sharing::MemberRole userRole =
      _collaborationService->GetCurrentUserRoleForGroup(groupId);
  return userRole == data_sharing::MemberRole::kOwner
             ? SharingState::kSharedAndOwned
             : SharingState::kShared;
}

// Takes the corresponded action to `actionType` for the shared `groupSyncID`.
// TabGroupActionType must be kLeaveSharedTabGroup or kDeleteSharedTabGroup.
- (void)takeActionForActionType:(TabGroupActionType)actionType
                 sharedTabGroup:(const base::Uuid&)groupSyncID {
  std::optional<tab_groups::SavedTabGroup> group =
      _tabGroupSyncService->GetGroup(groupSyncID);
  if (!group || !group.has_value() || !group->collaboration_id().has_value()) {
    return;
  }

  const tab_groups::CollaborationId collabId =
      group->collaboration_id().value();
  const data_sharing::GroupId groupId = data_sharing::GroupId(collabId.value());

  __weak TabGroupsPanelMediator* weakSelf = self;
  auto callback = base::BindOnce(^(bool success) {
    [weakSelf handleTakeActionForActionTypeOutcome:success];
  });

  // TODO(crbug.com/393073658): Block the screen.

  // Asynchronously call on the server.
  switch (actionType) {
    case TabGroupActionType::kLeaveSharedTabGroup:
      _collaborationService->LeaveGroup(groupId, std::move(callback));
      break;
    case TabGroupActionType::kDeleteSharedTabGroup:
      _collaborationService->DeleteGroup(groupId, std::move(callback));
      break;
    case TabGroupActionType::kUngroupTabGroup:
    case TabGroupActionType::kDeleteTabGroup:
    case TabGroupActionType::kLeaveOrKeepSharedTabGroup:
    case TabGroupActionType::kDeleteOrKeepSharedTabGroup:
      NOTREACHED();
  }
}

// Called when `takeActionForActionType:forSharedTabGroup:` server's call
// returned.
- (void)handleTakeActionForActionTypeOutcome:(BOOL)success {
  // TODO(crbug.com/393073658):
  // - Unblock the screen.
  // - Show an error if needed.
}

@end
