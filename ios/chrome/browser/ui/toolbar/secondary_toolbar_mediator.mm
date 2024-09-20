// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_mediator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/contextual_panel/model/active_contextual_panel_tab_helper_observation_forwarder.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button_style.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_consumer.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

@interface SecondaryToolbarMediator () <ContextualPanelTabHelperObserving,
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
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
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
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _activeContextualPanelObservationForwarder.reset();
    _webStateListObservation.reset();
    _webStateList.reset();
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
  if (IsTabGroupIndicatorEnabled()) {
    // Update the Tab Grid button style, based on whether the active tab is
    // grouped or not.
    const int active_index = webStateList->active_index();
    if (active_index != WebStateList::kInvalidIndex &&
        webStateList->GetGroupOfWebStateAt(active_index) != nullptr) {
      [self.consumer
          setTabGridButtonStyle:ToolbarTabGridButtonStyle::kTabGroup];
    } else {
      [self.consumer setTabGridButtonStyle:ToolbarTabGridButtonStyle::kNormal];
    }
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

@end
