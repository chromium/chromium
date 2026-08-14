// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_navigation_buttons_container.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation ToolbarNavigationButtonsContainer {
  UIView* _backgroundView;
  BOOL _incognito;
}

- (instancetype)initWithBackButton:(ToolbarButton*)backButton
                     forwardButton:(ToolbarButton*)forwardButton
                         incognito:(BOOL)incognito {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    CHECK(backButton);
    CHECK(forwardButton);
    _incognito = incognito;

    self.translatesAutoresizingMaskIntoConstraints = NO;
    [self setContentCompressionResistancePriority:UILayoutPriorityRequired
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
    [self setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisHorizontal];

    UIView* backgroundView = [[UIView alloc] init];
    _backgroundView = backgroundView;
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    backgroundView.backgroundColor = ToolbarElementBackgroundColor(incognito);
    [self addSubview:backgroundView];
    AddSameConstraints(backgroundView, self);

    // Internal stack view to handle dynamic resizing when the forward button
    // visibility changes.
    UIStackView* buttonsStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ backButton, forwardButton ]];
    buttonsStack.translatesAutoresizingMaskIntoConstraints = NO;
    buttonsStack.axis = UILayoutConstraintAxisHorizontal;
    buttonsStack.distribution = UIStackViewDistributionFill;
    buttonsStack.alignment = UIStackViewAlignmentFill;

    [backgroundView addSubview:buttonsStack];
    AddSameConstraints(buttonsStack, backgroundView);

    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor constraintEqualToAnchor:backButton.heightAnchor]
    ]];

    [self updateCornerRadius];
    backgroundView.clipsToBounds = YES;
    ConfigureShadowForToolbarElement(self);

    // Remove effects from the standalone buttons in the container
    backButton.shadowAndBackgroundRemoved = YES;
    forwardButton.shadowAndBackgroundRemoved = YES;

    __weak __typeof(self) weakSelf = self;
    [self
        registerForTraitChanges:@[
          UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class
        ]
                    withHandler:^(id<UITraitEnvironment> environment,
                                  UITraitCollection* previousTraitCollection) {
                      [weakSelf updateCornerRadius];
                    }];
    [self
        registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                    withHandler:^(id<UITraitEnvironment> environment,
                                  UITraitCollection* previousTraitCollection) {
                      ConfigureShadowForToolbarElement(weakSelf);
                    }];
  }
  return self;
}

#pragma mark - ToolbarElementWithBackground

- (void)setBackgroundAlpha:(CGFloat)backgroundAlpha {
  if (!IsGlassToolbarEnabled()) {
    return;
  }
  _backgroundView.backgroundColor =
      ToolbarElementBackgroundColor(_incognito, backgroundAlpha);
}

#pragma mark - Private

// Updates the corner radius of the background view based on current traits.
- (void)updateCornerRadius {
  ConfigureCornerRadiusForToolbarButtonContainer(_backgroundView,
                                                 self.traitCollection);
}

@end
