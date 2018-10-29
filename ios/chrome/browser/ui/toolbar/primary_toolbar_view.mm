// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"

#import "base/ios/ios_util.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_progress_bar.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryToolbarView ()
// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// ContentView of the vibrancy effect if there is one, self otherwise.
@property(nonatomic, strong) UIView* contentView;

// The blur visual effect view, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* blur;

// Container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* locationBarContainer;
// The height of the container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite) NSLayoutConstraint* locationBarHeight;
// The layout guide used to give extra padding at the bottom for the location
// bar. This padding is considered as "extra" as it is added to the one defined
// in |locationBarBottomConstraint|.
@property(nonatomic, strong) UILayoutGuide* extraPaddingGuide;

// StackView containing the leading buttons (relative to the location bar). It
// should only contain ToolbarButtons. Redefined as readwrite.
@property(nonatomic, strong, readwrite) UIStackView* leadingStackView;
// Buttons from the leading stack view.
@property(nonatomic, strong) NSArray<ToolbarButton*>* leadingStackViewButtons;
// StackView containing the trailing buttons (relative to the location bar). It
// should only contain ToolbarButtons. Redefined as readwrite.
@property(nonatomic, strong, readwrite) UIStackView* trailingStackView;
// Buttons from the trailing stack view.
@property(nonatomic, strong) NSArray<ToolbarButton*>* trailingStackViewButtons;

// Progress bar displayed below the toolbar, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarProgressBar* progressBar;

#pragma mark** Buttons in the leading stack view. **
// Button to navigate back, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* backButton;
// Button to navigate forward, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* forwardButton;
// Button to display the TabGrid, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarTabGridButton* tabGridButton;
// Button to stop the loading of the page, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* stopButton;
// Button to reload the page, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* reloadButton;

#pragma mark** Buttons in the trailing stack view. **
// Button to display the share menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* shareButton;
// Button to manage the bookmarks of this page, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* bookmarkButton;
// Button to display the tools menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarToolsMenuButton* toolsMenuButton;

// Button to cancel the edit of the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIButton* cancelButton;
// Button taking the full size of the toolbar. Expands the toolbar when  tapped.
// Redefined as readwrite.
@property(nonatomic, strong, readwrite) UIButton* collapsedToolbarButton;

// Constraints for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite)
    NSMutableArray<NSLayoutConstraint*>* expandedConstraints;
@property(nonatomic, strong, readwrite)
    NSMutableArray<NSLayoutConstraint*>* contractedConstraints;
@property(nonatomic, strong, readwrite)
    NSMutableArray<NSLayoutConstraint*>* contractedNoMarginConstraints;

@end

@implementation PrimaryToolbarView

@synthesize locationBarView = _locationBarView;
@synthesize fakeOmniboxTarget = _fakeOmniboxTarget;
@synthesize locationBarBottomConstraint = _locationBarBottomConstraint;
@synthesize locationBarExtraBottomPadding = _locationBarExtraBottomPadding;
@synthesize locationBarHeight = _locationBarHeight;
@synthesize extraPaddingGuide = _extraPaddingGuide;
@synthesize buttonFactory = _buttonFactory;
@synthesize allButtons = _allButtons;
@synthesize progressBar = _progressBar;
@synthesize leadingStackView = _leadingStackView;
@synthesize leadingStackViewButtons = _leadingStackViewButtons;
@synthesize backButton = _backButton;
@synthesize forwardButton = _forwardButton;
@synthesize tabGridButton = _tabGridButton;
@synthesize stopButton = _stopButton;
@synthesize reloadButton = _reloadButton;
@synthesize locationBarContainer = _locationBarContainer;
@synthesize trailingStackView = _trailingStackView;
@synthesize trailingStackViewButtons = _trailingStackViewButtons;
@synthesize shareButton = _shareButton;
@synthesize bookmarkButton = _bookmarkButton;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize cancelButton = _cancelButton;
@synthesize collapsedToolbarButton = _collapsedToolbarButton;
@synthesize expandedConstraints = _expandedConstraints;
@synthesize contractedConstraints = _contractedConstraints;
@synthesize contractedNoMarginConstraints = _contractedNoMarginConstraints;
@synthesize blur = _blur;
@synthesize contentView = _contentView;

#pragma mark - Public

- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)factory {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _buttonFactory = factory;
  }
  return self;
}

- (void)setUp {
  if (self.subviews.count > 0) {
    // Setup the view only once.
    return;
  }
  DCHECK(self.buttonFactory);

  self.translatesAutoresizingMaskIntoConstraints = NO;

  [self setUpBlurredBackground];
  [self setUpLeadingStackView];
  [self setUpTrailingStackView];
  [self setUpCancelButton];
  [self setUpLocationBar];
  [self setUpProgressBar];
  [self setUpCollapsedToolbarButton];

  [self setUpConstraints];
}

- (void)addFakeOmniboxTarget {
  self.fakeOmniboxTarget = [[UIView alloc] init];
  self.fakeOmniboxTarget.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.fakeOmniboxTarget];
  AddSameConstraints(self.locationBarContainer, self.fakeOmniboxTarget);
}

- (void)removeFakeOmniboxTarget {
  [self.fakeOmniboxTarget removeFromSuperview];
  self.fakeOmniboxTarget = nil;
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kAdaptiveToolbarHeight);
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (IsRegularXRegularSizeClass(self)) {
    self.backgroundColor =
        self.buttonFactory.toolbarConfiguration.backgroundColor;
    self.blur.alpha = 0;
  } else {
    self.backgroundColor = [UIColor clearColor];
    self.blur.alpha = 1;
  }
}

#pragma mark - Setup

// Sets the blur effect on the toolbar background.
- (void)setUpBlurredBackground {
  UIBlurEffect* blurEffect = self.buttonFactory.toolbarConfiguration.blurEffect;
  if (blurEffect) {
    self.blur = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  } else {
    self.blur = [[UIView alloc] init];
  }
  self.blur.backgroundColor =
      self.buttonFactory.toolbarConfiguration.blurBackgroundColor;
  [self addSubview:self.blur];

  self.contentView = self;

  if (UIVisualEffect* vibrancy = [self.buttonFactory.toolbarConfiguration
          vibrancyEffectForBlurEffect:blurEffect]) {
    UIVisualEffectView* vibrancyView =
        [[UIVisualEffectView alloc] initWithEffect:vibrancy];
    self.contentView = vibrancyView.contentView;
    [self addSubview:vibrancyView];
    vibrancyView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self, vibrancyView);
  }

  self.blur.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.blur, self);
}

// Sets the cancel button to stop editing the location bar.
- (void)setUpCancelButton {
  self.cancelButton = [self.buttonFactory cancelButton];
  self.cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.cancelButton];
}

// Sets the location bar container and its view if present.
- (void)setUpLocationBar {
  self.locationBarContainer = [[UIView alloc] init];
  self.locationBarContainer.backgroundColor =
      [self.buttonFactory.toolbarConfiguration
          locationBarBackgroundColorWithVisibility:1];
  self.locationBarContainer.layer.cornerRadius =
      kAdaptiveLocationBarCornerRadius;
  [self.locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];
  self.locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;

  // The location bar shouldn't have vibrancy.
  [self addSubview:self.locationBarContainer];

  // Add layout guide to add extra padding for the location bar if needed.
  self.extraPaddingGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:self.extraPaddingGuide];

  if (self.locationBarView) {
    [self.locationBarContainer addSubview:self.locationBarView];
  }
}

// Sets the leading stack view.
- (void)setUpLeadingStackView {
  self.backButton = [self.buttonFactory backButton];
  self.forwardButton = [self.buttonFactory forwardButton];
  self.stopButton = [self.buttonFactory stopButton];
  self.stopButton.hiddenInCurrentState = YES;
  self.reloadButton = [self.buttonFactory reloadButton];

  self.leadingStackViewButtons = @[
    self.backButton, self.forwardButton, self.stopButton, self.reloadButton
  ];
  self.leadingStackView = [[UIStackView alloc]
      initWithArrangedSubviews:self.leadingStackViewButtons];
  self.leadingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.leadingStackView.spacing = kAdaptiveToolbarStackViewSpacing;
  [self.leadingStackView
      setContentHuggingPriority:UILayoutPriorityDefaultHigh
                        forAxis:UILayoutConstraintAxisHorizontal];

  [self.contentView addSubview:self.leadingStackView];
}

// Sets the trailing stack view.
- (void)setUpTrailingStackView {
  self.shareButton = [self.buttonFactory shareButton];
  self.bookmarkButton = [self.buttonFactory bookmarkButton];
  self.tabGridButton = [self.buttonFactory tabGridButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  self.trailingStackViewButtons = @[
    self.bookmarkButton, self.shareButton, self.tabGridButton,
    self.toolsMenuButton
  ];
  self.trailingStackView = [[UIStackView alloc]
      initWithArrangedSubviews:self.trailingStackViewButtons];
  self.trailingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.trailingStackView.spacing = kAdaptiveToolbarStackViewSpacing;
  [self.trailingStackView
      setContentHuggingPriority:UILayoutPriorityDefaultHigh
                        forAxis:UILayoutConstraintAxisHorizontal];

  [self.contentView addSubview:self.trailingStackView];
}

// Sets the progress bar up.
- (void)setUpProgressBar {
  self.progressBar = [[ToolbarProgressBar alloc] init];
  self.progressBar.translatesAutoresizingMaskIntoConstraints = NO;
  self.progressBar.hidden = YES;
  [self addSubview:self.progressBar];
}

// Sets the collapsedToolbarButton up.
- (void)setUpCollapsedToolbarButton {
  self.collapsedToolbarButton = [[UIButton alloc] init];
  self.collapsedToolbarButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.collapsedToolbarButton.hidden = YES;
  [self addSubview:self.collapsedToolbarButton];
}

// Sets the constraints up.
- (void)setUpConstraints {
  id<LayoutGuideProvider> safeArea = SafeAreaLayoutGuideForView(self);
  self.expandedConstraints = [NSMutableArray array];
  self.contractedConstraints = [NSMutableArray array];
  self.contractedNoMarginConstraints = [NSMutableArray array];

  // Leading StackView constraints
  [NSLayoutConstraint activateConstraints:@[
    [self.leadingStackView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kAdaptiveToolbarMargin],
    [self.leadingStackView.bottomAnchor
        constraintEqualToAnchor:safeArea.bottomAnchor
                       constant:-kTopButtonsBottomMargin],
    [self.leadingStackView.heightAnchor
        constraintEqualToConstant:kAdaptiveToolbarButtonHeight],
  ]];

  // When switching between incognito and non-incognito BVCs, it is possible for
  // all of the toolbar's buttons to be temporarily hidden, which results in the
  // stack view having zero width.  This seems to permanently break autolayout
  // on iOS 10.  Adding an optional width constraint seems to work around this
  // issue.  See https://crbug.com/851954.
  if (!base::ios::IsRunningOnIOS11OrLater()) {
    NSLayoutConstraint* minWidthConstraint =
        [self.leadingStackView.widthAnchor constraintEqualToConstant:1.0];
    minWidthConstraint.priority = UILayoutPriorityDefaultLow;
    minWidthConstraint.active = YES;
  }

  // LocationBar constraints.
  self.locationBarHeight = [self.locationBarContainer.heightAnchor
      constraintEqualToConstant:kAdaptiveToolbarHeight -
                                2 * kAdaptiveLocationBarVerticalMargin];
  self.locationBarBottomConstraint = [self.locationBarContainer.bottomAnchor
      constraintEqualToAnchor:self.extraPaddingGuide.topAnchor
                     constant:-kAdaptiveLocationBarVerticalMargin];
  self.locationBarExtraBottomPadding =
      [self.extraPaddingGuide.heightAnchor constraintEqualToConstant:0];

  [NSLayoutConstraint activateConstraints:@[
    self.locationBarBottomConstraint,
    self.locationBarHeight,
    self.locationBarExtraBottomPadding,
    [self.extraPaddingGuide.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor],
  ]];
  [self.contractedConstraints addObjectsFromArray:@[
    [self.locationBarContainer.trailingAnchor
        constraintEqualToAnchor:self.trailingStackView.leadingAnchor
                       constant:-kContractedLocationBarHorizontalMargin],
    [self.locationBarContainer.leadingAnchor
        constraintEqualToAnchor:self.leadingStackView.trailingAnchor
                       constant:kContractedLocationBarHorizontalMargin],
  ]];

  // Constraints for contractedNoMarginConstraints.
  [self.contractedNoMarginConstraints addObjectsFromArray:@[
    [self.locationBarContainer.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kExpandedLocationBarHorizontalMargin],
    [self.locationBarContainer.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kExpandedLocationBarHorizontalMargin]
  ]];

  [self.expandedConstraints addObjectsFromArray:@[
    [self.locationBarContainer.trailingAnchor
        constraintEqualToAnchor:self.cancelButton.leadingAnchor],
    [self.locationBarContainer.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kExpandedLocationBarHorizontalMargin]
  ]];

  // Trailing StackView constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.trailingStackView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kAdaptiveToolbarMargin],
    [self.trailingStackView.bottomAnchor
        constraintEqualToAnchor:safeArea.bottomAnchor
                       constant:-kTopButtonsBottomMargin],
    [self.trailingStackView.heightAnchor
        constraintEqualToConstant:kAdaptiveToolbarButtonHeight],
  ]];

  // locationBarView constraints, if present.
  if (self.locationBarView) {
    AddSameConstraints(self.locationBarView, self.locationBarContainer);
  }

  // Cancel button constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.cancelButton.topAnchor
        constraintEqualToAnchor:self.trailingStackView.topAnchor],
    [self.cancelButton.bottomAnchor
        constraintEqualToAnchor:self.trailingStackView.bottomAnchor],
  ]];
  NSLayoutConstraint* visibleCancel = [self.cancelButton.trailingAnchor
      constraintEqualToAnchor:safeArea.trailingAnchor
                     constant:-kExpandedLocationBarHorizontalMargin];
  NSLayoutConstraint* hiddenCancel = [self.cancelButton.leadingAnchor
      constraintEqualToAnchor:self.trailingAnchor];
  [self.expandedConstraints addObject:visibleCancel];
  [self.contractedConstraints addObject:hiddenCancel];
  [self.contractedNoMarginConstraints addObject:hiddenCancel];

  // ProgressBar constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.progressBar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.progressBar.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [self.progressBar.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [self.progressBar.heightAnchor
        constraintEqualToConstant:kProgressBarHeight],
  ]];

  // CollapsedToolbarButton constraints.
  AddSameConstraints(self, self.collapsedToolbarButton);
}

#pragma mark - Property accessors

- (void)setLocationBarView:(UIView*)locationBarView {
  if (_locationBarView == locationBarView) {
    return;
  }
  [_locationBarView removeFromSuperview];

  _locationBarView = locationBarView;
  locationBarView.translatesAutoresizingMaskIntoConstraints = NO;
  [locationBarView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                     forAxis:UILayoutConstraintAxisHorizontal];

  if (!self.locationBarContainer || !locationBarView)
    return;

  [self.locationBarContainer addSubview:locationBarView];
  AddSameConstraints(self.locationBarView, self.locationBarContainer);
  [self.locationBarContainer.trailingAnchor
      constraintGreaterThanOrEqualToAnchor:self.locationBarView.trailingAnchor]
      .active = YES;
}

- (NSArray<ToolbarButton*>*)allButtons {
  if (!_allButtons) {
    _allButtons = [self.leadingStackViewButtons
        arrayByAddingObjectsFromArray:self.trailingStackViewButtons];
  }
  return _allButtons;
}

#pragma mark - AdaptiveToolbarView

- (ToolbarButton*)omniboxButton {
  return nil;
}

@end
