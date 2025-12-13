// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_container_view.h"

#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/time/time.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"

namespace {
// Duration for fade animations.
constexpr base::TimeDelta kFadeDuration = base::Milliseconds(500);
}  // namespace

@interface SearchEngineLogoContainerView ()

// Image view used for downloaded doodles.
@property(strong, nonatomic, readonly) UIImageView* doodleLogo;

// Handler called when this view is tapped.
- (void)logoWasTapped;

// Updates `doodleLogo`'s content mode to ensure that `doodle` is rendered
// properly for the given frame size.
- (void)updateDoodleContentModeForDoodle:(UIImage*)doodle;

@end

@implementation SearchEngineLogoContainerView {
  SearchEngineLogoState _logoState;
  UIImageView* _shrunkLogoView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    // Create logo view.
    _shrunkLogoView = [[UIImageView alloc] init];
    _shrunkLogoView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_shrunkLogoView];
    // TODO(crbug.com/1170491): Ideally the width anchor added so the
    // imageview frame matches the intrinsic size.
    [NSLayoutConstraint activateConstraints:@[
      [_shrunkLogoView.heightAnchor constraintEqualToAnchor:self.heightAnchor],
      [_shrunkLogoView.centerXAnchor
          constraintEqualToAnchor:self.centerXAnchor],
      [_shrunkLogoView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor]
    ]];

    // Create doodle view and add it to hierarchy.
    _doodleLogo = ios::provider::CreateAnimatedImageView();
    [self addSubview:_doodleLogo];
    [_doodleLogo setTranslatesAutoresizingMaskIntoConstraints:NO];
    AddSameConstraints(_doodleLogo, self);
    [_doodleLogo setContentMode:UIViewContentModeCenter];
    // Only add tap gesture recognizer to Doodle.
    [_doodleLogo setUserInteractionEnabled:YES];
    // Create tap handler.
    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(logoWasTapped)];
    [self addGestureRecognizer:tapRecognizer];

    // The doodle is originally hidden until `showingDoodle` is updated.
    [_doodleLogo setAlpha:0.0];
    _logoState = SearchEngineLogoState::kLogo;
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // Set the point size to match the height so that the symbol logo is sized to
  // fill the entire UIImageView height with a 1 pt margin on top and bottom.
  _shrunkLogoView.preferredSymbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:self.frame.size.height - 2];
}

#pragma mark Public

- (void)setLogoState:(SearchEngineLogoState)logoState animated:(BOOL)animated {
  if (_logoState == logoState) {
    return;
  }
  _logoState = logoState;
  BOOL showingDoodle = _logoState == SearchEngineLogoState::kDoodle;
  UIView* logoView = self.shrunkLogoView;
  DCHECK(logoView.superview);
  if (!showingDoodle) {
    [self.doodleLogo stopAnimating];
  }
  __weak UIView* viewToFadeOut = showingDoodle ? logoView : self.doodleLogo;
  __weak UIView* viewToFadeIn = showingDoodle ? self.doodleLogo : logoView;
  ProceduralBlock fadeOutAnimation = ^{
    [viewToFadeOut setAlpha:0.0];
  };
  ProceduralBlock fadeInAnimation = ^{
    [viewToFadeIn setAlpha:1.0];
  };
  if (animated) {
    ProceduralBlock animations = ^{
      [UIView addKeyframeWithRelativeStartTime:0.0
                              relativeDuration:0.5
                                    animations:fadeOutAnimation];
      [UIView addKeyframeWithRelativeStartTime:0.5
                              relativeDuration:0.5
                                    animations:fadeInAnimation];
    };
    [UIView animateKeyframesWithDuration:kFadeDuration.InSecondsF()
                                   delay:0.0
                                 options:0
                              animations:animations
                              completion:nil];
  } else {
    fadeOutAnimation();
    fadeInAnimation();
  }
}

- (void)setDoodleImage:(UIImage*)image
              animated:(BOOL)animated
            animations:(ProceduralBlock)animations {
  __weak __typeof(self) weakSelf = self;
  [UIView transitionWithView:self
                    duration:animated ? kFadeDuration.InSecondsF() : 0.0
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    if (animations) {
                      animations();
                    }
                    [weakSelf updateDoodleContentModeForDoodle:image];
                    weakSelf.doodleLogo.image = image;
                  }
                  completion:nil];
}

- (void)setAnimatedDoodleImage:(UIImage*)image animated:(BOOL)animated {
  [self updateDoodleContentModeForDoodle:image];
  __weak __typeof(self) weakSelf = self;
  [UIView transitionWithView:self.doodleLogo
      duration:animated ? kFadeDuration.InSecondsF() : 0.0
      options:UIViewAnimationOptionTransitionCrossDissolve
      animations:^{
        weakSelf.doodleLogo.image = image;
      }
      completion:^(BOOL finished) {
        [weakSelf.doodleLogo startAnimating];
      }];
}

#pragma mark Accessors

- (SearchEngineLogoState)logoState {
  return _logoState;
}

- (BOOL)isAnimatingDoodle {
  return [self.doodleLogo isAnimating];
}

- (UIImageView*)shrunkLogoView {
  return _shrunkLogoView;
}

- (void)setDoodleAltText:(NSString*)doodleAltText {
  if ([_doodleAltText isEqual:doodleAltText]) {
    return;
  }
  _doodleAltText = [doodleAltText copy];
  [self.doodleLogo setAccessibilityLabel:_doodleAltText];
  [self.doodleLogo setIsAccessibilityElement:[_doodleAltText length] > 0];
}

#pragma mark - UIAccessibility

- (BOOL)accessibilityRespondsToUserInteraction {
  // The logo doesn't have any actual behavior when tapped; it only tracks a
  // metric. To simplify accessibility, don't allow Voice Control to tap a
  // logo.
  return _logoState == SearchEngineLogoState::kDoodle;
}

#pragma mark Internal

- (void)logoWasTapped {
  if (_logoState == SearchEngineLogoState::kDoodle) {
    base::RecordAction(base::UserMetricsAction("IOS.NTP.Doodle.Tapped"));
    [self.delegate searchEngineLogoContainerViewDoodleWasTapped:self];
  } else {
    // For now nothing happens when tapping the logo, but record a metric to
    // see if anything interesting is happening here.
    base::RecordAction(base::UserMetricsAction("MobileNTPLogoTapped"));
  }
}

- (void)updateDoodleContentModeForDoodle:(UIImage*)doodle {
  if (!doodle) {
    return;
  }
  // Sometimes GWS sends down a doodle that is too big. In that case, shrink
  // the doodle image with content mode ScaleAspectFit. If the doodle is smaller
  // than the doodle container, set content mode to Center, which will show
  // the doodle at its original size, centered.
  CGSize maxSize = self.doodleLogo.bounds.size;
  if (doodle.size.width > maxSize.width ||
      doodle.size.height > maxSize.height) {
    self.doodleLogo.contentMode = UIViewContentModeScaleAspectFit;
  } else {
    self.doodleLogo.contentMode = UIViewContentModeCenter;
  }
}

@end
