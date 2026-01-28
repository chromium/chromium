// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/debugger/composebox_debugger_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/aim/debugger/coordinator/aim_debugger_coordinator.h"
#import "ios/chrome/browser/composebox/debugger/composebox_debugger_breadcrumbs_view_controller.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The size of the options button.
const CGSize kOptionsButtonSize = {80.0f, 40.0f};

}  // namespace

@interface ComposeboxDebuggerCoordinator () <AimDebuggerPresenter> {
  NSMutableArray<ComposeboxDebuggerEvent*>* _events;
}

@end

@implementation ComposeboxDebuggerCoordinator {
  UIButton* _optionsButton;
  // The coordinator for the AIM debugger.
  AimDebuggerCoordinator* _aimDebuggerCoordinator;
}

- (void)start {
  CHECK(experimental_flags::IsOmniboxDebuggingEnabled());
  [self setupOptionsButton];
  [self setupOptionsMenu];
  _events = [[NSMutableArray alloc] init];
}

- (void)stop {
  [self dismissAimDebuggerWithAnimation:NO];
}

- (void)logEvent:(ComposeboxDebuggerEvent*)event {
  [_events addObject:event];
}

#pragma mark - private

- (void)setupOptionsButton {
  _optionsButton = [[UIButton alloc] init];

  UIButtonConfiguration* config =
      [UIButtonConfiguration filledButtonConfiguration];
  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:20
                          weight:UIImageSymbolWeightMedium];
  UIImage* icon =
      DefaultSymbolWithConfiguration(@"wrench.and.screwdriver", symbolConfig);

  config.image = icon;
  config.baseForegroundColor = [UIColor whiteColor];
  config.baseBackgroundColor =
      [[UIColor systemBlueColor] colorWithAlphaComponent:0.6];
  _optionsButton = [UIButton buttonWithConfiguration:config primaryAction:nil];
  _optionsButton.layer.cornerRadius = 10;

  [self.baseViewController.view addSubview:_optionsButton];
  _optionsButton.frame =
      CGRectMake(48, 300, kOptionsButtonSize.width, kOptionsButtonSize.height);

  UIPanGestureRecognizer* panGesture =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handlePan:)];
  [_optionsButton addGestureRecognizer:panGesture];
}

- (void)setupOptionsMenu {
  __weak __typeof(self) weakSelf = self;
  UIAction* breadcrumbsAction = [UIAction
      actionWithTitle:@"Composebox logs"
                image:DefaultSymbolWithPointSize(@"binoculars.circle", 16)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf showBreadcrumbsLogs];
              }];
  UIAction* aimEligibilityDebuggerAction = [UIAction
      actionWithTitle:@"AIM Eligibility"
                image:CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol, 16)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf startAIMDebugger];
              }];

  UIAction* omniboxDebuggerAction = [UIAction
      actionWithTitle:@"Omnibox debugger"
                image:DefaultSymbolWithPointSize(@"ladybug.circle.fill", 16)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf
                        .delegate composeboxDebuggerDidRequestOmniboxDebugging];
              }];

  UIMenu* menu = [UIMenu menuWithTitle:@"Debugging options"
                              children:@[
                                aimEligibilityDebuggerAction, breadcrumbsAction,
                                omniboxDebuggerAction
                              ]];

  _optionsButton.showsMenuAsPrimaryAction = YES;
  _optionsButton.preferredMenuElementOrder =
      UIContextMenuConfigurationElementOrderFixed;
  _optionsButton.menu = menu;
}

- (void)showBreadcrumbsLogs {
  UIViewController* breadcrumbsViewController =
      [[ComposeboxDebuggerBreadcrumbsViewController alloc]
          initWithEvents:_events];

  [self.baseViewController presentViewController:breadcrumbsViewController
                                        animated:YES
                                      completion:nil];
}

- (void)handlePan:(UIPanGestureRecognizer*)gesture {
  UIView* draggedButton = gesture.view;
  UIView* containerView = self.baseViewController.view;
  CGPoint translation = [gesture translationInView:containerView];

  if (gesture.state == UIGestureRecognizerStateChanged) {
    CGPoint newCenter = CGPointMake(draggedButton.center.x + translation.x,
                                    draggedButton.center.y + translation.y);

    // Optional: Keep button within screen bounds
    CGFloat midX = CGRectGetMidX(draggedButton.bounds);
    CGFloat midY = CGRectGetMidY(draggedButton.bounds);

    newCenter.x =
        MAX(midX, MIN(containerView.bounds.size.width - midX, newCenter.x));
    newCenter.y =
        MAX(midY, MIN(containerView.bounds.size.height - midY, newCenter.y));

    draggedButton.center = newCenter;

    [gesture setTranslation:CGPointZero inView:containerView];
  }
}

- (void)startAIMDebugger {
  [self dismissAimDebuggerWithAnimation:NO];
  _aimDebuggerCoordinator = [[AimDebuggerCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  _aimDebuggerCoordinator.presenter = self;
  [_aimDebuggerCoordinator start];
}

#pragma mark - AimDebuggerPresenter

- (void)dismissAimDebuggerWithAnimation:(BOOL)animated {
  if (animated) {
    [_aimDebuggerCoordinator stopAnimatedWithCompletion:nil];
  } else {
    [_aimDebuggerCoordinator stop];
  }
  _aimDebuggerCoordinator = nil;
}

@end
