// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_with_bricks_view_controller.h"

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_configuration.h"
#import "ios/chrome/browser/shared/ui/bricks/animation_brick.h"
#import "ios/chrome/browser/shared/ui/bricks/layout_brick.h"
#import "ios/chrome/browser/shared/ui/bricks/title_brick.h"
#import "ui/base/device_form_factor.h"

namespace {

// Constant for the stackView top constraint.
constexpr CGFloat kTopConstraintConstant = 32;

// Vertical center offset for tablets.
constexpr CGFloat kTabletCenterOffset = 40;

}  // namespace

@implementation AnimatedPromoWithBricksViewController {
  // View controller's configuration.
  AnimatedPromoConfiguration* _config;
  // Animation container.
  UIView* _animationView;
  // Container for the title and subtitle.
  UIView* _titleView;
  // StackView for the _animationView, _titleView and underTitleView.
  UIStackView* _stackView;
  // Top constraint for the stack view, updated when changing vertical size
  // class.
  NSLayoutConstraint* _stackViewTopConstraint;
}

- (instancetype)initWithConfiguration:(AnimatedPromoConfiguration*)config {
  self = [super initWithConfiguration:config.buttonStackConfiguration];
  if (self) {
    _config = config;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  _animationView = [[AnimationBrick alloc]
      initWithConfiguration:_config.animationBrickConfiguration];
  _animationView.translatesAutoresizingMaskIntoConstraints = NO;
  [self configureAnimationView];

  NSMutableArray* stackSubviews = [[NSMutableArray alloc] init];

  if (_config.titleBrickConfiguration) {
    _titleView = [[TitleBrick alloc]
        initWithConfiguration:_config.titleBrickConfiguration];
    [stackSubviews addObject:_titleView];
  }

  if (_config.underTitleView) {
    [stackSubviews addObject:_config.underTitleView];
  }

  _stackView = [LayoutBrick contentStackWithSubviews:stackSubviews];
  [self configureStackView];
  [self
      registerForTraitChanges:@[ UITraitVerticalSizeClass.class ]
                   withAction:@selector(
                                  configureStackViewTopConstraintForSizeClass)];
}

#pragma mark - Private

// Updates the _stackView top constraint, called when the size class changes.
- (void)configureStackViewTopConstraintForSizeClass {
  _stackViewTopConstraint.active = NO;

  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    _stackViewTopConstraint =
        [_stackView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                             constant:kTopConstraintConstant];
  } else {
    _stackViewTopConstraint = [_stackView.topAnchor
        constraintEqualToAnchor:_animationView.bottomAnchor
                       constant:kTopConstraintConstant];
  }
  _stackViewTopConstraint.active = YES;
}

// Initializes and configures the _stackView.
- (void)configureStackView {
  [self.contentView addSubview:_stackView];

  [NSLayoutConstraint activateConstraints:@[
    [_stackView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor],
    [_stackView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor],
    [_stackView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor]
  ]];

  [self configureStackViewTopConstraintForSizeClass];
}

// Configures the _animationView.
- (void)configureAnimationView {
  [self.contentView addSubview:_animationView];
  [NSLayoutConstraint activateConstraints:@[
    [_animationView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_animationView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_animationView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor],
    [_animationView.bottomAnchor constraintEqualToAnchor:self.view.centerYAnchor
                                                constant:[self centerYOffset]],
  ]];
}

// The offset from center Y to place the divider between the animation and the
// stackView.
- (CGFloat)centerYOffset {
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? -kTabletCenterOffset
             : 0;
}

@end
