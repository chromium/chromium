// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_mediator.h"

#import "base/memory/weak_ptr.h"
#import "components/saved_tab_groups/tab_group_sync_service.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_action_type.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_mediator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_consumer.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

@interface TabGroupIndicatorMediator () <WebStateListObserving>
@end

@implementation TabGroupIndicatorMediator {
  ProfileIOS* _profile;
  raw_ptr<tab_groups::TabGroupSyncService> _tabGroupSyncService;
  __weak id<TabGroupIndicatorConsumer> _consumer;
  base::WeakPtr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

- (instancetype)initWithProfile:(ProfileIOS*)profile
            tabGroupSyncService:
                (tab_groups::TabGroupSyncService*)tabGroupSyncService
                       consumer:(id<TabGroupIndicatorConsumer>)consumer
                   webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    CHECK(profile);
    CHECK(consumer);
    CHECK(webStateList);
    CHECK(IsTabGroupIndicatorEnabled());
    _profile = profile;
    _tabGroupSyncService = tabGroupSyncService;
    _consumer = consumer;
    _webStateList = webStateList->AsWeakPtr();
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateList = nullptr;
  }
  _webStateListObserver.reset();
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
    } else {
      [_consumer setTabGroupTitle:nil groupColor:nil];
    }
  }
}

#pragma mark - TabGroupIndicatorMutator

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

  const auto insertionParams =
      WebStateList::InsertionParams::Automatic().InGroup(tabGroup);
  [self insertAndActivateNewWebStateWithInsertionParams:insertionParams];
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

#pragma mark - Private

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
  if (!_webStateList) {
    return nil;
  }
  return _webStateList->GetGroupOfWebStateAt(_webStateList->active_index());
}

// Inserts and activate a new WebState opened at `kChromeUINewTabURL` using
// `insertionParams`.
- (void)insertAndActivateNewWebStateWithInsertionParams:
    (WebStateList::InsertionParams)insertionParams {
  CHECK(_webStateList);
  CHECK(_profile);

  if (!IsAddNewTabAllowedByPolicy(_profile->GetPrefs(),
                                  _profile->IsOffTheRecord())) {
    return;
  }

  web::WebState::CreateParams params(_profile);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  GURL url(kChromeUINewTabURL);
  web::NavigationManager::WebLoadParams loadParams(url);
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  _webStateList->InsertWebState(std::move(webState),
                                insertionParams.Activate());
}

@end
