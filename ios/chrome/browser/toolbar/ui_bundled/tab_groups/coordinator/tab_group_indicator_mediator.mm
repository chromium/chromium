// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_flow_entry_point.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/data_sharing/public/group_data.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_observer_bridge.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"
#import "ios/chrome/browser/share_kit/model/share_kit_face_pile_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/share_kit/model/sharing_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_mediator_delegate.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/tab_group_indicator_features_utils.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/ui/tab_group_indicator_consumer.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"

using collaboration::CollaborationServiceShareOrManageEntryPoint;
using ScopedTabGroupSyncObservation =
    base::ScopedObservation<tab_groups::TabGroupSyncService,
                            tab_groups::TabGroupSyncService::Observer>;
using ScopedDataSharingSyncObservation =
    base::ScopedObservation<data_sharing::DataSharingService,
                            data_sharing::DataSharingService::Observer>;
using tab_groups::SharingState;

namespace {
// The preferred size in points for the avatar icons.
constexpr CGFloat kFacePileAvatarSize = 20;
}  // namespace

@interface TabGroupIndicatorMediator () <DataSharingServiceObserverDelegate,
                                         TabGroupSyncServiceObserverDelegate,
                                         WebStateListObserving>
@end

@implementation TabGroupIndicatorMediator {
  raw_ptr<collaboration::CollaborationService> _collaborationService;
  raw_ptr<ShareKitService> _shareKitService;
  raw_ptr<tab_groups::TabGroupSyncService> _tabGroupSyncService;
  raw_ptr<data_sharing::DataSharingService> _dataSharingService;
  raw_ptr<feature_engagement::Tracker> _tracker;

  // Bridges between C++ service observers and this Objective-C class.
  std::unique_ptr<TabGroupSyncServiceObserverBridge>
      _tabGroupSyncServiceObserver;
  std::unique_ptr<ScopedTabGroupSyncObservation>
      _scopedTabGroupSyncServiceObservation;
  std::unique_ptr<DataSharingServiceObserverBridge> _dataSharingServiceObserver;
  std::unique_ptr<ScopedDataSharingSyncObservation>
      _scopedDataSharingServiceObservation;

  // URL loader to open tabs when needed.
  raw_ptr<UrlLoadingBrowserAgent> _URLLoader;
  __weak id<TabGroupIndicatorConsumer> _consumer;
  base::WeakPtr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  BOOL _incognito;
}

- (instancetype)
    initWithTabGroupSyncService:
        (tab_groups::TabGroupSyncService*)tabGroupSyncService
                shareKitService:(ShareKitService*)shareKitService
           collaborationService:
               (collaboration::CollaborationService*)collaborationService
             dataSharingService:
                 (data_sharing::DataSharingService*)dataSharingService
                       consumer:(id<TabGroupIndicatorConsumer>)consumer
                   webStateList:(WebStateList*)webStateList
                      URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                 featureTracker:(feature_engagement::Tracker*)tracker
                      incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(webStateList);
    CHECK(tracker);
    _URLLoader = URLLoader;
    _shareKitService = shareKitService;
    _collaborationService = collaborationService;
    _tabGroupSyncService = tabGroupSyncService;
    _dataSharingService = dataSharingService;

    if (tabGroupSyncService) {
      _tabGroupSyncServiceObserver =
          std::make_unique<TabGroupSyncServiceObserverBridge>(self);
      _scopedTabGroupSyncServiceObservation =
          std::make_unique<ScopedTabGroupSyncObservation>(
              _tabGroupSyncServiceObserver.get());
      _scopedTabGroupSyncServiceObservation->Observe(_tabGroupSyncService);
    }

    if (dataSharingService) {
      _dataSharingServiceObserver =
          std::make_unique<DataSharingServiceObserverBridge>(self);
      _scopedDataSharingServiceObservation =
          std::make_unique<ScopedDataSharingSyncObservation>(
              _dataSharingServiceObserver.get());
      _scopedDataSharingServiceObservation->Observe(_dataSharingService);
    }

    _consumer = consumer;
    _tracker = tracker;
    _incognito = incognito;
    _webStateList = webStateList->AsWeakPtr();
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    BOOL shareAvailable = shareKitService && shareKitService->IsSupported();
    [_consumer setShareAvailable:shareAvailable];
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateList = nullptr;
  }
  _webStateListObserver.reset();
  _scopedTabGroupSyncServiceObservation.reset();
  _tabGroupSyncServiceObserver.reset();
  _scopedDataSharingServiceObservation.reset();
  _dataSharingServiceObserver.reset();
  _tabGroupSyncService = nullptr;
  _collaborationService = nullptr;
  _shareKitService = nullptr;
  _dataSharingService = nullptr;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  BOOL groupUpdate = NO;
  switch (change.type()) {
    case WebStateListChange::Type::kGroupVisualDataUpdate:
    case WebStateListChange::Type::kStatusOnly:
      groupUpdate = YES;
      break;
    default:
      break;
  }

  web::WebState* webState = status.new_active_web_state;
  if ((status.active_web_state_change() || groupUpdate) && webState) {
    const TabGroup* tabGroup = [self currentTabGroup];
    if (tabGroup && IsTabGroupIndicatorEnabled() &&
        HasTabGroupIndicatorVisible()) {
      [_consumer setTabGroupTitle:tabGroup->GetTitle()
                       groupColor:tab_groups::ColorForTabGroupColorId(
                                      tabGroup->GetColor())];
      [self updateTabGroupSharingState:tabGroup];
    } else {
      [_consumer setTabGroupTitle:nil groupColor:nil];
      [_consumer setSharingState:SharingState::kNotShared];
    }
    [self updateFacePileUI];
  }
}

#pragma mark - TabGroupIndicatorMutator

- (void)shareGroup {
  [self.delegate
      shareOrManageTabGroup:[self currentTabGroup]
                 entryPoint:CollaborationServiceShareOrManageEntryPoint::
                                kiOSTabGroupIndicatorShare];
}

- (void)showRecentActivity {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  [_delegate showRecentActivityForGroup:tabGroup->GetWeakPtr()];
}

- (void)manageGroup {
  [self.delegate
      shareOrManageTabGroup:[self currentTabGroup]
                 entryPoint:CollaborationServiceShareOrManageEntryPoint::
                                kiOSTabGroupIndicatorManage];
}

- (void)showTabGroupEdition {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  [_delegate showTabGroupIndicatorEditionForGroup:tabGroup->GetWeakPtr()];
}

- (void)addNewTabInGroup {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }

  GURL URL(kChromeUINewTabURL);
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.in_incognito = _incognito;
  params.load_in_group = true;
  params.tab_group = tabGroup->GetWeakPtr();
  _URLLoader->Load(params);
}

- (void)closeGroup {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  [self closeTabGroup:tabGroup andDeleteGroup:NO];
}

- (void)unGroupWithConfirmation:(BOOL)confirmation {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  if (confirmation) {
    [_delegate
        showTabGroupIndicatorConfirmationForAction:TabGroupActionType::
                                                       kUngroupTabGroup
                                             group:tabGroup->GetWeakPtr()];
    return;
  }
  _webStateList->DeleteGroup(tabGroup);
}

- (void)deleteGroupWithConfirmation:(BOOL)confirmation {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  if (IsTabGroupSyncEnabled() && confirmation) {
    [_delegate
        showTabGroupIndicatorConfirmationForAction:TabGroupActionType::
                                                       kDeleteTabGroup
                                             group:tabGroup->GetWeakPtr()];
    return;
  }
  [self closeTabGroup:tabGroup andDeleteGroup:YES];
}

- (void)deleteSharedGroupWithConfirmation:(BOOL)confirmation {
  DCHECK(IsTabGroupSyncEnabled());
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  if (confirmation) {
    [_delegate startLeaveOrDeleteSharedGroup:tabGroup->GetWeakPtr()
                                   forAction:TabGroupActionType::
                                                 kDeleteSharedTabGroup];
    return;
  }
  [self takeActionForActionType:TabGroupActionType::kDeleteSharedTabGroup
                 sharedTabGroup:tabGroup];
}

- (void)leaveSharedGroupWithConfirmation:(BOOL)confirmation {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  if (confirmation) {
    [_delegate
        startLeaveOrDeleteSharedGroup:tabGroup->GetWeakPtr()
                            forAction:TabGroupActionType::kLeaveSharedTabGroup];
    return;
  }
  [self takeActionForActionType:TabGroupActionType::kLeaveSharedTabGroup
                 sharedTabGroup:tabGroup];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (!_tabGroupSyncService) {
    return;
  }
  if (level < SceneActivationLevelForegroundActive) {
    return;
  }
  [self presentForegroundIPHIfNeeded];
}

#pragma mark TabGroupSyncServiceObserverDelegate

- (void)tabGroupSyncServiceInitialized {
  [self presentForegroundIPHIfNeeded];
  [self updateTabGroupSharingState:[self currentTabGroup]];
  [self updateFacePileUI];
}

- (void)tabGroupSyncServiceTabGroupMigrated:
            (const tab_groups::SavedTabGroup&)newGroup
                                  oldSyncID:(const base::Uuid&)oldSyncId
                                 fromSource:(tab_groups::TriggerSource)source {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup || newGroup.local_group_id() != tabGroup->tab_group_id()) {
    return;
  }
  [self updateTabGroupSharingState:tabGroup];
  [self updateFacePileUI];
}

#pragma mark DataSharingServiceObserverDelegate

- (void)dataSharingServiceInitialized {
  [self presentForegroundIPHIfNeeded];
  [self updateTabGroupSharingState:[self currentTabGroup]];
  [self updateFacePileUI];
}

- (void)dataSharingServiceDidAddGroup:(const data_sharing::GroupData&)groupData
                               atTime:(base::Time)eventTime {
  [self handleDataSharingUpdateForGroupId:groupData.group_token.group_id];
}

- (void)dataSharingServiceDidRemoveGroup:(const data_sharing::GroupId&)groupId
                                  atTime:(base::Time)eventTime {
  [self handleDataSharingUpdateForGroupId:groupId];
}

- (void)dataSharingServiceDidAddMember:(const GaiaId&)memberId
                               toGroup:(const data_sharing::GroupId&)groupId
                                atTime:(base::Time)eventTime {
  [self handleDataSharingUpdateForGroupId:groupId];
}

- (void)dataSharingServiceDidRemoveMember:(const GaiaId&)memberId
                                  toGroup:(const data_sharing::GroupId&)groupId
                                   atTime:(base::Time)eventTime {
  [self handleDataSharingUpdateForGroupId:groupId];
}

#pragma mark - Private

// Updates the consumer after a data sharing service update for the current tab
// group. `groupId` The ID of the group that was updated.
- (void)handleDataSharingUpdateForGroupId:
    (const data_sharing::GroupId&)groupId {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }

  std::optional<tab_groups::SavedTabGroup> savedGroup =
      _tabGroupSyncService->GetGroup(tabGroup->tab_group_id());

  // Early return if the current group is not shared.
  if (!savedGroup || !savedGroup->collaboration_id().has_value()) {
    return;
  }

  // Group Ids doesn't match.
  if (savedGroup->collaboration_id().value() !=
      tab_groups::CollaborationId(groupId.value())) {
    return;
  }

  [self updateTabGroupSharingState:tabGroup];
  [self updateFacePileUI];
}

// Takes the corresponded action to `actionType` for the shared `group`.
// TabGroupActionType must be kLeaveSharedTabGroup or kDeleteSharedTabGroup.
- (void)takeActionForActionType:(TabGroupActionType)actionType
                 sharedTabGroup:(const TabGroup*)group {
  CHECK(_collaborationService);

  const tab_groups::CollaborationId collabId =
      tab_groups::utils::GetTabGroupCollabID(group, _tabGroupSyncService);
  CHECK(!collabId->empty());
  const data_sharing::GroupId groupId = data_sharing::GroupId(collabId.value());

  __weak TabGroupIndicatorMediator* weakSelf = self;
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

// Tries to present the IPH to be presented when the app is foregrounded with a
// shared tab group visible.
- (void)presentForegroundIPHIfNeeded {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup ||
      !tab_groups::utils::IsTabGroupShared(tabGroup, _tabGroupSyncService)) {
    return;
  }
  if (_tracker->WouldTriggerHelpUI(
          feature_engagement::kIPHiOSSharedTabGroupForeground)) {
    [self.delegate showIPHForSharedTabGroupForegrounded];
  }
}

// Updates the facePile UI and the share state of the consumer.
- (void)updateFacePileUI {
  if (!_shareKitService || !_shareKitService->IsSupported() ||
      !_collaborationService || !_tabGroupSyncService) {
    return;
  }

  const TabGroup* tabGroup = [self currentTabGroup];

  tab_groups::CollaborationId savedCollabID =
      tab_groups::utils::GetTabGroupCollabID(tabGroup, _tabGroupSyncService);
  BOOL isShared = !savedCollabID.value().empty();
  [self updateTabGroupSharingState:tabGroup];

  // Prevent the face pile from being set up for tab groups that are not shared.
  if (!isShared) {
    [_consumer setFacePileView:nil];
  }

  // Configure the face pile.
  ShareKitFacePileConfiguration* config =
      [[ShareKitFacePileConfiguration alloc] init];
  config.collabID = base::SysUTF8ToNSString(savedCollabID.value());
  config.showsEmptyState = NO;
  config.avatarSize = kFacePileAvatarSize;

  [_consumer setFacePileView:_shareKitService->FacePileView(config)];
}

// Closes all tabs in `tabGroup`. If `deleteGroup` is false, the group is closed
// locally.
- (void)closeTabGroup:(const TabGroup*)tabGroup
       andDeleteGroup:(BOOL)deleteGroup {
  if (IsTabGroupSyncEnabled() && !deleteGroup) {
    [_delegate showTabGroupIndicatorSnackbarAfterClosingGroup];
    tab_groups::utils::CloseTabGroupLocally(tabGroup, _webStateList.get(),
                                            _tabGroupSyncService);
  } else {
    // Using `CloseAllWebStatesInGroup` will result in calling the web state
    // list observers which will take care of updating the consumer.
    CloseAllWebStatesInGroup(*_webStateList, tabGroup,
                             WebStateList::CLOSE_USER_ACTION);
  }
}

// Returns the current tab group.
- (const TabGroup*)currentTabGroup {
  if (!_webStateList ||
      _webStateList->active_index() == WebStateList::kInvalidIndex) {
    return nullptr;
  }
  return _webStateList->GetGroupOfWebStateAt(_webStateList->active_index());
}

// Updates the sharing state for the given `tabGroup`.
- (void)updateTabGroupSharingState:(const TabGroup*)tabGroup {
  BOOL shared =
      tab_groups::utils::IsTabGroupShared(tabGroup, _tabGroupSyncService);
  if (!shared) {
    [_consumer setSharingState:SharingState::kNotShared];
    return;
  }

  data_sharing::MemberRole userRole = tab_groups::utils::GetUserRoleForGroup(
      tabGroup, _tabGroupSyncService, _collaborationService);

  SharingState state = userRole == data_sharing::MemberRole::kOwner
                           ? SharingState::kSharedAndOwned
                           : SharingState::kShared;
  [_consumer setSharingState:state];
}

@end
