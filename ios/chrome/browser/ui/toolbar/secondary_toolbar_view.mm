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
#import "ios/chrome/browser/ui/toolbar/toolbar_progress_bar.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {
const CGFloat kToolsMenuOffset = -7;

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
// Progress bar displayed below the toolbar, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarProgressBar* progressBar;

// The stack view containing the buttons, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIStackView* buttonStackView;

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

@implementation SecondaryToolbarView {
  // Constraint between `self.topAnchor` and `buttonStackView.topAnchor`. Active
  // when the omnibox is not in the bottom toolbar.
  NSLayoutConstraint* _buttonStackViewNoOmniboxConstraint;
  // Constraint between `locationBarContainer.bottomAnchor` and
  // `buttonStackView.topAnchor`. Active when the omnibox is in the bottom
  // toolbar.
  NSLayoutConstraint* _locationBarBottomConstraint;
}

@synthesize allButtons = _allButtons;
@synthesize backButton = _backButton;
@synthesize buttonFactory = _buttonFactory;
@synthesize buttonStackView = _buttonStackView;
@synthesize collapsedToolbarButton = _collapsedToolbarButton;
@synthesize forwardButton = _forwardButton;
@synthesize locationBarContainer = _locationBarContainer;
@synthesize locationBarContainerHeight = _locationBarContainerHeight;
@synthesize openNewTabButton = _openNewTabButton;
@synthesize progressBar = _progressBar;
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
  [contentView addSubview:self.buttonStackView];

  UILayoutGuide* safeArea = self.safeAreaLayoutGuide;

  if (IsBottomOmniboxSteadyStateEnabled()) {
    self.buttonStackView.backgroundColor = self.backgroundColor;
    self.collapsedToolbarButton = SecondaryToolbarCollapsedToolbarButton();
    self.locationBarContainer =
        SecondaryToolbarLocationBarContainerView(self.buttonFactory);

    // Add locationBarContainer below buttons as it might move under the
    // buttons.
    [contentView insertSubview:self.locationBarContainer
                  belowSubview:self.buttonStackView];

    // Put `collapsedToolbarButton` on top of everything.
    [contentView addSubview:self.collapsedToolbarButton];
    [contentView bringSubviewToFront:self.collapsedToolbarButton];
    AddSameConstraints(self, self.collapsedToolbarButton);

    // Add progress bar on the top edge.
    _progressBar = [[ToolbarProgressBar alloc] init];
    _progressBar.translatesAutoresizingMaskIntoConstraints = NO;
    _progressBar.hidden = YES;
    [_progressBar.heightAnchor constraintEqualToConstant:kProgressBarHeight]
        .active = YES;
    [contentView addSubview:_progressBar];
    AddSameConstraintsToSides(
        self, _progressBar,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);

    // LocationBarView constraints.
    if (self.locationBarView) {
      AddSameConstraints(self.locationBarView, self.locationBarContainer);
    }

    // Height of location bar, constant controlled by view controller.
    self.locationBarContainerHeight =
        [self.locationBarContainer.heightAnchor constraintEqualToConstant:0];
    // Top margin of location bar, constant controlled by view controller.
    self.locationBarTopConstraint = [self.locationBarContainer.topAnchor
        constraintEqualToAnchor:self.topAnchor];
    _locationBarBottomConstraint = [self.buttonStackView.topAnchor
        constraintEqualToAnchor:self.locationBarContainer.bottomAnchor
                       constant:kBottomAdaptiveLocationBarBottomMargin];

    // Constraint used to move the location bar above the keyboard. The view
    // controller will set the constant to the keyboard's size when necessary.
    _locationBarKeyboardConstraint = [self.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:self.locationBarContainer
                                                 .bottomAnchor];

    _buttonStackViewNoOmniboxConstraint = [self.buttonStackView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kBottomButtonsTopMargin];
    [self updateButtonStackViewConstraint];

    [NSLayoutConstraint activateConstraints:@[
      self.locationBarTopConstraint,
      self.locationBarContainerHeight,
      [self.locationBarContainer.leadingAnchor
          constraintEqualToAnchor:safeArea.leadingAnchor
                         constant:kExpandedLocationBarHorizontalMargin],
      [self.locationBarContainer.trailingAnchor
          constraintEqualToAnchor:safeArea.trailingAnchor
                         constant:-kExpandedLocationBarHorizontalMargin],
      [self.buttonStackView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:kBottomButtonsTopMargin],
    ]];

  } else {  // Bottom omnibox flag disabled.
    [self.buttonStackView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kBottomButtonsTopMargin]
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

  [self updateButtonStackViewConstraint];
  if (!self.locationBarContainer || !locationBarView) {
    return;
  }

  [self.locationBarContainer addSubview:locationBarView];
  AddSameConstraints(self.locationBarView, self.locationBarContainer);
}

#pragma mark - Private

/// Updates `buttonStackView.topAnchor` constraints when adding/removing the
/// location bar.
- (void)updateButtonStackViewConstraint {
  // Reset `buttonStackView` top constraints.
  _locationBarBottomConstraint.active = NO;
  _buttonStackViewNoOmniboxConstraint.active = NO;

  // Set the correct constrant for `buttonStackView.topAnchor`.
  if (self.locationBarView) {
    _locationBarBottomConstraint.active = YES;
  } else {
    _buttonStackViewNoOmniboxConstraint.active = YES;
  }
}

@end
