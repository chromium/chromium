// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "components/collaboration/public/collaboration_service.h"
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
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/ui/tab_group_indicator_consumer.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"

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
    CHECK(IsTabGroupIndicatorEnabled());
    _URLLoader = URLLoader;
    _shareKitService = shareKitService;
    _collaborationService = collaborationService;
    _tabGroupSyncService = tabGroupSyncService;
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
    if (tabGroup) {
      [_consumer setTabGroupTitle:tabGroup->GetTitle()
                       groupColor:tabGroup->GetColor()];
      BOOL shared =
          tab_groups::utils::IsTabGroupShared(tabGroup, _tabGroupSyncService);
      [_consumer setShared:shared];
    } else {
      [_consumer setTabGroupTitle:nil groupColor:nil];
      [_consumer setShared:NO];
    }
    [self updateFacePileUI];
  }
}

#pragma mark - TabGroupIndicatorMutator

- (void)shareGroup {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup || !_shareKitService) {
    return;
  }

  ShareKitShareGroupConfiguration* config =
      [[ShareKitShareGroupConfiguration alloc] init];
  config.tabGroup = tabGroup;
  config.baseViewController = self.baseViewController;
  config.applicationHandler = self.applicationHandler;
  _shareKitService->ShareTabGroup(config);
}

- (void)showRecentActivity {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  [_delegate showRecentActivityForGroup:tabGroup->GetWeakPtr()];
}

- (void)manageGroup {
  const TabGroup* tabGroup = [self currentTabGroup];
  NSString* collabID =
      tab_groups::utils::GetTabGroupCollabID(tabGroup, _tabGroupSyncService);
  if (!_shareKitService || !collabID) {
    return;
  }

  ShareKitManageConfiguration* config =
      [[ShareKitManageConfiguration alloc] init];
  config.baseViewController = self.baseViewController;
  config.collabID = collabID;
  config.applicationHandler = self.applicationHandler;
  _shareKitService->ManageTabGroup(config);
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
    [_delegate showTabGroupIndicatorConfirmationForAction:TabGroupActionType::
                                                              kUngroupTabGroup];
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
    [_delegate showTabGroupIndicatorConfirmationForAction:TabGroupActionType::
                                                              kDeleteTabGroup];
    return;
  }
  [self closeTabGroup:tabGroup andDeleteGroup:YES];
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
  [self updateFacePileUI];
}

#pragma mark - Private

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

  NSString* savedCollabID =
      tab_groups::utils::GetTabGroupCollabID(tabGroup, _tabGroupSyncService);
  BOOL isShared = savedCollabID != nil;
  [_consumer setShared:isShared];

  // Prevent the face pile from being set up for tab groups that are not shared.
  if (!isShared) {
    [_consumer setFacePileViewController:nil];
  }

  // Configure the face pile.
  ShareKitFacePileConfiguration* config =
      [[ShareKitFacePileConfiguration alloc] init];
  config.collabID = savedCollabID;
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

@end
