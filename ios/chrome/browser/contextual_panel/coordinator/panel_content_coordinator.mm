// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_coordinator.h"

#import "base/memory/raw_ptr.h"
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
#import "ios/chrome/common/ui/util/ui_util.h"

@interface PanelContentCoordinator () <
    PanelContentViewControllerMetricsDelegate,
    UIAdaptivePresentationControllerDelegate>

@end

@implementation PanelContentCoordinator {
  // The view controller managed by this coordinator.
  PanelContentViewController* _viewController;

  // The mediator managed by this coordinator.
  PanelContentMediator* _mediator;

  // The child modulators owned by this coordinator.
  NSMutableArray<PanelBlockModulator*>* _modulators;

  // The contextual panel tab helper to use for this panel.
  raw_ptr<ContextualPanelTabHelper> _contextualPanelTabHelper;

  // Read-write version of `self.baseViewController` as the base view
  // controller for this coordinator changes during its lifetime.
  UIViewController* _modifiableBaseViewController;
}

- (void)start {
  _modifiableBaseViewController = self.baseViewController;

  _viewController = [[PanelContentViewController alloc] init];
  _viewController.metricsDelegate = self;
  _viewController.traitCollectionDelegate = self.traitCollectionDelegate;

  ChromeBroadcaster* broadcaster =
      FullscreenController::FromBrowser(self.browser)->broadcaster();

  _mediator = [[PanelContentMediator alloc] initWithBroadcaster:broadcaster];
  _mediator.consumer = _viewController;

  _modulators = [[NSMutableArray alloc] init];

  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  _contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(activeWebState);

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>> configurations =
      _contextualPanelTabHelper->GetCurrentCachedConfigurations();

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

  // On iPad, present using iOS's built-in UISheetController.
  if (IsRegularXRegularSizeClass(_modifiableBaseViewController)) {
    [self presentViewControllerFromBaseViewControllerAnimated:YES];
  } else {
    // On iPhone/iPad multiwindow, add the view controller instead of presenting
    // it to use the custom Contextual Panel sheet.
    [self addViewControllerToBaseViewController];
  }
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
  if ([_viewController presentingViewController]) {
    [_modifiableBaseViewController dismissViewControllerAnimated:YES
                                                      completion:nil];
  } else {
    [self removeViewControllerFromBaseViewController];
  }
  _viewController = nil;
}

#pragma mark - Public

- (void)presentFromNewBaseViewController:(UIViewController*)viewController {
  [self removeViewControllerFromBaseViewController];

  _modifiableBaseViewController = viewController;

  [self presentViewControllerFromBaseViewControllerAnimated:NO];
}

- (void)embedInParentViewController:(UIViewController*)viewController {
  __weak __typeof(self) weakSelf = self;
  [_modifiableBaseViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf addViewControllerToBaseViewController];
                         }];

  _modifiableBaseViewController = viewController;
}

#pragma mark - View hierarcy manipulation helper methods

// Adds the view controller as a child of the current base view controller.
- (void)addViewControllerToBaseViewController {
  if ([_modifiableBaseViewController
          conformsToProtocol:@protocol(ContextualSheetDisplayController)]) {
    _viewController.sheetDisplayController =
        static_cast<id<ContextualSheetDisplayController>>(
            _modifiableBaseViewController);
  }

  [_modifiableBaseViewController addChildViewController:_viewController];
  [_modifiableBaseViewController.view addSubview:_viewController.view];
  _viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(_modifiableBaseViewController.view, _viewController.view);

  [_viewController didMoveToParentViewController:_modifiableBaseViewController];
}

// Removes the view controller from the view hierarcy of the current base view
// controller.
- (void)removeViewControllerFromBaseViewController {
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
}

// Presents the view controller modally from the current base view controller.
- (void)presentViewControllerFromBaseViewControllerAnimated:(BOOL)animated {
  _viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  _viewController.presentationController.delegate = self;
  [_modifiableBaseViewController presentViewController:_viewController
                                              animated:animated
                                            completion:nil];
}

#pragma mark - PanelContentViewControllerMetricsDelegate

- (NSString*)entrypointInfoBlockName {
  auto entrypointConfig = _contextualPanelTabHelper->GetFirstCachedConfig();

  if (!entrypointConfig) {
    return nil;
  }

  return base::SysUTF8ToNSString(
      StringForItemType(entrypointConfig->item_type));
}

- (BOOL)wasLoudEntrypoint {
  return _contextualPanelTabHelper->WasLoudMomentEntrypointShown();
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  id<ContextualSheetCommands> contextualSheetCommandHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         ContextualSheetCommands);
  [contextualSheetCommandHandler closeContextualSheet];
}

@end
