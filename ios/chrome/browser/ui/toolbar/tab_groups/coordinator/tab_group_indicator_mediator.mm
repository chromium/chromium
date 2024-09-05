// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_mediator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_action_type.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_consumer.h"

@interface TabGroupIndicatorMediator () <WebStateListObserving>
@end

@implementation TabGroupIndicatorMediator {
  __weak id<TabGroupIndicatorConsumer> _consumer;
  base::WeakPtr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

- (instancetype)initWithConsumer:(id<TabGroupIndicatorConsumer>)consumer
                    webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(webStateList);
    CHECK(IsTabGroupIndicatorEnabled());
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

- (void)unGroup {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  [_delegate showTabGroupIndicatorConfirmationForAction:TabGroupActionType::
                                                            kUngroupTabGroup];
}

- (void)closeGroup {
  const TabGroup* tabGroup = [self currentTabGroup];
  if (!tabGroup) {
    return;
  }
  [_delegate showTabGroupIndicatorConfirmationForAction:TabGroupActionType::
                                                            kDeleteTabGroup];
}

#pragma mark - Private

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
  // TODO(crbug.com/361499394): Implement this.
}

@end
