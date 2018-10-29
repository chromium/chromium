// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_collapsing.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SecondaryToolbarView ()<ToolbarCollapsing>
// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// Redefined as readwrite
@property(nonatomic, strong, readwrite) NSArray<ToolbarButton*>* allButtons;

// The blur visual effect view, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* blur;

// The stack view containing the buttons.
@property(nonatomic, strong) UIStackView* stackView;

// Button to navigate back, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* backButton;
// Buttons to navigate forward, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* forwardButton;
// Button to display the tools menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarToolsMenuButton* toolsMenuButton;
// Button to display the tab grid, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarTabGridButton* tabGridButton;
// Button to focus the omnibox, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* omniboxButton;

@end

@implementation SecondaryToolbarView

@synthesize allButtons = _allButtons;
@synthesize buttonFactory = _buttonFactory;
@synthesize stackView = _stackView;
@synthesize backButton = _backButton;
@synthesize forwardButton = _forwardButton;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize omniboxButton = _omniboxButton;
@synthesize tabGridButton = _tabGridButton;
@synthesize blur = _blur;

#pragma mark - Public

- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)factory {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _buttonFactory = factory;
    [self setUp];
  }
  return self;
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kAdaptiveToolbarHeight);
}

- (void)willMoveToWindow:(UIWindow*)newWindow {
  [super willMoveToWindow:newWindow];
  [NamedGuide guideWithName:kSecondaryToolbarGuide view:self].constrainedView =
      nil;
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [NamedGuide guideWithName:kSecondaryToolbarGuide view:self].constrainedView =
      self;
}

#pragma mark - Setup

// Sets all the subviews and constraints of the view.
- (void)setUp {
  if (self.subviews.count > 0) {
    // Make sure the view is instantiated only once.
    return;
  }
  DCHECK(self.buttonFactory);

  self.translatesAutoresizingMaskIntoConstraints = NO;

  UIBlurEffect* blurEffect = self.buttonFactory.toolbarConfiguration.blurEffect;
  if (blurEffect) {
    self.blur = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  } else {
    self.blur = [[UIView alloc] init];
  }
  self.blur.backgroundColor =
      self.buttonFactory.toolbarConfiguration.blurBackgroundColor;
  [self addSubview:self.blur];
  self.blur.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.blur, self);

  UIView* contentView = self;
  UIVisualEffect* vibrancy = [self.buttonFactory.toolbarConfiguration
      vibrancyEffectForBlurEffect:blurEffect];
  if (vibrancy && IconForSearchButton() != ToolbarSearchButtonIconColorful) {
    // Add vibrancy only if we have a vibrancy effect.
    UIVisualEffectView* vibrancyView =
        [[UIVisualEffectView alloc] initWithEffect:vibrancy];
    [self addSubview:vibrancyView];
    vibrancyView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self, vibrancyView);
    contentView = vibrancyView.contentView;
  }

  self.backButton = [self.buttonFactory backButton];
  self.forwardButton = [self.buttonFactory forwardButton];
  self.omniboxButton = [self.buttonFactory omniboxButton];
  self.tabGridButton = [self.buttonFactory tabGridButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  self.allButtons = @[
    self.backButton, self.forwardButton, self.omniboxButton, self.tabGridButton,
    self.toolsMenuButton
  ];

  self.stackView =
      [[UIStackView alloc] initWithArrangedSubviews:self.allButtons];
  self.stackView.distribution = UIStackViewDistributionEqualSpacing;
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:self.stackView];

  id<LayoutGuideProvider> safeArea = SafeAreaLayoutGuideForView(self);

  [NSLayoutConstraint activateConstraints:@[
    [self.stackView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kAdaptiveToolbarMargin],
    [self.stackView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kAdaptiveToolbarMargin],
    [self.stackView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kBottomButtonsBottomMargin],
  ]];
}

#pragma mark - AdaptiveToolbarView

- (ToolbarButton*)stopButton {
  return nil;
}

- (ToolbarButton*)reloadButton {
  return nil;
}

- (ToolbarButton*)shareButton {
  return nil;
}

- (ToolbarButton*)bookmarkButton {
  return nil;
}

- (MDCProgressView*)progressBar {
  return nil;
}

#pragma mark - ToolbarCollapsing

- (CGFloat)expandedToolbarHeight {
  return self.intrinsicContentSize.height;
}

- (CGFloat)collapsedToolbarHeight {
  return 0.0;
}

@end
