// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/test/fullscreen_app_interface.h"

#import <algorithm>
#import <cmath>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

namespace {

// Touch injection parameters for `dragWithoutDecelerationInDirection:amount:`.
constexpr CGFloat kStepDistance = 3.0;
constexpr int kMaxMoveSteps = 100;
constexpr int kHoldSteps = 10;
constexpr NSTimeInterval kTouchTimeout = 10.0;

}  // namespace

@implementation FullscreenAppInterface

+ (UIEdgeInsets)currentViewportInsets {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  if (!webState) {
    return UIEdgeInsetsZero;
  }

  if (IsFullscreenRefactoringEnabled()) {
    return webState->GetWebViewProxy().obscuredInsets;
  }

  Browser* browser = chrome_test_util::GetCurrentBrowser();
  if (!browser) {
    return UIEdgeInsetsZero;
  }
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(browser);
  if (!fullscreenController) {
    return UIEdgeInsetsZero;
  }
  return fullscreenController->GetCurrentViewportInsets();
}

+ (UIEdgeInsets)currentWindowSafeArea {
  UIWindow* keyWindow = GetAnyKeyWindow();
  return keyWindow ? keyWindow.safeAreaInsets : UIEdgeInsetsZero;
}

+ (BOOL)isFullscreenRefactoringEnabled {
  return IsFullscreenRefactoringEnabled();
}

+ (BOOL)isScrolledToBottom {
  Browser* browser = chrome_test_util::GetCurrentBrowser();
  if (!browser) {
    return NO;
  }

  if (IsFullscreenRefactoringEnabled()) {
    id target = [browser->GetCommandDispatcher()
        forwardingTargetForSelector:@selector(exitForceFullscreen)];
    FullscreenMediator* mediator =
        base::apple::ObjCCast<FullscreenMediator>(target);
    return [mediator isScrolledToBottomForTesting];
  }

  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(browser);
  if (!fullscreenController) {
    return NO;
  }
  return fullscreenController->IsScrolledToBottomForTesting();
}

+ (id<GREYAction>)dragWithoutDecelerationInDirection:(GREYDirection)direction
                                              amount:(CGFloat)amount {
  return [GREYActionBlock
      actionWithName:[NSString
                         stringWithFormat:@"Drag without deceleration %ld %f",
                                          static_cast<long>(direction), amount]
         constraints:nil
        performBlock:^(UIView* element, NSError* __strong* errorOrNil) {
          UIWindow* window = [element window];
          if (window == nil) {
            NSString* errorDescription = [NSString
                stringWithFormat:
                    @"Cannot drag on this view as it has no window and "
                    @"isn't a window itself:\n%@",
                    [element grey_description]];
            *errorOrNil = [NSError
                errorWithDomain:@"No window available"
                           code:0
                       userInfo:@{@"Failure Reason" : (errorDescription)}];
            return NO;
          }
          CGFloat unitX = 0;
          CGFloat unitY = 0;
          switch (direction) {
            case kGREYDirectionDown:
              unitY = 1.0;
              break;
            case kGREYDirectionUp:
              unitY = -1.0;
              break;
            case kGREYDirectionLeft:
              unitX = -1.0;
              break;
            case kGREYDirectionRight:
              unitX = 1.0;
              break;
          }
          CGRect accessibilityFrame = element.accessibilityFrame;
          CGPoint startPoint = CGPointMake(CGRectGetMidX(accessibilityFrame),
                                           CGRectGetMidY(accessibilityFrame));
          CGPoint currentPoint = startPoint;

          UIScrollView* scrollView =
              base::apple::ObjCCast<UIScrollView>(element);
          const CGPoint initialOffset =
              scrollView ? scrollView.contentOffset : CGPointZero;
          web::WebState* webState = chrome_test_util::GetCurrentWebState();
          UIView* webStateView = webState ? webState->GetView() : nil;
          const CGFloat initialWebStateViewOriginY =
              webStateView ? webStateView.frame.origin.y : 0.0;
          UIView* scrollViewSuperview = scrollView ? scrollView.superview : nil;
          const CGFloat initialScrollViewSuperviewOriginY =
              scrollViewSuperview ? scrollViewSuperview.frame.origin.y : 0.0;

          GREYSyntheticEvents* eventGenerator =
              [[GREYSyntheticEvents alloc] init];
          [eventGenerator beginTouchAtPoint:currentPoint
                           relativeToWindow:window
                          immediateDelivery:YES
                                    timeout:kTouchTimeout];

          for (int step = 0; step < kMaxMoveSteps; ++step) {
            CGFloat progressedDistance = 0;
            if (scrollView) {
              progressedDistance =
                  std::fabs(scrollView.contentOffset.x - initialOffset.x) +
                  std::fabs(scrollView.contentOffset.y - initialOffset.y);
              if (webStateView) {
                progressedDistance += std::fabs(webStateView.frame.origin.y -
                                                initialWebStateViewOriginY);
              }
              if (scrollViewSuperview && scrollViewSuperview != webStateView) {
                progressedDistance +=
                    std::fabs(scrollViewSuperview.frame.origin.y -
                              initialScrollViewSuperviewOriginY);
              }
            } else {
              progressedDistance = std::hypot(currentPoint.x - startPoint.x,
                                              currentPoint.y - startPoint.y);
            }
            if (progressedDistance >= amount) {
              break;
            }
            currentPoint.x += unitX * kStepDistance;
            currentPoint.y += unitY * kStepDistance;
            [eventGenerator continueTouchAtPoint:currentPoint
                               immediateDelivery:YES
                                         timeout:kTouchTimeout];
          }

          // Hold stationary at the final touch point so pan velocity decays to
          // zero before lifting the finger.
          for (int i = 0; i < kHoldSteps; ++i) {
            [eventGenerator continueTouchAtPoint:currentPoint
                               immediateDelivery:YES
                                         timeout:kTouchTimeout];
          }
          [eventGenerator endTouchWithTimeout:kTouchTimeout];
          return YES;
        }];
}

@end
