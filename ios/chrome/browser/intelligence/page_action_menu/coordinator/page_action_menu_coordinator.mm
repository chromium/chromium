// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"

#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_metrics.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_controls_view.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_view_controller.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"

namespace {

const CGFloat kMenuCornerRadius = 20;

}

@interface PageActionMenuCoordinator () <
    PageActionMenuViewControllerDelegate,
    UINavigationControllerDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation PageActionMenuCoordinator {
  UINavigationController* _navigationController;
  PageActionMenuViewController* _viewController;
  PageActionMenuMediator* _mediator;
  // Reader mode view controller and mediator.
  ReaderModeOptionsViewController* _readerModeOptionsViewController;
  ReaderModeOptionsMediator* _readerModeOptionsMediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  BOOL readerModeActive = NO;
  BOOL readerModeAvailable = NO;
  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(activeWebState);
  if (readerModeTabHelper) {
    readerModeActive = readerModeTabHelper->IsActive();
    readerModeAvailable =
        base::FeatureList::IsEnabled(
            kEnableReaderModePageEligibilityForToolsMenu)
            ? readerModeTabHelper->CurrentPageIsDistillable()
            : readerModeTabHelper->CurrentPageIsEligibleForReaderMode();

    DistillerService* distillerService =
        DistillerServiceFactory::GetForProfile(self.profile);
    _readerModeOptionsMediator = [[ReaderModeOptionsMediator alloc]
        initWithDistilledPagePrefs:distillerService->GetDistilledPagePrefs()
                      webStateList:self.browser->GetWebStateList()];
  }

  _viewController = [[PageActionMenuViewController alloc]
      initWithReaderModeActive:readerModeActive];
  _viewController.delegate = self;
  _mediator = [[PageActionMenuMediator alloc] init];
  if (readerModeAvailable) {
    _viewController.readerModeHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ReaderModeCommands);
  }

  _viewController.pageActionMenuHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PageActionMenuCommands);

  BwgService* BWGService = BwgServiceFactory::GetForProfile(self.profile);
  if (BWGService->IsBwgAvailableForWebState(activeWebState)) {
    CHECK(BWGService->IsProfileEligibleForBwg());
    _viewController.BWGHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(), BWGCommands);
  }

  if (IsLensOverlayAvailable(self.profile->GetPrefs()) &&
      search::DefaultSearchProviderIsGoogle(
          ios::TemplateURLServiceFactory::GetForProfile(self.profile))) {
    _viewController.lensOverlayHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), LensOverlayCommands);
  }

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.delegate = self;
  _navigationController.presentationController.delegate = self;
  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;

  // Configure presentation sheet.
  __weak __typeof(self) weakSelf = self;
  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf resolveDetentValueForSheetPresentation:context];
  };
  UISheetPresentationControllerDetent* initialDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kAIHubDetentIdentifier
                            resolver:detentResolver];
  _navigationController.sheetPresentationController.detents = @[
    initialDetent,
  ];
  _navigationController.sheetPresentationController.selectedDetentIdentifier =
      kAIHubDetentIdentifier;
  _navigationController.sheetPresentationController.preferredCornerRadius =
      kMenuCornerRadius;
  _navigationController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;
  _navigationController.sheetPresentationController.prefersGrabberVisible = NO;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];

  [super start];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

#pragma mark - Public

- (void)stopWithCompletion:(ProceduralBlock)completion {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:completion];
  }
  _viewController = nil;
  _mediator = nil;
  _readerModeOptionsViewController = nil;
  [_readerModeOptionsMediator disconnect];
  _readerModeOptionsMediator = nil;
  [super stop];
}

#pragma mark - PageActionMenuViewControllerDelegate

- (void)viewControllerDidTapReaderModeOptionsButton:
    (PageActionMenuViewController*)viewController {
  _readerModeOptionsViewController =
      [[ReaderModeOptionsViewController alloc] init];
  [_readerModeOptionsViewController updateHideReaderModeButtonVisibility:YES];
  _readerModeOptionsViewController.readerModeOptionsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         ReaderModeOptionsCommands);
  _readerModeOptionsViewController.mutator = _readerModeOptionsMediator;
  _readerModeOptionsViewController.controlsView.mutator =
      _readerModeOptionsMediator;
  [_navigationController pushViewController:_readerModeOptionsViewController
                                   animated:YES];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordAIHubAction(IOSAIHubAction::kDismiss);
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  if (viewController == _viewController) {
    // If `_viewController` will be shown then it means that is was just pushed
    // as the root view controller, or `_readerModeOptionsViewController` was
    // just popped. In any case, `_readerModeOptionsViewController` can be set
    // to nil so it can be freed.
    _readerModeOptionsViewController = nil;
    _readerModeOptionsMediator.consumer = _viewController;
  } else {
    _readerModeOptionsMediator.consumer =
        _readerModeOptionsViewController.controlsView;
  }
  // Invalidate detents.
  [navigationController.sheetPresentationController animateChanges:^{
    [navigationController.sheetPresentationController invalidateDetents];
  }];
}

#pragma mark - Private

// Returns the appropriate detent value for a sheet presentation in `context`.
- (CGFloat)resolveDetentValueForSheetPresentation:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  // TODO(crbug.com/432213672): Add a protocol with
  // `resolveDetentValueForSheetPresentation:`.
  if (_readerModeOptionsViewController ==
      _navigationController.topViewController) {
    return [_readerModeOptionsViewController
        resolveDetentValueForSheetPresentation:context];
  }
  return [_viewController resolveDetentValueForSheetPresentation:context];
}

@end
