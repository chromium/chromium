// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_selection_placeholder_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_web_state_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/web/model/web_state_delegate_browser_agent.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

@interface LensOverlayCoordinator () <LensOverlayCommands,
                                      UISheetPresentationControllerDelegate,
                                      LensOverlayResultConsumer,
                                      LensResultPageWebStateDelegate>
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

  /// The tab helper associated with the current UI.
  LensOverlayTabHelper* _associatedTabHelper;
}

#pragma mark - properties

- (void)createUIWithSnapshot:(UIImage*)snapshot {
  [self createContainerViewController];
  [self createSelectionViewController];
  [self createMediator];

  // Wire up consumers and delegates
  _containerViewController.selectionViewController = _selectionViewController;
  _selectionViewController.delegate = _mediator;
  _mediator.snapshotConsumer = _selectionViewController;

  [_mediator startWithSnapshot:snapshot];
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

  // Results UI is lazily initialized; see comment in LensOverlayResultConsumer
  // section.
  _mediator.resultConsumer = self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(IsLensOverlayAvailable());
  [super start];

  Browser* browser = self.browser;
  CHECK(browser, kLensOverlayNotFatalUntil);

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(lowMemoryWarningReceived)
             name:UIApplicationDidReceiveMemoryWarningNotification
           object:nil];

  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensOverlayCommands)];
}

- (void)stop {
  if (Browser* browser = self.browser) {
    [browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  }

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidReceiveMemoryWarningNotification
              object:nil];

  [super stop];
}

#pragma mark - LensOverlayCommands

- (void)createAndShowLensUI:(BOOL)animated {
  if ([self isUICreated]) {
    // The UI is probably associated with the non-active tab. Destroy it with no
    // animation.
    [self destroyLensUI:NO];
  }

  _associatedTabHelper = [self activeTabHelper];
  CHECK(_associatedTabHelper, kLensOverlayNotFatalUntil);

  // The instance that creates the Lens UI designates itself as the command
  // handler for the associated tab.
  _associatedTabHelper->SetLensOverlayCommandsHandler(self);
  _associatedTabHelper->SetLensOverlayShown(true);

  UIImage* snapshot = [self captureSnapshot];
  [self createUIWithSnapshot:snapshot];
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
  // The reason the UI is destroyed can be that Omnient gets associated to a
  // different tab. In this case mark the stale tab helper as not shown.
  if (_associatedTabHelper) {
    _associatedTabHelper->SetLensOverlayShown(false);
    _associatedTabHelper = nil;
  }

  if (_containerViewController.presentingViewController) {
    [_containerViewController.presentingViewController
        dismissViewControllerAnimated:animated
                           completion:^{
                             [self destroyViewControllersAndMediators];
                           }];
  } else {
    [self destroyViewControllersAndMediators];
  }
}

#pragma mark - UISheetPresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return presentationController !=
         _resultViewController.sheetPresentationController;
}

#pragma mark - LensOverlayResultConsumer

// This coordinator acts as a proxy consumer to the result consumer to implement
// lazy initialization of the result UI.
// Upon any call, the results UI is created and set as consumer, then the call
// is repeated.
- (void)loadResultsURL:(GURL)url {
  DCHECK(!_resultMediator);

  [self startResultPage];
  [_resultMediator loadResultsURL:url];
}

#pragma mark - private

- (void)startResultPage {
  Browser* browser = self.browser;
  web::WebState::CreateParams params =
      web::WebState::CreateParams(browser->GetBrowserState());
  web::WebStateDelegate* browserWebStateDelegate =
      WebStateDelegateBrowserAgent::FromBrowser(browser);
  _resultMediator = [[LensResultPageMediator alloc]
       initWithWebStateParams:params
      browserWebStateDelegate:browserWebStateDelegate
                  isIncognito:browser->GetBrowserState()->IsOffTheRecord()];
  _resultMediator.applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  _mediator.resultConsumer = _resultMediator;

  _resultViewController = [[LensResultPageViewController alloc] init];

  _resultMediator.consumer = _resultViewController;
  _resultMediator.webViewContainer = _resultViewController.webViewContainer;

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
  [_resultMediator disconnect];
  _resultMediator = nil;
  _mediator.resultConsumer = self;
}

- (BOOL)isUICreated {
  return _containerViewController != nil;
}

// Disconnect and destroy all of the owned view controllers.
- (void)destroyViewControllersAndMediators {
  [self stopResultPage];
  _containerViewController = nil;
  _mediator = nil;
}

// The tab helper for the active web state.
- (LensOverlayTabHelper*)activeTabHelper {
  if (!self.browser || !self.browser->GetWebStateList() ||
      !self.browser->GetWebStateList()->GetActiveWebState()) {
    return nullptr;
  }

  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  LensOverlayTabHelper* tabHelper =
      LensOverlayTabHelper::FromWebState(activeWebState);

  CHECK(tabHelper, kLensOverlayNotFatalUntil);

  return tabHelper;
}

// Captures a screenshot of the active web state.
- (UIImage*)captureSnapshot {
  if (!self.browser) {
    return nil;
  }

  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  if (!activeWebState) {
    return nil;
  }

  SnapshotTabHelper* snapshotTabHelper =
      SnapshotTabHelper::FromWebState(activeWebState);
  CHECK(snapshotTabHelper, kLensOverlayNotFatalUntil);

  return snapshotTabHelper->GenerateSnapshotWithoutOverlays();
}

- (void)lowMemoryWarningReceived {
  // Preserve the UI if it's currently visible to the user.
  if ([self isLensOverlayVisible]) {
    return;
  }

  [self destroyLensUI:NO];
}

- (BOOL)isLensOverlayVisible {
  if (_associatedTabHelper) {
    return _associatedTabHelper->IsLensOverlayShown();
  }

  return NO;
}

- (void)lensResultPageWebStateDestroyed {
  [self stopResultPage];
}

@end
