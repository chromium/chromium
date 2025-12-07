// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/metrics/metrics_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_coordinator.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/animated_lens/coordinator/animated_lens_promo_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/coordinator/interactive_lens_promo_coordinator.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace first_run {

// Helper class used to access the passkey needed to call
// MetricsService::StartOutOfBandUploadIfPossible().
class FirstRunCoordinatorMetricsHelper final {
 public:
  FirstRunCoordinatorMetricsHelper() {
    metrics_service_ = GetApplicationContext()->GetMetricsService();
  }
  ~FirstRunCoordinatorMetricsHelper() {}

  // Triggers an UMA metrics log upload.
  void StartOutOfBandUploadIfPossible() {
    metrics_service_->StartOutOfBandUploadIfPossible(
        metrics::MetricsService::OutOfBandUploadPasskey());
  }

 private:
  raw_ptr<metrics::MetricsService> metrics_service_;
};

}  // namespace first_run

@interface FirstRunCoordinator () <FirstRunScreenDelegate,
                                   HistorySyncCoordinatorDelegate>

@property(nonatomic, strong) ScreenProvider* screenProvider;
@property(nonatomic, strong) ChromeCoordinator* childCoordinator;

@end

@implementation FirstRunCoordinator {
  // First Run navigation controller.
  UINavigationController* _navigationController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            screenProvider:(ScreenProvider*)screenProvider {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK_EQ(browser->type(), Browser::Type::kRegular,
             base::NotFatalUntil::M145);
    _screenProvider = screenProvider;
    _navigationController =
        [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                      toolbarClass:nil];
    _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  }
  return self;
}

- (void)start {
  [self presentScreen:[self.screenProvider nextScreenType]];
  void (^completion)(void) = ^{
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kStart);
  };
  [_navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:_navigationController
                                        animated:NO
                                      completion:completion];
}

- (void)stopWithCompletion:(ProceduralBlock)completionHandler {
  if (self.childCoordinator) {
    // If the child coordinator is not nil, then the FRE is stopped because
    // Chrome is being shutdown.
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kFirstRunInterrupted);
    [self stopChildCoordinator];
  }
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completionHandler];
  _navigationController = nil;
  [super stop];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

#pragma mark - FirstRunScreenDelegate

- (void)screenWillFinishPresenting {
  [self stopChildCoordinator];
  [self presentScreen:[self.screenProvider nextScreenType]];

  if (base::FeatureList::IsEnabled(first_run::kManualLogUploadsInTheFRE)) {
    // Trigger a metrics log upload with the MetricsService.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          std::unique_ptr<first_run::FirstRunCoordinatorMetricsHelper>
              metricsHelper = std::make_unique<
                  first_run::FirstRunCoordinatorMetricsHelper>();
          metricsHelper->StartOutOfBandUploadIfPossible();
        }));
  }
}

#pragma mark - Helper

// Presents the screen of certain `type`.
- (void)presentScreen:(ScreenType)type {
  // If no more screen need to be present, call delegate to stop presenting
  // screens.
  if (type == kStepsCompleted) {
    // The user went through all screens of the FRE.
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kComplete);

    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(self.profile);

    if (tracker) {
      tracker->NotifyEvent(feature_engagement::events::kIOSFirstRunComplete);
    }

    WriteFirstRunSentinel();
    [self.delegate didFinishFirstRun];

    if (IsBestOfAppLensAnimatedPromoEnabled()) {
      // Present the Lens entrypoint IPH.
      [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                          NewTabPageCommands) presentLensIconBubble];
    } else {
      // Present feed swipe IPH.
      [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                          NewTabPageCommands) presentFeedSwipeFirstRunBubble];
    }

    return;
  }
  self.childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [self.childCoordinator start];
}

// Creates a screen coordinator according to `type`.
- (ChromeCoordinator*)createChildCoordinatorWithScreenType:(ScreenType)type {
  switch (type) {
    case kSignIn:
      return [[FullscreenSigninScreenCoordinator alloc]
           initWithBaseNavigationController:_navigationController
                                    browser:self.browser
                                   delegate:self
                               contextStyle:SigninContextStyle::kDefault
                                accessPoint:signin_metrics::AccessPoint::
                                                kStartPage
                                promoAction:signin_metrics::PromoAction::
                                                PROMO_ACTION_NO_SIGNIN_PROMO
          changeProfileContinuationProvider:DoNothingContinuationProvider()];
    case kHistorySync:
      return [[HistorySyncCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  delegate:self
                                  firstRun:YES
                             showUserEmail:NO
                                isOptional:YES
                              contextStyle:SigninContextStyle::kDefault
                               accessPoint:signin_metrics::AccessPoint::
                                               kStartPage];
    case kDefaultBrowserPromo:
      return [[DefaultBrowserScreenCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  delegate:self];
    case kChoice:
      return [[SearchEngineChoiceCoordinator alloc]
          initForFirstRunWithBaseNavigationController:_navigationController
                                              browser:self.browser
                                     firstRunDelegate:self];
    case kDockingPromo:
      return [[DockingPromoCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  delegate:self];
    case kBestFeatures:
      return [[BestFeaturesScreenCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  delegate:self];
    case kLensInteractivePromo: {
      InteractiveLensPromoCoordinator* lensInteractivePromoCoordinator =
          [[InteractiveLensPromoCoordinator alloc]
              initWithBaseNavigationController:_navigationController
                                       browser:self.browser];
      lensInteractivePromoCoordinator.firstRunDelegate = self;
      return lensInteractivePromoCoordinator;
    }
    case kLensAnimatedPromo: {
      AnimatedLensPromoCoordinator* lensAnimatedPromoCoordinator =
          [[AnimatedLensPromoCoordinator alloc]
              initWithBaseNavigationController:_navigationController
                                       browser:self.browser];
      lensAnimatedPromoCoordinator.firstRunDelegate = self;
      return lensAnimatedPromoCoordinator;
    }
    case kSyncedSetUp:
    case kGuidedTour:
    case kSafariImport:
    case kStepsCompleted:
      NOTREACHED() << "Reaches kStepsCompleted unexpectedly.";
  }
  return nil;
}

#pragma mark - HistorySyncCoordinatorDelegate

- (void)historySyncCoordinator:(HistorySyncCoordinator*)historySyncCoordinator
                    withResult:(HistorySyncResult)result {
  CHECK_EQ(self.childCoordinator, historySyncCoordinator);
  [self screenWillFinishPresenting];
}

#pragma mark - Private

- (void)stopChildCoordinator {
  [self.childCoordinator stop];
  self.childCoordinator = nil;
}

@end
