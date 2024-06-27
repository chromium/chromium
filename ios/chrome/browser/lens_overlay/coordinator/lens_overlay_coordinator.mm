// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_selection_placeholder_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@interface LensOverlayCoordinator () <LensOverlayCommands,
                                      UISheetPresentationControllerDelegate>

// The tab helper for the instance for the active web state.
@property(nonatomic, readonly, assign) LensOverlayTabHelper* tabHelper;

@end

@implementation LensOverlayCoordinator {
  /// Container view controller.
  /// Hosts all of lens UI: contains the selection UI, presents the results UI
  /// modally.
  LensOverlayContainerViewController* _containerViewController;

  /// Selection view controller.
  LensOverlaySelectionPlaceholderViewController* _selectionViewController;

  /// The mediator for lens overlay.
  LensOverlayMediator* _mediator;

  /// The view controller for lens results.
  LensResultPageViewController* _resultViewController;
  /// The mediator for lens results.
  LensResultPageMediator* _resultMediator;
}

#pragma mark - properties

- (void)createUI {
  [self createContainerViewController];
  [self createSelectionViewController];
  [self createMediator];

  // Wire up consumers and delegates
  _containerViewController.selectionViewController = _selectionViewController;
  _selectionViewController.delegate = _mediator;
}

- (void)createSelectionViewController {
  if (_selectionViewController) {
    return;
  }
  _selectionViewController =
      [[LensOverlaySelectionPlaceholderViewController alloc] init];
}

- (void)createContainerViewController {
  if (_containerViewController) {
    return;
  }
  _containerViewController = [[LensOverlayContainerViewController alloc]
      initWithLensOverlayCommandsHandler:self];
  _containerViewController.modalPresentationStyle =
      UIModalPresentationOverFullScreen;
  _containerViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
}

- (void)createMediator {
  if (_mediator) {
    return;
  }
  _mediator = [[LensOverlayMediator alloc] init];
}

- (LensOverlayTabHelper*)tabHelper {
  if (!self.browser || !self.browser->GetWebStateList() ||
      self.browser->GetWebStateList()->GetActiveWebState()) {
    return nullptr;
  }

  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  LensOverlayTabHelper* tabHelper =
      LensOverlayTabHelper::FromWebState(activeWebState);

  DCHECK(tabHelper);

  return tabHelper;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(base::FeatureList::IsEnabled(kEnableLensOverlay));
  [super start];

  Browser* browser = self.browser;
  CHECK(browser, kLensOverlayNotFatalUntil);

  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensOverlayCommands)];
}

- (void)stop {
  if (Browser* browser = self.browser) {
    [browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  }

  [super stop];
}

#pragma mark - LensOverlayCommands

- (void)createAndShowLensUI:(BOOL)animated {
  if ([self isUICreated]) {
    // The UI is probably associated with the non-active tab. Destroy it with no
    // animation.
    [self destroyLensUI:NO];
  }

  if (LensOverlayTabHelper* tabHelper = self.tabHelper) {
    // The instance that creates the Lens UI designates itself as the command
    // handler for the associated tab.
    tabHelper->SetLensOverlayCommandsHandler(self);
    tabHelper->SetLensOverlayShown(true);
  }

  [self createUI];
  [self showLensUI:animated];
}

- (void)showLensUI:(BOOL)animated {
  if (![self isUICreated]) {
    return;
  }

  [self.baseViewController presentViewController:_containerViewController
                                        animated:animated
                                      completion:nil];
}

- (void)hideLensUI:(BOOL)animated {
  if (![self isUICreated]) {
    return;
  }

  [_containerViewController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:nil];
}

- (void)destroyLensUI:(BOOL)animated {
  if (LensOverlayTabHelper* tabHelper = self.tabHelper) {
    tabHelper->SetLensOverlayShown(false);
  }

  if (_containerViewController.presentingViewController) {
    [_containerViewController.presentingViewController
        dismissViewControllerAnimated:animated
                           completion:^{
                             [self destroyViewControllers];
                           }];
  } else {
    [self destroyViewControllers];
  }
}

#pragma mark - UISheetPresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return presentationController !=
         _resultViewController.sheetPresentationController;
}

#pragma mark - private

- (void)startResultPage {
  _resultMediator = [[LensResultPageMediator alloc] init];

  _resultViewController = [[LensResultPageViewController alloc] init];

  UISheetPresentationController* sheet =
      _resultViewController.sheetPresentationController;
  sheet.delegate = self;
  sheet.prefersEdgeAttachedInCompactHeight = YES;
  sheet.largestUndimmedDetentIdentifier =
      [UISheetPresentationControllerDetent largeDetent].identifier;
  sheet.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  sheet.prefersGrabberVisible = YES;

  [_containerViewController presentViewController:_resultViewController
                                         animated:YES
                                       completion:nil];
}

- (void)stopResultPage {
  [_resultViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _resultViewController = nil;
  _resultMediator = nil;
}

- (BOOL)isUICreated {
  return _containerViewController != nil;
}

// Disconnect and destroy all of the owned view controllers.
- (void)destroyViewControllers {
  _containerViewController = nil;
}

@end
