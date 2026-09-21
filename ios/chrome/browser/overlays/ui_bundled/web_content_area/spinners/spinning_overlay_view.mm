// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_view.h"

#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_view_delegate.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Alpha value for the background view.
constexpr CGFloat kOverlayViewBackgroundAlpha = 0.6;

// Width of the label displayed on the view as a percentage of the view's width.
constexpr CGFloat kOverlayLabelWidthPercentage = 0.7;

// Top margin to prevent the label from overflooding the top.
constexpr CGFloat kOverlayLabelTopMargin = 16.0;

// Bottom margin for the label displayed on the view.
constexpr CGFloat kOverlayLabelCenterOffset = 60.0;

// Shadow offset for the overlay label.
constexpr CGSize kLabelShadowOffset = {0.0, 1.0};

}  // namespace

@interface SpinningOverlayView () <UIGestureRecognizerDelegate>
@end

@implementation SpinningOverlayView {
  // Whether user taps cancel the overlay.
  BOOL _cancellable;
}

- (instancetype)initWithLabelText:(NSString*)labelText
                      cancellable:(BOOL)cancellable {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _cancellable = cancellable;
    [self setUpViewsWithLabelText:labelText];
    self.accessibilityViewIsModal = YES;
  }
  return self;
}

#pragma mark - UIView

- (void)didMoveToWindow {
  [super didMoveToWindow];
  if (self.window) {
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    self);
  }
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  return YES;
}

#pragma mark - Private

// Sets up the child views and constraints.
- (void)setUpViewsWithLabelText:(NSString*)labelText {
  UIView* backgroundView = [[UIView alloc] init];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  backgroundView.backgroundColor = [UIColor colorNamed:kGrey800Color];
  backgroundView.alpha = kOverlayViewBackgroundAlpha;
  [self addSubview:backgroundView];
  AddSameConstraints(backgroundView, self);

  UIActivityIndicatorView* spinner = GetLargeUIActivityIndicatorView();
  spinner.translatesAutoresizingMaskIntoConstraints = NO;
  spinner.hidesWhenStopped = YES;
  spinner.userInteractionEnabled = NO;
  [spinner startAnimating];
  [self addSubview:spinner];
  AddSameCenterConstraints(spinner, self);

  if (labelText.length > 0) {
    [self setUpLabelWithText:labelText];
  }

  UITapGestureRecognizer* tapRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];
  tapRecognizer.delegate = self;
  [self addGestureRecognizer:tapRecognizer];
}

// Sets up the label with `text`.
- (void)setUpLabelWithText:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textColor = [UIColor whiteColor];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  label.numberOfLines = 0;
  label.shadowColor = [UIColor blackColor];
  label.shadowOffset = kLabelShadowOffset;
  label.backgroundColor = [UIColor clearColor];
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.textAlignment = NSTextAlignmentCenter;
  label.text = text;
  [self addSubview:label];

  [NSLayoutConstraint activateConstraints:@[
    [label.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.safeAreaLayoutGuide.topAnchor
                                    constant:kOverlayLabelTopMargin],
    [label.bottomAnchor constraintEqualToAnchor:self.centerYAnchor
                                       constant:-kOverlayLabelCenterOffset],
    [label.widthAnchor constraintEqualToAnchor:self.widthAnchor
                                    multiplier:kOverlayLabelWidthPercentage],
    [label.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
  ]];
}

// Handles tap gesture on the overlay.
- (void)handleTap:(UITapGestureRecognizer*)recognizer {
  if (_cancellable) {
    [self.delegate spinningOverlayViewDidTap:self];
  }
}

@end
