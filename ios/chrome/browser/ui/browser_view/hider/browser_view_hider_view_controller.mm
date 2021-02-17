// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/hider/browser_view_hider_view_controller.h"

#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/ui/colors/dynamic_color_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

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
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/981889): When iOS 12 is dropped, only the next line is
    // needed for styling. Every other check for |incognitoStyle| can be
    // removed, as well as the incognito specific assets.
    self.view.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }
  UIColor* backgroundColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kSecondaryBackgroundColor], true,
      [UIColor colorNamed:kSecondaryBackgroundDarkColor]);
  self.view.backgroundColor = backgroundColor;
  self.view.accessibilityIdentifier = @"BrowserViewHiderView";
  self.view.layer.cornerRadius = kTopCornerRadius;
  self.view.hidden = YES;

  [self.view addGestureRecognizer:[[UITapGestureRecognizer alloc]
                                      initWithTarget:self
                                              action:@selector(handleTap:)]];

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

- (void)handleTap:(UITapGestureRecognizer*)sender {
  if (sender.state != UIGestureRecognizerStateEnded) {
    return;
  }
  [self.panGestureHandler setNextState:ViewRevealState::Hidden animated:YES];
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
  NSString* ntpText =
      self.incognito
          ? l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_INCOGNITO_TAB_PAGE)
          : l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_TAB_PAGE);
  [self.steadyView setLocationLabelText:ntpText];
}

#pragma mark - ViewRevealingAnimatee

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
