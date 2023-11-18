// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/notifications_promo_view.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/notifications_promo_view_constants.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Margins for the close button.
constexpr CGFloat kCloseButtonTrailingMargin = -8.0;
constexpr CGFloat kCloseButtonTopMargin = 8.0;
// Size for the close button width and height.
constexpr CGFloat kCloseButtonWidthHeight = 24;
}  // namespace

@interface NotificationsPromoView ()
// Redeclare as read-write.
@property(nonatomic, strong, readwrite) UIButton* closeButton;

@end

@implementation NotificationsPromoView {
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _closeButton = [self createCloseButton];
    [self addSubview:_closeButton];

    // Constraints that apply to all styles.
    [NSLayoutConstraint activateConstraints:@[
      // Close button size constraints.
      [_closeButton.heightAnchor
          constraintEqualToConstant:kCloseButtonWidthHeight],
      [_closeButton.widthAnchor
          constraintEqualToConstant:kCloseButtonWidthHeight],
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:kCloseButtonTrailingMargin],
      [_closeButton.topAnchor constraintEqualToAnchor:self.topAnchor
                                             constant:kCloseButtonTopMargin],
      [_closeButton.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-kCloseButtonTopMargin],
    ]];
  }
  return self;
}

#pragma mark - Private

// Creates the close button for the Promo.
- (UIButton*)createCloseButton {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.accessibilityIdentifier = kNotificationsPromoCloseButtonId;
  [button addTarget:self
                action:@selector(onCloseButtonAction:)
      forControlEvents:UIControlEventTouchUpInside];
  [button setImage:[UIImage imageNamed:@"signin_promo_close_gray"]
          forState:UIControlStateNormal];
  button.hidden = NO;
  button.pointerInteractionEnabled = YES;
  return button;
}

- (void)accessibilityCloseAction:(id)unused {
  DCHECK(self.closeButton.enabled);
  [self.closeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

// Handles close button action.
- (void)onCloseButtonAction:(id)unused {
}

@end
