// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button_style.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_constants.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_progress_bar.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/gfx/ios/uikit_util.h"

@interface PrimaryToolbarView ()

// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// ContentView of the vibrancy effect if there is one, self otherwise.
@property(nonatomic, strong) UIView* contentView;

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

// Separator below the toolbar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* separator;

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
// Button to display the tools menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* toolsMenuButton;

// Button to cancel the edit of the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIButton* cancelButton;

#pragma mark** Location bar. **
// Location bar containing the omnibox.
@property(nonatomic, strong) UIView* locationBarView;
// Container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* locationBarContainer;
// The height of the container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* locationBarContainerHeight;
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

// Constraints for the tabGroupIndicator.
@property(nonatomic, strong, readwrite)
    NSArray<NSLayoutConstraint*>* tabGroupIndicatorTopOmniboxConstraints;
@property(nonatomic, strong, readwrite)
    NSArray<NSLayoutConstraint*>* tabGroupIndicatorBottomOmniboxConstraints;

@end

@implementation PrimaryToolbarView

@synthesize fakeOmniboxTarget = _fakeOmniboxTarget;
@synthesize locationBarBottomConstraint = _locationBarBottomConstraint;
@synthesize locationBarContainerHeight = _locationBarContainerHeight;
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
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize cancelButton = _cancelButton;
@synthesize collapsedToolbarButton = _collapsedToolbarButton;
@synthesize expandedConstraints = _expandedConstraints;
@synthesize contractedConstraints = _contractedConstraints;
@synthesize contractedNoMarginConstraints = _contractedNoMarginConstraints;
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

  [self setUpToolbarBackground];
  [self setUpLeadingStackView];
  [self setUpTrailingStackView];
  [self setUpCancelButton];
  [self setUpLocationBar];
  [self setUpProgressBar];
  [self setUpCollapsedToolbarButton];
  [self setUpSeparator];

  [self setUpConstraints];
}

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];
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

- (void)updateTabGroupIndicatorAvailability {
  CHECK(IsTabGroupIndicatorEnabled());

  BOOL isTopOmnibox = self.locationBarView != nil;
  if (isTopOmnibox) {
    [NSLayoutConstraint
        deactivateConstraints:self.tabGroupIndicatorBottomOmniboxConstraints];
    [NSLayoutConstraint
        activateConstraints:self.tabGroupIndicatorTopOmniboxConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:self.tabGroupIndicatorTopOmniboxConstraints];
    [NSLayoutConstraint
        activateConstraints:self.tabGroupIndicatorBottomOmniboxConstraints];
  }
  self.tabGroupIndicatorView.showSeparator = !isTopOmnibox;

  BOOL canShowTabStrip = IsRegularXRegularSizeClass(self.superview);
  BOOL isAvailable = !IsCompactHeight(self.superview) && !canShowTabStrip;
  self.tabGroupIndicatorView.available = isAvailable;
}

#pragma mark - Properties

- (void)setMatchNTPHeight:(BOOL)matchNTPHeight {
  if (_matchNTPHeight == matchNTPHeight) {
    return;
  }
  _matchNTPHeight = matchNTPHeight;
  [self invalidateIntrinsicContentSize];
  [self.superview setNeedsLayout];
  [self.superview layoutIfNeeded];
}

// Sets tabgroupIndicatorView.
- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view {
  CHECK(IsTabGroupIndicatorEnabled());
  _tabGroupIndicatorView = view;
  _tabGroupIndicatorView.hidden = YES;
  _tabGroupIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  _tabGroupIndicatorView.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  [self addSubview:_tabGroupIndicatorView];

  id<LayoutGuideProvider> safeArea = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [self.tabGroupIndicatorView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor],
    [self.tabGroupIndicatorView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor],
    [self.tabGroupIndicatorView.heightAnchor
        constraintEqualToConstant:kTabGroupIndicatorHeight],
  ]];
  self.tabGroupIndicatorTopOmniboxConstraints = @[
    [self.tabGroupIndicatorView.bottomAnchor
        constraintEqualToAnchor:self.locationBarContainer.topAnchor
                       constant:-kAdaptiveLocationBarVerticalMargin],
  ];
  self.tabGroupIndicatorBottomOmniboxConstraints = @[
    [self.tabGroupIndicatorView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor],
  ];

  [self updateTabGroupIndicatorAvailability];
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  CGFloat height = 0;

  BOOL isTopOmnibox = self.locationBarView != nil;
  if (isTopOmnibox) {
    height += self.matchNTPHeight
                  ? content_suggestions::FakeToolbarHeight()
                  : ToolbarExpandedHeight(
                        self.traitCollection.preferredContentSizeCategory);
  }

  // If the tab group indicator is visible, add its height to the total height.
  if (IsTabGroupIndicatorEnabled() && !_tabGroupIndicatorView.hidden) {
    height += kTabGroupIndicatorHeight;
    // If the Omnibox is not at the top, remove the top vertical margin to avoid
    // extra space when the tab group indicator is present.
    if (!isTopOmnibox) {
      height -= kTopToolbarUnsplitMargin;
    } else {
    }
  }
  // TODO(crbug.com/40279063): Find out why primary toolbar height cannot be
  // zero. This is a temporary fix for the pdf bug.
  return CGSizeMake(UIViewNoIntrinsicMetric, height > 0 ? height : 1);
}

- (void)didMoveToSuperview {
  if (IsTabGroupIndicatorEnabled()) {
    // Ensure the tab group indicator's visibility aligns with the new
    // superview's layout context.
    [self updateTabGroupIndicatorAvailability];
  }
}

#pragma mark - Setup

// Sets up the toolbar background.
- (void)setUpToolbarBackground {
  self.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  self.contentView = self;
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
  [self.locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];
  self.locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;

  // The location bar shouldn't have vibrancy.
  [self addSubview:self.locationBarContainer];
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
  self.tabGridButton = [self.buttonFactory tabGridButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  self.trailingStackViewButtons =
      @[ self.shareButton, self.tabGridButton, self.toolsMenuButton ];

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

// Sets the separator up.
- (void)setUpSeparator {
  self.separator = [[UIView alloc] init];
  self.separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  self.separator.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.separator];
}

// Sets the constraints up.
- (void)setUpConstraints {
  id<LayoutGuideProvider> safeArea = self.safeAreaLayoutGuide;
  self.expandedConstraints = [NSMutableArray array];
  self.contractedConstraints = [NSMutableArray array];
  self.contractedNoMarginConstraints = [NSMutableArray array];

  // Separator constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [self.separator.topAnchor constraintEqualToAnchor:self.bottomAnchor],
    [self.separator.heightAnchor
        constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                      kToolbarSeparatorHeight)],
  ]];

  // Leading StackView constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.leadingStackView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kAdaptiveToolbarMargin],
    [self.leadingStackView.centerYAnchor
        constraintEqualToAnchor:self.locationBarContainer.centerYAnchor],
    [self.leadingStackView.heightAnchor
        constraintEqualToConstant:kAdaptiveToolbarButtonHeight],
  ]];

  // LocationBar constraints. The constant value is set by the VC.
  self.locationBarContainerHeight =
      [self.locationBarContainer.heightAnchor constraintEqualToConstant:0];
  self.locationBarBottomConstraint = [self.locationBarContainer.bottomAnchor
      constraintEqualToAnchor:self.bottomAnchor];

  [NSLayoutConstraint activateConstraints:@[
    self.locationBarBottomConstraint,
    self.locationBarContainerHeight,
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
    [self.trailingStackView.centerYAnchor
        constraintEqualToAnchor:self.locationBarContainer.centerYAnchor],
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

#pragma mark - AdaptiveToolbarView

- (void)setLocationBarView:(UIView*)locationBarView {
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
  if (IsTabGroupIndicatorEnabled()) {
    [self updateTabGroupIndicatorAvailability];
  }

  if (!self.locationBarContainer || !locationBarView)
    return;

  [self.locationBarContainer addSubview:locationBarView];
  AddSameConstraints(self.locationBarView, self.locationBarContainer);
  [self.locationBarContainer.trailingAnchor
      constraintGreaterThanOrEqualToAnchor:self.locationBarView.trailingAnchor]
      .active = YES;
}

- (void)setTabGridButtonStyle:(ToolbarTabGridButtonStyle)tabGridButtonStyle {
  self.tabGridButton.tabGridButtonStyle = tabGridButtonStyle;
}

- (NSArray<ToolbarButton*>*)allButtons {
  if (!_allButtons) {
    _allButtons = [self.leadingStackViewButtons
        arrayByAddingObjectsFromArray:self.trailingStackViewButtons];
  }
  return _allButtons;
}

- (ToolbarButton*)openNewTabButton {
  return nil;
}

@end
