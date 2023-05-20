// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kToolsMenuOffset = -7;

// Vertical stack view for `SecondaryToolbarView` containing the
// `locationBarContainer` and `buttonStackView`.
UIStackView* SecondaryToolbarVerticalStackView() {
  UIStackView* verticalStackView = [[UIStackView alloc] init];
  verticalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  verticalStackView.axis = UILayoutConstraintAxisVertical;
  verticalStackView.spacing = kTopButtonsBottomMargin;
  verticalStackView.distribution = UIStackViewDistributionFill;
  verticalStackView.alignment = UIStackViewAlignmentCenter;
  return verticalStackView;
}

// Button shown when the view is collapsed to exit fullscreen.
UIButton* SecondaryToolbarCollapsedToolbarButton() {
  UIButton* collapsedToolbarButton = [[UIButton alloc] init];
  collapsedToolbarButton.translatesAutoresizingMaskIntoConstraints = NO;
  collapsedToolbarButton.hidden = YES;
  return collapsedToolbarButton;
}

// Container for the location bar view.
UIView* SecondaryToolbarLocationBarContainerView(
    ToolbarButtonFactory* buttonFactory) {
  UIView* locationBarContainer = [[UIView alloc] init];
  locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;
  locationBarContainer.backgroundColor = [buttonFactory.toolbarConfiguration
      locationBarBackgroundColorWithVisibility:1];
  [locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];
  return locationBarContainer;
}

}  // namespace

@interface SecondaryToolbarView ()
// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// Redefined as readwrite
@property(nonatomic, strong, readwrite) NSArray<ToolbarButton*>* allButtons;

// Separator above the toolbar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* separator;

// The stack view containing the buttons, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIStackView* buttonStackView;
// The stack view containing `locationBarContainer` and `buttonStackView`.
@property(nonatomic, strong) UIStackView* verticalStackView;

// Button to navigate back, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* backButton;
// Buttons to navigate forward, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* forwardButton;
// Button to display the tools menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* toolsMenuButton;
// Button to display the tab grid, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarTabGridButton* tabGridButton;
// Button to create a new tab, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* openNewTabButton;

#pragma mark** Location bar. **
// Location bar containing the omnibox.
@property(nonatomic, strong) UIView* locationBarView;
// Container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* locationBarContainer;
// The height of the container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* locationBarContainerHeight;
// Button taking the full size of the toolbar. Expands the toolbar when tapped,
// redefined as readwrite.
@property(nonatomic, strong, readwrite) UIButton* collapsedToolbarButton;

@end

@implementation SecondaryToolbarView

@synthesize allButtons = _allButtons;
@synthesize backButton = _backButton;
@synthesize buttonFactory = _buttonFactory;
@synthesize buttonStackView = _buttonStackView;
@synthesize collapsedToolbarButton = _collapsedToolbarButton;
@synthesize forwardButton = _forwardButton;
@synthesize locationBarContainer = _locationBarContainer;
@synthesize locationBarContainerHeight = _locationBarContainerHeight;
@synthesize openNewTabButton = _openNewTabButton;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize tabGridButton = _tabGridButton;

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
  return CGSizeMake(UIViewNoIntrinsicMetric, kSecondaryToolbarHeight);
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

  self.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;

  UIView* contentView = self;

  // Toolbar buttons.
  self.backButton = [self.buttonFactory backButton];
  self.forwardButton = [self.buttonFactory forwardButton];
  self.openNewTabButton = [self.buttonFactory openNewTabButton];
  self.tabGridButton = [self.buttonFactory tabGridButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  // Move the tools menu button such as it looks visually balanced with the
  // button on the other side of the toolbar.
  NSInteger textDirection = base::i18n::IsRTL() ? -1 : 1;
  self.toolsMenuButton.transform =
      CGAffineTransformMakeTranslation(textDirection * kToolsMenuOffset, 0);

  self.allButtons = @[
    self.backButton, self.forwardButton, self.openNewTabButton,
    self.tabGridButton, self.toolsMenuButton
  ];

  // Separator.
  self.separator = [[UIView alloc] init];
  self.separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  self.separator.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.separator];

  // Button StackView.
  self.buttonStackView =
      [[UIStackView alloc] initWithArrangedSubviews:self.allButtons];
  self.buttonStackView.distribution = UIStackViewDistributionEqualSpacing;
  self.buttonStackView.translatesAutoresizingMaskIntoConstraints = NO;

  UILayoutGuide* safeArea = self.safeAreaLayoutGuide;

  if (IsBottomOmniboxSteadyStateEnabled()) {
    self.verticalStackView = SecondaryToolbarVerticalStackView();
    self.collapsedToolbarButton = SecondaryToolbarCollapsedToolbarButton();
    self.locationBarContainer =
        SecondaryToolbarLocationBarContainerView(self.buttonFactory);

    [contentView addSubview:self.verticalStackView];
    [contentView addSubview:self.collapsedToolbarButton];
    [self.verticalStackView addArrangedSubview:self.locationBarContainer];
    [self.verticalStackView addArrangedSubview:self.buttonStackView];

    // VerticalStackView constraints.
    AddSameConstraintsToSides(
        self.verticalStackView, self,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);

    // CollapsedToolbarButton constraints.
    AddSameConstraints(self, self.collapsedToolbarButton);

    // LocationBarView constraints.
    if (self.locationBarView) {
      AddSameConstraints(self.locationBarView, self.locationBarContainer);
    }

    // LocationBarContainer constraints. The constant value is set by the VC.
    self.locationBarContainerHeight =
        [self.locationBarContainer.heightAnchor constraintEqualToConstant:0];
    self.locationBarTopConstraint = [self.locationBarContainer.topAnchor
        constraintEqualToAnchor:self.topAnchor];

    [NSLayoutConstraint activateConstraints:@[
      self.locationBarTopConstraint,
      self.locationBarContainerHeight,
      [self.locationBarContainer.leadingAnchor
          constraintEqualToAnchor:safeArea.leadingAnchor
                         constant:kExpandedLocationBarHorizontalMargin],
      [self.locationBarContainer.trailingAnchor
          constraintEqualToAnchor:safeArea.trailingAnchor
                         constant:-kExpandedLocationBarHorizontalMargin],
    ]];

  } else {  // Bottom omnibox flag disabled.
    [contentView addSubview:self.buttonStackView];
    [self.buttonStackView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kBottomButtonsBottomMargin]
        .active = YES;
  }

  [NSLayoutConstraint activateConstraints:@[
    [self.buttonStackView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kAdaptiveToolbarMargin],
    [self.buttonStackView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kAdaptiveToolbarMargin],

    [self.separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [self.separator.bottomAnchor constraintEqualToAnchor:self.topAnchor],
    [self.separator.heightAnchor
        constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                      kToolbarSeparatorHeight)],
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

- (MDCProgressView*)progressBar {
  return nil;
}

- (void)setLocationBarView:(UIView*)locationBarView {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  if (_locationBarView == locationBarView) {
    return;
  }

  if ([_locationBarView superview] == self.locationBarContainer) {
    [_locationBarView removeFromSuperview];
  }
  _locationBarView = locationBarView;

  locationBarView.translatesAutoresizingMaskIntoConstraints = NO;
  [locationBarView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                     forAxis:UILayoutConstraintAxisHorizontal];

  if (!self.locationBarContainer || !locationBarView) {
    return;
  }

  [self.locationBarContainer addSubview:locationBarView];
  AddSameConstraints(self.locationBarView, self.locationBarContainer);
}

#pragma mark - ToolbarCollapsing

- (CGFloat)expandedToolbarHeight {
  return self.intrinsicContentSize.height;
}

- (CGFloat)collapsedToolbarHeight {
  return 0.0;
}

@end
