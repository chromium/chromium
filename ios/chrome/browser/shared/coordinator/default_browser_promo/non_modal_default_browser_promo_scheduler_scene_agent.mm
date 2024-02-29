// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"

#import "base/notreached.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_metrics_util.h"

namespace {

NonModalPromoTriggerType MetricTypeForPromoReason(PromoReason reason) {
  switch (reason) {
    case PromoReasonNone:
      return NonModalPromoTriggerType::kUnknown;
    case PromoReasonOmniboxPaste:
      return NonModalPromoTriggerType::kPastedLink;
    case PromoReasonExternalLink:
      return NonModalPromoTriggerType::kGrowthKitOpen;
    case PromoReasonShare:
      return NonModalPromoTriggerType::kShare;

    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

@interface NonModalDefaultBrowserPromoSchedulerSceneAgent () {
  __weak id<DefaultBrowserPromoNonModalCommands> _handler;
  NSInteger _userInteractionWithNonModalPromoCount;
  NSInteger _displayedFullscreenPromoCount;
}
@end

@implementation NonModalDefaultBrowserPromoSchedulerSceneAgent

- (bool)promoCanBeDisplayed {
  if (IsChromeLikelyDefaultBrowser()) {
    return false;
  }

  if (IsNonModalDefaultBrowserPromoCooldownRefactorEnabled() &&
      UserInNonModalPromoCooldown()) {
    return false;
  }

  if (!IsNonModalDefaultBrowserPromoCooldownRefactorEnabled() &&
      UserInFullscreenPromoCooldown()) {
    return false;
  }

  NSInteger count = UserInteractionWithNonModalPromoCount();
  return count < GetNonModalDefaultBrowserPromoImpressionLimit();
}

- (void)resetPromoHandler {
  _handler = nil;
}

- (void)initPromoHandler:(Browser*)browser {
  _handler = HandlerForProtocol(browser->GetCommandDispatcher(),
                                DefaultBrowserPromoNonModalCommands);
}

- (void)notifyHandlerShowPromo {
  // The count of past non-modal promo interactions and fullscreen promo
  // displays is cached because multiple interactions may be logged for the
  // current non-modal promo impression. This makes sure we don't over-increment
  // the interactions count value.
  _userInteractionWithNonModalPromoCount =
      UserInteractionWithNonModalPromoCount();
  _displayedFullscreenPromoCount = DisplayedFullscreenPromoCount();

  [_handler showDefaultBrowserNonModalPromo];
}

- (void)notifyHandlerDismissPromo:(BOOL)animated {
  [_handler dismissDefaultBrowserNonModalPromoAnimated:animated];
}

- (void)onEnteringBackground:(PromoReason)currentPromoReason
              promoIsShowing:(bool)promoIsShowing {
  if (currentPromoReason != PromoReasonNone && !promoIsShowing) {
    LogNonModalPromoAction(NonModalPromoAction::kBackgroundCancel,
                           MetricTypeForPromoReason(currentPromoReason),
                           _userInteractionWithNonModalPromoCount);
  }
  [super cancelTimerAndPromoOnBackground];
}

- (void)onEnteringForeground {
}

- (void)logPromoAppear:(PromoReason)currentPromoReason {
  LogNonModalPromoAction(NonModalPromoAction::kAppear,
                         MetricTypeForPromoReason(currentPromoReason),
                         _userInteractionWithNonModalPromoCount);
}

- (void)logPromoAction:(PromoReason)currentPromoReason
        promoShownTime:(base::TimeTicks)promoShownTime {
  LogNonModalPromoAction(NonModalPromoAction::kAccepted,
                         MetricTypeForPromoReason(currentPromoReason),
                         _userInteractionWithNonModalPromoCount);
  LogNonModalTimeOnScreen(promoShownTime);
  LogUserInteractionWithNonModalPromo(_userInteractionWithNonModalPromoCount,
                                      _displayedFullscreenPromoCount);

  NSURL* settingsURL = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
  [[UIApplication sharedApplication] openURL:settingsURL
                                     options:@{}
                           completionHandler:nil];
}

- (void)logPromoUserDismiss:(PromoReason)currentPromoReason
             promoShownTime:(base::TimeTicks)promoShownTime {
  LogNonModalPromoAction(NonModalPromoAction::kDismiss,
                         MetricTypeForPromoReason(currentPromoReason),
                         _userInteractionWithNonModalPromoCount);
  LogNonModalTimeOnScreen(promoShownTime);
  LogUserInteractionWithNonModalPromo(_userInteractionWithNonModalPromoCount,
                                      _displayedFullscreenPromoCount);
}

- (void)logPromoTimeout:(PromoReason)currentPromoReason
         promoShownTime:(base::TimeTicks)promoShownTime {
  LogNonModalPromoAction(NonModalPromoAction::kTimeout,
                         MetricTypeForPromoReason(currentPromoReason),
                         _userInteractionWithNonModalPromoCount);
  LogNonModalTimeOnScreen(promoShownTime);
  LogUserInteractionWithNonModalPromo(_userInteractionWithNonModalPromoCount,
                                      _displayedFullscreenPromoCount);
}

@end
