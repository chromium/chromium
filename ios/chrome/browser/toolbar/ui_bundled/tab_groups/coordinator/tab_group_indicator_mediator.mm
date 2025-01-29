// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/share_kit/model/share_kit_face_pile_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
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

using PeopleGroupActionOutcome =
    data_sharing::DataSharingService::PeopleGroupActionOutcome;
using ScopedTabGroupSyncObservation =
    base::ScopedObservation<tab_groups::TabGroupSyncService,
                            tab_groups::TabGroupSyncService::Observer>;

namespace {
// The preferred size in points for the avatar icons.
constexpr CGFloat kFacePileAvatarSize = 20;
}  // namespace

@interface TabGroupIndicatorMediator () <TabGroupSyncServiceObserverDelegate,
                                         WebStateListObserving>
@end

@implementation TabGroupIndicatorMediator {
  raw_ptr<collaboration::CollaborationService> _collaborationService;
  raw_ptr<ShareKitService> _shareKitService;
  raw_ptr<tab_groups::TabGroupSyncService> _tabGroupSyncService;
  raw_ptr<data_sharing::DataSharingService> _dataSharingService;
  raw_ptr<feature_engagement::Tracker> _tracker;
  // The bridge between the service C++ observer and this Objective-C class.
  std::unique_ptr<TabGroupSyncServiceObserverBridge> _syncServiceObserver;
  std::unique_ptr<ScopedTabGroupSyncObservation> _scopedSyncServiceObservation;
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
      _syncServiceObserver =
          std::make_unique<TabGroupSyncServiceObserverBridge>(self);
      _scopedSyncServiceObservation =
          std::make_unique<ScopedTabGroupSyncObservation>(
              _syncServiceObserver.get());
      _scopedSyncServiceObservation->Observe(_tabGroupSyncService);
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
  _scopedSyncServiceObservation.reset();
  _syncServiceObserver.reset();
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
                       groupColor:tabGroup->GetColor()];
      [self updateTabGroupSharedState:tabGroup];
    } else {
      [_consumer setTabGroupTitle:nil groupColor:nil];
      [_consumer setShared:NO owner:NO];
    }
    [self updateFacePileUI];
  }
}

#pragma mark - TabGroupIndicatorMutator

- (void)shareGroup {
  [self.delegate shareOrManageTabGroup:[self currentTabGroup]];
}

- (void)showRecentActivity {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  [_delegate showRecentActivityForGroup:tabGroup->GetWeakPtr()];
}

- (void)manageGroup {
  [self.delegate shareOrManageTabGroup:[self currentTabGroup]];
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
    [_delegate
        showTabGroupIndicatorConfirmationForAction:TabGroupActionType::
                                                       kDeleteSharedTabGroup
                                             group:tabGroup->GetWeakPtr()];
    return;
  }
  [self takeActionForActionType:TabGroupActionType::kDeleteSharedTabGroup
                 sharedTabGroup:tabGroup];
}

- (void)leaveSharedGroupWithConfirmation:(BOOL)confirmation {
  DCHECK(IsTabGroupSyncEnabled());
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  if (confirmation) {
    [_delegate
        showTabGroupIndicatorConfirmationForAction:TabGroupActionType::
                                                       kLeaveSharedTabGroup
                                             group:tabGroup->GetWeakPtr()];
    return;
  }
  [self takeActionForActionType:TabGroupActionType::kLeaveSharedTabGroup
                 sharedTabGroup:tabGroup];
}

- (void)updateSharedState {
  const TabGroup* tabGroup = [self currentTabGroup];
  [self updateTabGroupSharedState:tabGroup];
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
  [self updateTabGroupSharedState:tabGroup];
  [self updateFacePileUI];
}

#pragma mark - Private

// Takes the corresponded action to `actionType` for the shared `group`.
// Not handled TabGroupActionType: kUngroupTabGroup, kDeleteTabGroup.
- (void)takeActionForActionType:(TabGroupActionType)actionType
                 sharedTabGroup:(const TabGroup*)group {
  CHECK(_dataSharingService);

  const base::Uuid savedGroupId =
      _tabGroupSyncService->GetGroup(group->tab_group_id())->saved_guid();
  const tab_groups::CollaborationId collabId =
      tab_groups::utils::GetTabGroupCollabID(group, _tabGroupSyncService);
  CHECK(!collabId->empty());
  const data_sharing::GroupId groupId = data_sharing::GroupId(collabId.value());

  __weak TabGroupIndicatorMediator* weakSelf = self;
  auto callback = base::BindOnce(^(PeopleGroupActionOutcome outcome) {
    BOOL success = outcome == PeopleGroupActionOutcome::kSuccess;
    [weakSelf handTakeActionForActionTypeOutcome:success];
  });

  // TODO(crbug.com/393073658): Block the screen.

  // Asynchronously call on the server.
  switch (actionType) {
    case TabGroupActionType::kLeaveSharedTabGroup:
      _dataSharingService->LeaveGroup(groupId, std::move(callback));
      break;
    case TabGroupActionType::kDeleteSharedTabGroup:
      _dataSharingService->DeleteGroup(groupId, std::move(callback));
      break;
    case TabGroupActionType::kUngroupTabGroup:
    case TabGroupActionType::kDeleteTabGroup:
    case TabGroupActionType::kLeaveOrKeepSharedTabGroup:
    case TabGroupActionType::kDeleteOrKeepSharedTabGroup:
      NOTREACHED();
  }
}

// Called when `performAction:forSharedTabGroup:` server's call returned.
- (void)handTakeActionForActionTypeOutcome:(BOOL)success {
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
  [self updateTabGroupSharedState:tabGroup];

  // Prevent the face pile from being set up for tab groups that are not shared.
  if (!isShared) {
    [_consumer setFacePileViewController:nil];
  }

  // Configure the face pile.
  ShareKitFacePileConfiguration* config =
      [[ShareKitFacePileConfiguration alloc] init];
  config.collabID = base::SysUTF8ToNSString(savedCollabID.value());
  config.showsEmptyState = NO;
  config.avatarSize = kFacePileAvatarSize;

  [_consumer setFacePileViewController:_shareKitService->FacePile(config)];
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

// Updates the shared state of for the given `tabGroup`.
- (void)updateTabGroupSharedState:(const TabGroup*)tabGroup {
  BOOL shared =
      tab_groups::utils::IsTabGroupShared(tabGroup, _tabGroupSyncService);
  data_sharing::MemberRole userRole = tab_groups::utils::GetUserRoleForGroup(
      tabGroup, _tabGroupSyncService, _collaborationService);
  [_consumer setShared:shared
                 owner:userRole == data_sharing::MemberRole::kOwner];
}

@end
