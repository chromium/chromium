// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_button.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_button_factory.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_item.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_view_visibility_delegate.h"
#import "ios/chrome/browser/infobars/model/badge_state.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface IncognitoBadgeViewController ()

// BadgeButton to show in both FullScreen and non FullScreen.
@property(nonatomic, strong) BadgeButton* incognitoBadge;

@end

@implementation IncognitoBadgeViewController {
  // Button factory.
  BadgeButtonFactory* _buttonFactory;

  // StackView holding the incognitoBadge.
  UIStackView* _stackView;
}

@synthesize disabled = _disabled;

- (instancetype)initWithButtonFactory:(BadgeButtonFactory*)buttonFactory {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    DCHECK(buttonFactory);
    _buttonFactory = buttonFactory;
    _stackView = [[UIStackView alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  _stackView.axis = UILayoutConstraintAxisHorizontal;
  [self.view addSubview:_stackView];
  AddSameConstraints(self.view, _stackView);
}

#pragma mark - Protocols

#pragma mark IncognitoBadgeConsumer

- (void)setupWithIncognitoBadge:(id<BadgeItem>)incognitoBadgeItem {
  CHECK(incognitoBadgeItem);
  self.incognitoBadge =
      [_buttonFactory badgeButtonForBadgeType:incognitoBadgeItem.badgeType
                                 usingInfoBar:nil];
  [self.visibilityDelegate setIncognitoBadgeViewHidden:NO];
}

- (void)setDisabled:(BOOL)disabled {
  if (_disabled == disabled) {
    return;
  }

  if (disabled) {
    [self.visibilityDelegate setIncognitoBadgeViewHidden:YES];
  } else {
    // Turning off force disable mode doesn't imply that the badge view will
    // not remain hidden. Check if there is a badge to be displayed to avoid
    // accidentally removing the placeholder as a side effect of unhiding.
    [self.visibilityDelegate setIncognitoBadgeViewHidden:!self.incognitoBadge];
  }

  _disabled = disabled;
}

#pragma mark FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  BOOL badgeViewShouldCollapse = progress <= kFullscreenProgressThreshold;
  if (badgeViewShouldCollapse) {
    self.incognitoBadge.fullScreenOn = YES;
    // Fade in/out in the FullScreen badge with the FullScreen on
    // configurations.
    CGFloat alphaValue = fmax((kFullscreenProgressThreshold - progress) /
                                  kFullscreenProgressThreshold,
                              0);
    self.incognitoBadge.alpha = alphaValue;
  } else {
    self.incognitoBadge.fullScreenOn = NO;
    // Fade in/out the FullScreen badge with the FullScreen off configurations
    // at a speed matching that of the trailing button in the
    // LocationBarSteadyView.
    CGFloat alphaValue = fmax((progress - kFullscreenProgressThreshold) /
                                  (1 - kFullscreenProgressThreshold),
                              0);
    self.incognitoBadge.alpha = alphaValue;
  }
}

#pragma mark - Getter/Setter

- (void)setIncognitoBadge:(BadgeButton*)incognitoBadge {
  CHECK(incognitoBadge);
  [_stackView removeArrangedSubview:_incognitoBadge];
  [_incognitoBadge removeFromSuperview];
  _incognitoBadge = incognitoBadge;
  [_stackView addArrangedSubview:_incognitoBadge];
}

@end
