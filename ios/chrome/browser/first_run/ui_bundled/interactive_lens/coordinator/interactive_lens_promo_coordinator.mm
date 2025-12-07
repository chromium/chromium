// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/coordinator/interactive_lens_promo_coordinator.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "components/lens/lens_overlay_dismissal_source.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/interactive_lens_overlay_promo_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/lens_interactive_promo_results_page_presenter.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"

@interface InteractiveLensPromoCoordinator () <InteractiveLensPromoDelegate>
@end

@implementation InteractiveLensPromoCoordinator {
  // View controller for the Interactive Lens promo screen.
  InteractiveLensOverlayPromoViewController* _promoViewController;
  // Command handler for the Lens Overlay commands.
  __weak id<LensOverlayCommands> _lensOverlayHandler;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
    _lensOverlayHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), LensOverlayCommands);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  _promoViewController =
      [[InteractiveLensOverlayPromoViewController alloc] init];
  _promoViewController.delegate = self;
  _promoViewController.modalInPresentation = YES;

  // Force the view to load and layout.
  _promoViewController.view.frame = self.baseNavigationController.view.bounds;
  [_promoViewController.view layoutIfNeeded];

  // Now that the view has been laid out, create the lens overlay.
  // TODO(crbug.com/416480202): Consider pre-warming the Lens Overlay for cases
  // where the it might take longer to start up.
  __weak InteractiveLensOverlayPromoViewController* weakPromoViewController =
      _promoViewController;
  LensResultsPresenterFactory factory =
      ^(LensOverlayContainerViewController* baseViewController,
        LensResultPageViewController* resultsViewController) {
        LensInteractivePromoResultsPagePresenter* presenter =
            [[LensInteractivePromoResultsPagePresenter alloc]
                initWithBaseViewController:baseViewController
                  resultPageViewController:resultsViewController];
        presenter.interactivePromoDelegate = weakPromoViewController;
        return presenter;
      };

  [_lensOverlayHandler
          searchImageWithLens:_promoViewController.lensSearchImage
                   entrypoint:LensOverlayEntrypoint::kFREPromo
      initialPresentationBase:_promoViewController.lensContainerViewController
      resultsPresenterFactory:factory
                   completion:nil];

  base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                first_run::kInteractiveLensStart);

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _promoViewController ]
                                           animated:animated];
}

- (void)stop {
  [_lensOverlayHandler
      destroyLensUI:YES
             reason:lens::LensOverlayDismissalSource::kFREPromoNextButton];
  self.firstRunDelegate = nil;
  _promoViewController = nil;
  [super stop];
}

#pragma mark - InteractiveLensPromoDelegate

- (void)didTapContinueButtonWithInteraction:(BOOL)interaction {
  CHECK(self.firstRunDelegate);
  [self.firstRunDelegate screenWillFinishPresenting];
  first_run::FirstRunStage stage =
      interaction ? first_run::kInteractiveLensCompletionWithInteraction
                  : first_run::kInteractiveLensCompletionWithoutInteraction;
  base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram, stage);
}

@end
