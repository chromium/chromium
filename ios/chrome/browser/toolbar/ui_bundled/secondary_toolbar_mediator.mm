// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/secondary_toolbar_mediator.h"

#import "base/memory/weak_ptr.h"
#import "components/collaboration/public/messaging/message.h"
#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_bridge.h"
#import "ios/chrome/browser/contextual_panel/model/active_contextual_panel_tab_helper_observation_forwarder.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_tab_group_state.h"
#import "ios/chrome/browser/toolbar/ui_bundled/secondary_toolbar_consumer.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns a local tab group ID in `message`. Returns nullopt if the ID doesn't
// exist.
std::optional<tab_groups::LocalTabGroupID> LocalTabGroupID(
    collaboration::messaging::PersistentMessage message) {
  if (!message.attribution.tab_group_metadata.has_value()) {
    return std::nullopt;
  }
  collaboration::messaging::TabGroupMessageMetadata group_data =
      message.attribution.tab_group_metadata.value();
  return group_data.local_tab_group_id;
}

}  // namespace

@interface SecondaryToolbarMediator () <ContextualPanelTabHelperObserving,
                                        MessagingBackendServiceObserving,
                                        WebStateListObserving>

@end

@implementation SecondaryToolbarMediator {
  // The Browser's WebStateList.
  base::WeakPtr<WebStateList> _webStateList;

  // Observer machinery for the web state list.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;

  // Bridge for the ContextualPanelTabHelper observation.
  std::unique_ptr<ContextualPanelTabHelperObserverBridge>
      _contextualPanelObserverBridge;

  // Forwarder to always be observing the active ContextualPanelTabHelper.
  std::unique_ptr<ActiveContextualPanelTabHelperObservationForwarder>
      _activeContextualPanelObservationForwarder;

  // A service to get activity messages for a shared tab group.
  raw_ptr<collaboration::messaging::MessagingBackendService> _messagingService;
  // The bridge between the C++ MessagingBackendService observer and this
  // Objective-C class.
  std::unique_ptr<MessagingBackendServiceBridge> _messagingBackendServiceBridge;
  // A set of a shared group ID that has changed and a user has not seen it yet.
  std::set<tab_groups::LocalTabGroupID> _dirtyGroups;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                    messagingService:
                        (collaboration::messaging::MessagingBackendService*)
                            messagingService {
  if ((self = [super init])) {
    DCHECK(webStateList);
    _webStateList = webStateList->AsWeakPtr();

    // Web state list observation is only necessary for Contextual Panel
    // feature.
    if (IsContextualPanelEnabled()) {
      // Set up web state list observation.
      _webStateListObserver =
          std::make_unique<WebStateListObserverBridge>(self);
      _webStateListObservation = std::make_unique<
          base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
          _webStateListObserver.get());
      _webStateListObservation->Observe(_webStateList.get());

      // Set up active ContextualPanelTabHelper observation.
      _contextualPanelObserverBridge =
          std::make_unique<ContextualPanelTabHelperObserverBridge>(self);
      _activeContextualPanelObservationForwarder =
          std::make_unique<ActiveContextualPanelTabHelperObservationForwarder>(
              webStateList, _contextualPanelObserverBridge.get());
    }

    _messagingService = messagingService;
    if (_messagingService) {
      _messagingBackendServiceBridge =
          std::make_unique<MessagingBackendServiceBridge>(self);
      _messagingService->AddPersistentMessageObserver(
          _messagingBackendServiceBridge.get());
      [self fetchMessages];
    }
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _activeContextualPanelObservationForwarder.reset();
    _webStateListObservation.reset();
    _webStateList.reset();
  }
  if (_messagingService) {
    _messagingService->RemovePersistentMessageObserver(
        _messagingBackendServiceBridge.get());
    _messagingBackendServiceBridge.reset();
  }
  _contextualPanelObserverBridge.reset();
  _webStateListObserver.reset();
}

#pragma mark - SecondaryToolbarKeyboardStateProvider

- (BOOL)keyboardIsActiveForWebContent {
  if (_webStateList && _webStateList->GetActiveWebState()) {
    return _webStateList->GetActiveWebState()
        ->GetWebViewProxy()
        .keyboardVisible;
  }
  return NO;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  // Update the Tab Grid button style, based on whether the active tab is
  // grouped or not.
  if (IsTabGroupIndicatorEnabled()) {
    [self.consumer updateTabGroupState:[self tabGroupStateToDisplay]];
  }

  // Return early if the active web state is the same as before the change.
  if (!status.active_web_state_change()) {
    return;
  }

  // Return early if no new webstates are active.
  if (!status.new_active_web_state) {
    return;
  }
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(status.new_active_web_state);
  if (!contextualPanelTabHelper) {
    return;
  }

  if (contextualPanelTabHelper->IsContextualPanelCurrentlyOpened()) {
    [self.consumer makeTranslucent];
  } else {
    [self.consumer makeOpaque];
  }
}

#pragma mark - ContextualPanelTabHelperObserving

- (void)contextualPanelOpened:(ContextualPanelTabHelper*)tabHelper {
  [self.consumer makeTranslucent];
}

- (void)contextualPanelClosed:(ContextualPanelTabHelper*)tabHelper {
  [self.consumer makeOpaque];
}

#pragma mark - MessagingBackendServiceObserving

- (void)onMessagingBackendServiceInitialized {
  [self fetchMessages];
}

- (void)displayPersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());

  if (message.type !=
      collaboration::messaging::PersistentNotificationType::DIRTY_TAB_GROUP) {
    return;
  }

  if (std::optional<tab_groups::LocalTabGroupID> localTabGroupID =
          LocalTabGroupID(message)) {
    _dirtyGroups.insert(*localTabGroupID);
  }

  [self updateTabGridButtonBlueDot];
}

- (void)hidePersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());

  if (message.type !=
      collaboration::messaging::PersistentNotificationType::DIRTY_TAB_GROUP) {
    return;
  }

  if (std::optional<tab_groups::LocalTabGroupID> localTabGroupID =
          LocalTabGroupID(message)) {
    _dirtyGroups.erase(*localTabGroupID);
  }

  [self updateTabGridButtonBlueDot];
}

#pragma mark - Private

// Returns the tab group of the active web state, if any.
- (const TabGroup*)activeWebStateTabGroup {
  const int active_index = _webStateList->active_index();
  if (active_index != WebStateList::kInvalidIndex) {
    return _webStateList->GetGroupOfWebStateAt(active_index);
  }
  return nullptr;
}

// Returns the tab group state to display in the Tab Grid button.
- (ToolbarTabGroupState)tabGroupStateToDisplay {
  return [self activeWebStateTabGroup] != nullptr
             ? ToolbarTabGroupState::kTabGroup
             : ToolbarTabGroupState::kNormal;
}

// Updates the blue dot in the Tab Grid button depending on the messages and the
// current active web state.
- (void)updateTabGridButtonBlueDot {
  if ([self tabGroupStateToDisplay] == ToolbarTabGroupState::kNormal) {
    // Show the blue dot if there is at least one group that has been updated.
    [self.consumer setTabGridButtonBlueDot:_dirtyGroups.size() > 0];
    return;
  }

  // Show the blue dot if the current active group has been updated.
  CHECK([self tabGroupStateToDisplay] == ToolbarTabGroupState::kTabGroup);
  const TabGroup* activeGroup = [self activeWebStateTabGroup];
  [self.consumer setTabGridButtonBlueDot:_dirtyGroups.contains(
                                             activeGroup->tab_group_id())];
}

// Gets messages to indicate that a shared tab group has been changed.
- (void)fetchMessages {
  if (!_messagingService || !_messagingService->IsInitialized()) {
    return;
  }

  std::vector<collaboration::messaging::PersistentMessage> messages =
      _messagingService->GetMessages(
          collaboration::messaging::PersistentNotificationType::
              DIRTY_TAB_GROUP);

  for (auto& message : messages) {
    if (std::optional<tab_groups::LocalTabGroupID> localTabGroupID =
            LocalTabGroupID(message)) {
      _dirtyGroups.insert(*localTabGroupID);
    }
  }

  [self updateTabGridButtonBlueDot];
}

@end
