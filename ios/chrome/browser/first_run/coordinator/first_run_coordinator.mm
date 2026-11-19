// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "base/values.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/metrics/metrics_service.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_coordinator.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/animated_lens/coordinator/animated_lens_promo_coordinator.h"
#import "ios/chrome/browser/first_run/best_features/coordinator/best_features_screen_coordinator.h"
#import "ios/chrome/browser/first_run/default_browser/coordinator/default_browser_screen_coordinator.h"
#import "ios/chrome/browser/first_run/interactive_lens/coordinator/interactive_lens_promo_coordinator.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/public/first_run_util.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
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
    if (metrics_service_) {
      metrics_service_->StartOutOfBandUploadIfPossible(
          metrics::MetricsService::OutOfBandUploadPasskey());
    }
  }

 private:
  raw_ptr<metrics::MetricsService> metrics_service_ = nullptr;
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
  // YES once `_navigationController` has been presented on
  // `baseViewController`.
  BOOL _navigationControllerPresented;
  // Completion block called when the First Run UI finishes dismissing.
  ProceduralBlock _completion;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            screenProvider:(ScreenProvider*)screenProvider
                                completion:(ProceduralBlock)completion {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK_EQ(browser->type(), Browser::Type::kRegular);
    _screenProvider = screenProvider;
    _completion = [completion copy];
    _navigationController =
        [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                      toolbarClass:nil];
    _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  }
  return self;
}

- (void)dealloc {
  CHECK(!_navigationController, base::NotFatalUntil::M155);
  CHECK(!_childCoordinator, base::NotFatalUntil::M155);
}

- (void)start {
  // `-presentScreen:` may finish First Run synchronously if all screens skip
  // presentation, causing `delegate` to stop and deallocate `self`.
  __weak __typeof(self) weakSelf = self;
  [self presentScreen:[self.screenProvider nextScreenType]];
  [weakSelf presentNavigationControllerIfNeeded];
}

- (void)stopWithCompletion {
  [self stopWithExecuteCompletion:YES];
}

- (void)stop {
  [self stopWithExecuteCompletion:NO];
}

#pragma mark - FirstRunScreenDelegate

- (void)firstRunScreenCoordinatorWantsToBeStopped:
    (ChromeCoordinator*)coordinator {
  CHECK_EQ(coordinator, self.childCoordinator, base::NotFatalUntil::M155);
  [self stopChildCoordinator];
  [self presentScreen:[self.screenProvider nextScreenType]];

  // Trigger a metrics log upload with the MetricsService.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        std::unique_ptr<first_run::FirstRunCoordinatorMetricsHelper>
            metricsHelper =
                std::make_unique<first_run::FirstRunCoordinatorMetricsHelper>();
        metricsHelper->StartOutOfBandUploadIfPossible();
      }));
}

#pragma mark - Helper

// Presents the screen of certain `type`.
- (void)presentScreen:(ScreenType)type {
  // If no more screen need to be present, call delegate to stop presenting
  // screens.
  if (type == ScreenType::kStepsCompleted) {
    // The user went through all screens of the FRE.
    if (!_navigationControllerPresented) {
      // All screens skipped presentation synchronously during `-start`, so
      // `-presentViewController:animated:completion:` will not run to log
      // `kStart`.
      base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                    first_run::kStart);
    }
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kComplete);

    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(self.profile);

    if (tracker) {
      tracker->NotifyEvent(feature_engagement::events::kIOSFirstRunComplete);
    }

    WriteFirstRunSentinel();
    // `-didFinishFirstRun` stops and deallocates `self`, so any post-First Run
    // IPH fallback must run beforehand.
    [self presentPostFirstRunIPHIfNeeded];
    [self.delegate didFinishFirstRun];
    return;
  }
  self.childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [self.childCoordinator start];
}

// Creates a screen coordinator according to `type`.
- (ChromeCoordinator*)createChildCoordinatorWithScreenType:(ScreenType)type {
  using enum ScreenType;
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
  [self firstRunScreenCoordinatorWantsToBeStopped:historySyncCoordinator];
}

#pragma mark - Private

// Presents `_navigationController` if the coordinator has not already stopped
// during `-start`.
- (void)presentNavigationControllerIfNeeded {
  if (!_navigationController) {
    return;
  }
  _navigationControllerPresented = YES;
  void (^completion)(void) = ^{
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kStart);
  };
  [_navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:_navigationController
                                        animated:NO
                                      completion:completion];
}

// Presents the post-First Run IPH if the profile agent killswitch is disabled.
- (void)presentPostFirstRunIPHIfNeeded {
  if (first_run::IsPostFREIphInProfileAgentEnabled()) {
    return;
  }
  if (self.browser == nullptr) {
    // Speculative fix for some of the crashes in crbug.com/474279386. There
    // is most likely an underlying issue somewhere in the cleanup logic,
    // but null checking here should at least prevent some crashes.
    return;
  }
  if (self.profile && self.profile->GetPrefs() &&
      self.profile->GetPrefs()->GetBoolean(
          prefs::kAppStoreGeminiPromoTriggered)) {
    // If first run started due to app store external action, do not show
    // any follow up IPH.
    return;
  }
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (localState &&
      !localState->GetList(prefs::kFirstRunDeferredScreens).empty() &&
      first_run::IsFREAblationEnabled() &&
      first_run::GetFREAblationVariationType() ==
          first_run::FREAblationVariationType::kSkipAllExceptSignInAndChoice) {
    return;
  }
  if (IsBestOfAppLensAnimatedPromoEnabled()) {
    // Present the Lens entrypoint IPH.
    [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                        NewTabPageCommands) presentLensIconBubble];
  } else {
    // Present feed swipe IPH.
    [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                        NewTabPageCommands) presentFeedSwipeFirstRunBubble];
  }
}

// Stops the coordinator and optionally executes `_completion` after dismissal.
- (void)stopWithExecuteCompletion:(BOOL)executeCompletion {
  if (self.childCoordinator) {
    // If the child coordinator is not nil, then the FRE is stopped because
    // Chrome is being shutdown.
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kFirstRunInterrupted);
    [self stopChildCoordinator];
  }
  ProceduralBlock completion = nil;
  if (executeCompletion) {
    completion = _completion;
  }
  _completion = nil;
  if (_navigationController.presentingViewController) {
    [_navigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:completion];
  } else if (completion) {
    // `_navigationController` was never presented because all screens finished
    // synchronously during `-start`. Post `completion` to match the async
    // contract of `-dismissViewControllerAnimated:completion:`.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completion));
  }
  _navigationController = nil;
  [super stop];
}

- (void)stopChildCoordinator {
  // The child's `stop` method must unset the delegate.
  [self.childCoordinator stop];
  self.childCoordinator = nil;
}

@end
