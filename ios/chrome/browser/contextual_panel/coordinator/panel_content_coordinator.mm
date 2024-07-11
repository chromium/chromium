// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/coordinator/panel_block_modulator.h"
#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_mediator.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/sample/coordinator/sample_block_modulator.h"
#import "ios/chrome/browser/contextual_panel/ui/contextual_sheet_display_controller.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_content_view_controller.h"
#import "ios/chrome/browser/price_insights/coordinator/price_insights_modulator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface PanelContentCoordinator () <
    PanelContentViewControllerMetricsDelegate>

@end

@implementation PanelContentCoordinator {
  // The view controller managed by this coordinator.
  PanelContentViewController* _viewController;

  // The mediator managed by this coordinator.
  PanelContentMediator* _mediator;

  // The child modulators owned by this coordinator.
  NSMutableArray<PanelBlockModulator*>* _modulators;
}

- (void)start {
  _viewController = [[PanelContentViewController alloc] init];
  _viewController.metricsDelegate = self;

  ChromeBroadcaster* broadcaster =
      FullscreenController::FromBrowser(self.browser)->broadcaster();

  _mediator = [[PanelContentMediator alloc] initWithBroadcaster:broadcaster];
  _mediator.consumer = _viewController;

  _modulators = [[NSMutableArray alloc] init];

  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(activeWebState);

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>> configurations =
      contextualPanelTabHelper->GetCurrentCachedConfigurations();

  NSMutableArray<PanelBlockData*>* panelBlocks = [[NSMutableArray alloc] init];
  for (base::WeakPtr<ContextualPanelItemConfiguration> configuration :
       configurations) {
    PanelBlockModulator* modulator =
        [self modulatorForConfiguration:configuration];
    if (!modulator) {
      continue;
    }
    [modulator start];
    PanelBlockData* panelBlockData = [modulator panelBlockData];
    if (!panelBlockData) {
      [modulator stop];
      continue;
    }
    [_modulators addObject:modulator];
    [panelBlocks addObject:panelBlockData];
  }

  _viewController.contextualSheetCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ContextualSheetCommands);
  [_viewController setPanelBlocks:panelBlocks];
  if ([self.baseViewController
          conformsToProtocol:@protocol(ContextualSheetDisplayController)]) {
    _viewController.sheetDisplayController =
        static_cast<id<ContextualSheetDisplayController>>(
            self.baseViewController);
  }

  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:_viewController.view];
  AddSameConstraints(self.baseViewController.view, _viewController.view);

  [_viewController didMoveToParentViewController:self.baseViewController];
}

- (PanelBlockModulator*)modulatorForConfiguration:
    (base::WeakPtr<ContextualPanelItemConfiguration>)configuration {
  if (!configuration) {
    return nil;
  }

  switch (configuration->item_type) {
    case ContextualPanelItemType::SamplePanelItem:
      return [[SampleBlockModulator alloc]
          initWithBaseViewController:_viewController
                             browser:self.browser
                   itemConfiguration:configuration];
    case ContextualPanelItemType::PriceInsightsItem:
      return [[PriceInsightsModulator alloc]
          initWithBaseViewController:_viewController
                             browser:self.browser
                   itemConfiguration:configuration];
  }
}

- (void)stop {
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

// Convenience method for getting the tab helper for the current active web
// state.
- (ContextualPanelTabHelper*)contextualPanelTabHelper {
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  return ContextualPanelTabHelper::FromWebState(activeWebState);
}

#pragma mark - PanelContentViewControllerMetricsDelegate

- (NSString*)entrypointInfoBlockName {
  auto entrypointConfig =
      [self contextualPanelTabHelper]->GetFirstCachedConfig();

  if (!entrypointConfig) {
    return nil;
  }

  return base::SysUTF8ToNSString(
      StringForItemType(entrypointConfig->item_type));
}

- (BOOL)wasLoudEntrypoint {
  return [self contextualPanelTabHelper]->WasLoudMomentEntrypointShown();
}

@end
