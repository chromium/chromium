// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_with_bricks_view_controller.h"

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_configuration.h"
#import "ios/chrome/browser/shared/ui/bricks/animation_brick.h"
#import "ios/chrome/browser/shared/ui/bricks/layout_brick.h"
#import "ios/chrome/browser/shared/ui/bricks/title_brick.h"
#import "ios/chrome/common/ui/util/ui_util.h"

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
  // Center Y constraint for the animation view, updated when window traits
  // change.
  NSLayoutConstraint* _animationViewCenterYConstraint;
  // Registration for window trait changes.
  id<UITraitChangeRegistration> _windowTraitChangeRegistration;
  // The window on which `_windowTraitChangeRegistration` is registered.
  __weak UIWindow* _observedWindow;
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

- (void)viewIsAppearing:(BOOL)animated {
  [super viewIsAppearing:animated];
  [self updateAnimationViewCenterYConstraint];
  if (self.view.window && self.view.window != _observedWindow) {
    [_observedWindow unregisterForTraitChanges:_windowTraitChangeRegistration];
    _observedWindow = self.view.window;
    __weak __typeof(self) weakSelf = self;
    _windowTraitChangeRegistration = [_observedWindow
        registerForTraitChanges:@[
          UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class
        ]
                    withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                  UITraitCollection* previousCollection) {
                      [weakSelf updateAnimationViewCenterYConstraint];
                    }];
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if (_windowTraitChangeRegistration) {
    [_observedWindow unregisterForTraitChanges:_windowTraitChangeRegistration];
    _windowTraitChangeRegistration = nil;
    _observedWindow = nil;
  }
}

#pragma mark - Private

// Updates the _stackView top constraint, called when the vertical size class
// changes.
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

// Updates the animation view's center Y constraint based on the window's size
// class.
- (void)updateAnimationViewCenterYConstraint {
  _animationViewCenterYConstraint.constant = [self centerYOffset];
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
  _animationViewCenterYConstraint = [_animationView.bottomAnchor
      constraintEqualToAnchor:self.view.centerYAnchor
                     constant:[self centerYOffset]];
  [NSLayoutConstraint activateConstraints:@[
    [_animationView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_animationView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_animationView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor],
    _animationViewCenterYConstraint,
  ]];
}

// The offset from center Y to place the divider between the animation and the
// stackView.
- (CGFloat)centerYOffset {
  return IsRegularXRegularSizeClass(self.view.window) ? -kTabletCenterOffset
                                                      : 0;
}

@end
