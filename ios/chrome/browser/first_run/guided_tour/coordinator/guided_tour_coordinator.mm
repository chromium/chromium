// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/guided_tour/coordinator/guided_tour_coordinator.h"

#import <optional>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/metrics/metrics_service.h"
#import "ios/chrome/browser/first_run/guided_tour/coordinator/guided_tour_bubble_coordinator.h"
#import "ios/chrome/browser/first_run/guided_tour/coordinator/guided_tour_promo_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace first_run {

// Helper class used to access the passkey needed to call
// `MetricsService::StartOutOfBandUploadIfPossible()`. The class name matches
// the friend declaration in `metrics::MetricsService::OutOfBandUploadPasskey`.
class FirstRunProfileAgentMetricsHelper final {
 public:
  FirstRunProfileAgentMetricsHelper() = default;
  ~FirstRunProfileAgentMetricsHelper() = default;

  // Triggers an UMA metrics log upload.
  void StartOutOfBandUploadIfPossible() {
    metrics::MetricsService* metrics_service =
        GetApplicationContext()->GetMetricsService();
    // `MetricsService` can be nil for `TestingApplicationContext`.
    if (metrics_service) {
      metrics_service->StartOutOfBandUploadIfPossible(
          metrics::MetricsService::OutOfBandUploadPasskey());
    }
  }
};

}  // namespace first_run

namespace {

// Metrics logged for the Guided Tour.
const char kGuidedTourPromoResultHistogram[] = "IOS.GuidedTour.Promo.DidAccept";
const char kGuidedTourStepDidFinishHistogram[] = "IOS.GuidedTour.DidFinishStep";

}  // namespace

@interface GuidedTourCoordinator () <GuidedTourPromoCoordinatorDelegate,
                                     TabGridStateObserving>

@end

@implementation GuidedTourCoordinator {
  GuidedTourPromoCoordinator* _promoCoordinator;
  GuidedTourBubbleCoordinator* _ntpStepCoordinator;
  std::optional<GuidedTourStep> _currentGuidedTourStep;
  BOOL _guidedTourStarted;
}

- (void)dealloc {
  CHECK(!_promoCoordinator, base::NotFatalUntil::M162);
  CHECK(!_ntpStepCoordinator, base::NotFatalUntil::M162);
}

#pragma mark - ChromeCoordinator

- (void)start {
  _promoCoordinator = [[GuidedTourPromoCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  _promoCoordinator.delegate = self;
  [_promoCoordinator start];
}

- (void)stop {
  [self.sceneState.tabGridState removeObserver:self];
  if (_promoCoordinator) {
    [_promoCoordinator stopWithCompletion:nil];
    _promoCoordinator = nil;
    [self logGuidedTourPromoResult:NO];
  }
  if (_currentGuidedTourStep) {
    [self hideCurrentGuidedTourStep];
    _currentGuidedTourStep = std::nullopt;
  }
  if (self.completion) {
    void (^completion)(BOOL) = self.completion;
    self.completion = nil;
    completion(_guidedTourStarted);
  }
}

#pragma mark - TabGridStateObserving

- (void)willEnterTabGrid {
  if (_currentGuidedTourStep != GuidedTourStep::kTabGridIncognito) {
    return;
  }
  id<TabGridToolbarCommands> handler =
      HandlerForProtocol([self commandDispatcher], TabGridToolbarCommands);
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf stepCompleted:GuidedTourStep::kTabGridIncognito];
    [weakSelf showLongPressStep];
  };
  [handler showGuidedTourIncognitoStepWithDismissalCompletion:completion];
}

#pragma mark - GuidedTourPromoCoordinatorDelegate

- (void)dismissGuidedTourPromo {
  [_promoCoordinator stopWithCompletion:nil];
  _promoCoordinator = nil;
  [self logGuidedTourPromoResult:NO];
  [self guidedTourCompleted];
}

- (void)startGuidedTour {
  _guidedTourStarted = YES;
  _currentGuidedTourStep = GuidedTourStep::kNTP;
  [self.sceneState.tabGridState addObserver:self];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf showNTPStep];
  };
  [_promoCoordinator stopWithCompletion:completion];
  _promoCoordinator = nil;
  [self logGuidedTourPromoResult:YES];
}

#pragma mark - Private

// Command dispatcher for the browser.
- (CommandDispatcher*)commandDispatcher {
  return self.browser->GetCommandDispatcher();
}

// Shows the New Tab Page step of the Guided Tour.
- (void)showNTPStep {
  _currentGuidedTourStep = GuidedTourStep::kNTP;
  id<GuidedTourCommands> handler =
      HandlerForProtocol([self commandDispatcher], GuidedTourCommands);
  [handler highlightViewInStep:GuidedTourStep::kNTP];

  __weak __typeof(self) weakSelf = self;
  _ntpStepCoordinator =
      [[GuidedTourBubbleCoordinator alloc] initWithStep:GuidedTourStep::kNTP
                                     baseViewController:self.baseViewController
                                                browser:self.browser
                                        completionBlock:^{
                                          [weakSelf guidedTourNTPStepCompleted];
                                        }];
  [_ntpStepCoordinator start];
}

// Handles the completion of the NTP step of the Guided Tour.
- (void)guidedTourNTPStepCompleted {
  [_ntpStepCoordinator stop];
  _ntpStepCoordinator = nil;

  if (IsManualUploadForBestOfAppEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          first_run::FirstRunProfileAgentMetricsHelper metricsHelper;
          metricsHelper.StartOutOfBandUploadIfPossible();
        }));
  }
  id<GuidedTourCommands> handler =
      HandlerForProtocol([self commandDispatcher], GuidedTourCommands);
  [handler stepCompleted:GuidedTourStep::kNTP];

  [self stepCompleted:GuidedTourStep::kNTP];

  _currentGuidedTourStep = GuidedTourStep::kTabGridIncognito;
  id<SceneCommands> sceneHandler =
      HandlerForProtocol([self commandDispatcher], SceneCommands);
  [sceneHandler displayTabGridInMode:TabGridOpeningMode::kRegular];
}

// Shows the Long Press step of the Guided Tour in the Tab Grid.
- (void)showLongPressStep {
  _currentGuidedTourStep = GuidedTourStep::kTabGridLongPress;
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf stepCompleted:GuidedTourStep::kTabGridLongPress];
    [weakSelf showTabGroupStep];
  };
  id<TabGridCommands> handler =
      HandlerForProtocol([self commandDispatcher], TabGridCommands);
  [handler showGuidedTourLongPressStepWithDismissalCompletion:completion];
}

// Shows the Tab Group step of the Guided Tour in the Tab Grid.
- (void)showTabGroupStep {
  _currentGuidedTourStep = GuidedTourStep::kTabGridTabGroup;
  id<TabGridToolbarCommands> handler =
      HandlerForProtocol([self commandDispatcher], TabGridToolbarCommands);
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf stepCompleted:GuidedTourStep::kTabGridTabGroup];
    [weakSelf guidedTourCompleted];
  };
  [handler showGuidedTourTabGroupStepWithDismissalCompletion:completion];
}

// Called when the Guided Tour flow is completed.
- (void)guidedTourCompleted {
  [self.sceneState.tabGridState removeObserver:self];
  _currentGuidedTourStep = std::nullopt;
  if (self.completion) {
    void (^completion)(BOOL) = self.completion;
    self.completion = nil;
    completion(_guidedTourStarted);
  }
  CommandDispatcher* dispatcher = [self commandDispatcher];
  if ([dispatcher dispatchingForProtocol:@protocol(SceneCommands)]) {
    id<SceneCommands> sceneHandler =
        HandlerForProtocol(dispatcher, SceneCommands);
    [sceneHandler hideGuidedTour];
  }
}

// Handles the dismissal completion of `step` in the Guided Tour.
- (void)stepCompleted:(GuidedTourStep)step {
  CHECK(_currentGuidedTourStep);
  CHECK_EQ(step, _currentGuidedTourStep.value());
  base::UmaHistogramEnumeration(kGuidedTourStepDidFinishHistogram, step);
}

// Hides the currently active Guided Tour step UI.
- (void)hideCurrentGuidedTourStep {
  CommandDispatcher* dispatcher = [self commandDispatcher];

  switch (_currentGuidedTourStep.value()) {
    case GuidedTourStep::kNTP: {
      if ([dispatcher dispatchingForProtocol:@protocol(GuidedTourCommands)]) {
        id<GuidedTourCommands> handler =
            HandlerForProtocol(dispatcher, GuidedTourCommands);
        [handler stepCompleted:GuidedTourStep::kNTP];
      }
      [_ntpStepCoordinator stop];
      _ntpStepCoordinator = nil;
      break;
    }
    // Both tab grid steps are exited in the same way.
    case GuidedTourStep::kTabGridIncognito:
    case GuidedTourStep::kTabGridTabGroup: {
      if ([dispatcher
              dispatchingForProtocol:@protocol(TabGridToolbarCommands)]) {
        id<TabGridToolbarCommands> tabGridToolbarHandler =
            HandlerForProtocol(dispatcher, TabGridToolbarCommands);
        [tabGridToolbarHandler hideTabGridToolbarGuidedTour];
      }
      break;
    }
    case GuidedTourStep::kTabGridLongPress: {
      if ([dispatcher dispatchingForProtocol:@protocol(TabGridCommands)]) {
        id<TabGridCommands> tabGridHandler =
            HandlerForProtocol(dispatcher, TabGridCommands);
        [tabGridHandler hideTabGridGuidedTour];
      }
      break;
    }
  }
}

// Logs the user decision for the Guided Tour promo.
- (void)logGuidedTourPromoResult:(BOOL)didAccept {
  base::UmaHistogramBoolean(kGuidedTourPromoResultHistogram, didAccept);
  if (IsManualUploadForBestOfAppEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          first_run::FirstRunProfileAgentMetricsHelper metricsHelper;
          metricsHelper.StartOutOfBandUploadIfPossible();
        }));
  }
}

@end
