// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_coordinator.h"

#import "ios/chrome/browser/contextual_panel/coordinator/panel_block_modulator.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/sample/coordinator/sample_block_modulator.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_content_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation PanelContentCoordinator {
  // The view controller managed by this coordinator.
  PanelContentViewController* _viewController;

  // The child modulators owned by this coordinator.
  NSMutableArray<PanelBlockModulator*>* _modulators;
}

- (void)start {
  _viewController = [[PanelContentViewController alloc] init];
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
    if (!configuration) {
      continue;
    }
    PanelBlockModulator* modulator =
        [[SampleBlockModulator alloc] initWithBaseViewController:_viewController
                                                         browser:self.browser
                                               itemConfiguration:configuration];
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

  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:_viewController.view];
  AddSameConstraints(self.baseViewController.view, _viewController.view);

  [_viewController didMoveToParentViewController:self.baseViewController];
}

@end
