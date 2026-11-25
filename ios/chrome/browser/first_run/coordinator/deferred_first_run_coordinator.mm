// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/deferred_first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/check_op.h"
#import "base/notreached.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/first_run/animated_lens/coordinator/animated_lens_promo_coordinator.h"
#import "ios/chrome/browser/first_run/best_features/coordinator/best_features_screen_coordinator.h"
#import "ios/chrome/browser/first_run/default_browser/coordinator/default_browser_screen_coordinator.h"
#import "ios/chrome/browser/first_run/interactive_lens/coordinator/interactive_lens_promo_coordinator.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/promos_manager/coordinator/promos_manager_ui_handler.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"

namespace {

// Converts a persisted integer `raw_value` from
// `prefs::kFirstRunDeferredScreens` into its corresponding deferrable
// `ScreenType`, or `std::nullopt` if invalid.
std::optional<ScreenType> DeferredScreenTypeFromInt(int raw_value) {
  switch (raw_value) {
    case static_cast<int>(ScreenType::kHistorySync):
      return ScreenType::kHistorySync;
    case static_cast<int>(ScreenType::kDefaultBrowserPromo):
      return ScreenType::kDefaultBrowserPromo;
    case static_cast<int>(ScreenType::kBestFeatures):
      return ScreenType::kBestFeatures;
    case static_cast<int>(ScreenType::kLensInteractivePromo):
      return ScreenType::kLensInteractivePromo;
    case static_cast<int>(ScreenType::kLensAnimatedPromo):
      return ScreenType::kLensAnimatedPromo;
    default:
      return std::nullopt;
  }
}

// Pops and returns the next valid deferrable `ScreenType` from
// `prefs::kFirstRunDeferredScreens` in `local_state`, or returns
// `ScreenType::kStepsCompleted` when none remain.
ScreenType PopNextDeferredScreen(PrefService* local_state) {
  if (!local_state ||
      local_state->GetList(prefs::kFirstRunDeferredScreens).empty()) {
    return ScreenType::kStepsCompleted;
  }
  ScopedListPrefUpdate update(local_state, prefs::kFirstRunDeferredScreens);
  base::ListValue& list = update.Get();
  while (!list.empty()) {
    std::optional<int> raw_value = list.front().GetIfInt();
    list.erase(list.begin());
    if (raw_value.has_value()) {
      if (std::optional<ScreenType> type =
              DeferredScreenTypeFromInt(*raw_value)) {
        return *type;
      }
    }
  }
  return ScreenType::kStepsCompleted;
}

}  // namespace

@interface DeferredFirstRunCoordinator () <
    FirstRunScreenDelegate,
    HistorySyncCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation DeferredFirstRunCoordinator {
  // Navigation controller hosting the deferred screen sequence.
  UINavigationController* _navigationController;
  // Currently presented child screen coordinator.
  ChromeCoordinator* _childCoordinator;
  // `YES` while the coordinator is running between `-start` and `-stop`.
  BOOL _started;
  // `YES` once the flow has notified `promosUIHandler` and requested dismissal.
  BOOL _finished;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK_EQ(browser->type(), Browser::Type::kRegular);
  }
  return self;
}

- (void)dealloc {
  CHECK(!_navigationController);
  CHECK(!_childCoordinator);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  _started = YES;
  _navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

  // Populate the initial screen before presenting `_navigationController` to
  // avoid flashing an empty sheet. Child coordinators (e.g.,
  // `HistorySyncCoordinator` or `BestFeaturesScreenCoordinator`) can auto-skip
  // synchronously during `-start`; if every deferred screen skips,
  // `-presentNextScreen` sends `-hideDeferredFirstRunPromo`, whose handler
  // stops and releases `self` before returning. Dispatching via `weakSelf`
  // avoids dereferencing a potentially deallocated `self` and skips presenting
  // an empty modal.
  __weak __typeof(self) weakSelf = self;
  [self presentNextScreen];
  [weakSelf presentNavigationControllerIfNeeded];
}

- (void)stop {
  [self stopChildCoordinator];
  if (_started) {
    _started = NO;
    [self.promosUIHandler promoWasDismissed];
    self.promosUIHandler = nil;
  }
  _navigationController.presentationController.delegate = nil;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController = nil;
  [super stop];
}

#pragma mark - FirstRunScreenDelegate

- (void)firstRunScreenCoordinatorWantsToBeStopped:
    (ChromeCoordinator*)coordinator {
  CHECK_EQ(coordinator, _childCoordinator);
  [self stopChildCoordinator];
  [self presentNextScreen];
}

#pragma mark - HistorySyncCoordinatorDelegate

- (void)historySyncCoordinator:(HistorySyncCoordinator*)historySyncCoordinator
                    withResult:(HistorySyncResult)result {
  CHECK_EQ(_childCoordinator, historySyncCoordinator);
  [self firstRunScreenCoordinatorWantsToBeStopped:historySyncCoordinator];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self finishDeferredFirstRun];
}

#pragma mark - Private

// Presents `_navigationController` if the coordinator did not already stop
// synchronously during `-start`.
- (void)presentNavigationControllerIfNeeded {
  if (!_navigationController) {
    return;
  }
  _navigationController.presentationController.delegate = self;
  [_navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

// Pops the next deferred screen from `LocalState` and presents it, or finishes
// the flow on `ScreenType::kStepsCompleted`.
- (void)presentNextScreen {
  ScreenType type =
      PopNextDeferredScreen(GetApplicationContext()->GetLocalState());
  if (type == ScreenType::kStepsCompleted) {
    [self finishDeferredFirstRun];
    return;
  }
  [_navigationController setNavigationBarHidden:YES animated:NO];
  _childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [_childCoordinator start];
}

// Creates the child coordinator corresponding to `type`.
- (ChromeCoordinator*)createChildCoordinatorWithScreenType:(ScreenType)type {
  using enum ScreenType;
  switch (type) {
    case kHistorySync:
      return [[HistorySyncCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  delegate:self
                                  firstRun:NO
                             showUserEmail:YES
                                isOptional:YES
                              contextStyle:SigninContextStyle::kDefault
                               accessPoint:signin_metrics::AccessPoint::
                                               kStartPage];
    case kDefaultBrowserPromo:
      return [[DefaultBrowserScreenCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  firstRun:NO
                                  delegate:self];
    case kBestFeatures:
      return [[BestFeaturesScreenCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  firstRun:NO
                                  delegate:self];
    case kLensInteractivePromo: {
      InteractiveLensPromoCoordinator* coordinator =
          [[InteractiveLensPromoCoordinator alloc]
              initWithBaseNavigationController:_navigationController
                                       browser:self.browser
                                      firstRun:NO];
      coordinator.firstRunDelegate = self;
      return coordinator;
    }
    case kLensAnimatedPromo: {
      AnimatedLensPromoCoordinator* coordinator =
          [[AnimatedLensPromoCoordinator alloc]
              initWithBaseNavigationController:_navigationController
                                       browser:self.browser
                                      firstRun:NO];
      coordinator.firstRunDelegate = self;
      return coordinator;
    }
    case kSignIn:
    case kChoice:
    case kSyncedSetUp:
    case kGuidedTour:
    case kSafariImport:
    case kStepsCompleted:
      NOTREACHED();
  }
}

// Stops the active child coordinator.
- (void)stopChildCoordinator {
  [_childCoordinator stop];
  _childCoordinator = nil;
}

// Notifies `promosUIHandler`, then sends `-hideDeferredFirstRunPromo` so that
// the `PromosManagerCommands` handler stops and releases `self`.
- (void)finishDeferredFirstRun {
  if (_finished) {
    return;
  }
  _finished = YES;
  id<PromosManagerUIHandler> promosUIHandler = self.promosUIHandler;
  self.promosUIHandler = nil;
  id<PromosManagerCommands> promosManagerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PromosManagerCommands);
  [promosUIHandler promoWasDismissed];
  [promosManagerHandler hideDeferredFirstRunPromo];
}

@end
