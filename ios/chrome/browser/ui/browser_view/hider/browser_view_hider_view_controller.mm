// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/hider/browser_view_hider_view_controller.h"

#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserViewHiderViewController ()

// Gesture recognizer for swipes on this view.
@property(nonatomic, strong) UIPanGestureRecognizer* panGestureRecognizer;

@property(nonatomic, strong) LocationBarSteadyView* steadyView;

@end

@implementation BrowserViewHiderViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundDarkColor];
  self.view.accessibilityIdentifier = @"BrowserViewHiderView";
  self.view.layer.cornerRadius = kTopCornerRadius;
  self.view.hidden = YES;

  self.steadyView = [[LocationBarSteadyView alloc] init];
  self.steadyView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.steadyView];
  self.steadyView.colorScheme =
      [LocationBarSteadyViewColorScheme incognitoScheme];
  self.steadyView.locationButton.enabled = NO;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];

  NamedGuide* guide = [NamedGuide guideWithName:kPrimaryToolbarLocationViewGuide
                                           view:self.view];
  AddSameConstraints(guide, self.steadyView);
}

- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler {
  _panGestureHandler = panGestureHandler;
  [self.view removeGestureRecognizer:self.panGestureRecognizer];

  UIPanGestureRecognizer* panGestureRecognizer = [[UIPanGestureRecognizer alloc]
      initWithTarget:panGestureHandler
              action:@selector(handlePanGesture:)];
  panGestureRecognizer.delegate = panGestureHandler;
  panGestureRecognizer.maximumNumberOfTouches = 1;
  [self.view addGestureRecognizer:panGestureRecognizer];

  self.panGestureRecognizer = panGestureRecognizer;
}

#pragma mark - LocationBarSteadyViewConsumer

- (void)updateLocationText:(NSString*)string clipTail:(BOOL)clipTail {
  [self.steadyView setLocationLabelText:string];
  self.steadyView.locationLabel.lineBreakMode =
      clipTail ? NSLineBreakByTruncatingTail : NSLineBreakByTruncatingHead;
}

- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText {
  [self.steadyView setLocationImage:icon];
  self.steadyView.securityLevelAccessibilityString = statusText;
}

- (void)updateLocationShareable:(BOOL)shareable {
  // No-op. The share button should never be visible on the hider view.
}

- (void)updateAfterNavigatingToNTP {
  [self.steadyView setLocationLabelText:@""];
}

#pragma mark - viewRevealingAnimatee

- (void)willAnimateViewRevealFromState:(ViewRevealState)currentViewRevealState
                               toState:(ViewRevealState)nextViewRevealState {
  self.view.alpha = currentViewRevealState == ViewRevealState::Revealed ? 1 : 0;
  self.view.hidden = NO;
}

- (void)animateViewReveal:(ViewRevealState)nextViewRevealState {
  switch (nextViewRevealState) {
    case ViewRevealState::Hidden:
      self.view.alpha = 0;
      break;
    case ViewRevealState::Peeked:
      self.view.alpha = 0;
      break;
    case ViewRevealState::Revealed:
      self.view.alpha = 1;
      break;
  }
}

- (void)didAnimateViewReveal:(ViewRevealState)viewRevealState {
  self.view.hidden = viewRevealState != ViewRevealState::Revealed;
}

@end
