// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Spacing between buttons in the stack.
const CGFloat kButtonSpacing = 8.0;

// Default bottom margin for the button stack.
const CGFloat kButtonStackBottomMargin = 20.0;
// Legacy bottom margin for the button stack.
const CGFloat kLegacyButtonStackBottomMargin = 0.0;

// Inset for the content view at the bottom, used when
// `_addsContentViewBottomInset` is YES.
const CGFloat kContentViewBottomInset = 20;

// Height of the gradient view above the action buttons.
const CGFloat kGradientHeight = 30;

// Max multiplier for the detent height.
const CGFloat kMaxDetentMultiplier = 0.75;

// Min multiplier for the detent height.
const CGFloat kMinDetentMultiplier = 0.25;

// The position of a button in the stack.
typedef NS_ENUM(NSInteger, ButtonStackButtonPosition) {
  ButtonStackButtonPositionPrimary,
  ButtonStackButtonPositionSecondary,
  ButtonStackButtonPositionTertiary,
};

}  // namespace

@interface ButtonStackViewController () <UIScrollViewDelegate>

// The bottom margin for the action button stack.
@property(nonatomic, assign) CGFloat actionStackBottomMargin;

// Redefine properties as readwrite for internal use.
@property(nonatomic, strong, readwrite) UIView* contentView;
@property(nonatomic, strong, readwrite) UILayoutGuide* widthLayoutGuide;
@property(nonatomic, strong, readwrite) ButtonStackConfiguration* configuration;
@property(nonatomic, strong, readwrite) ChromeButton* primaryActionButton;
@property(nonatomic, strong, readwrite) ChromeButton* secondaryActionButton;
@property(nonatomic, strong, readwrite) ChromeButton* tertiaryActionButton;

@end

@implementation ButtonStackViewController {
  NSLayoutConstraint* _scrollContainerBottomToSafeAreaBottomConstraint;
  // Stack view for the action buttons.
  UIStackView* _actionStackView;
  // The bottom constraint for the action stack view against the safe area.
  NSLayoutConstraint* _actionStackSafeAreaBottomConstraint;
  // A secondary bottom constraint for the action stack view against the view.
  NSLayoutConstraint* _actionStackBottomConstraint;
  // The container for the scroll view.
  UIView* _scrollContainerView;
  // The scroll view containing the content.
  UIScrollView* _scrollView;
  // The gradient mask for the scroll view.
  CAGradientLayer* _gradientMask;
  // Whether the gradient view is shown.
  BOOL _showsGradientView;
  // The width layout guide for the content.
  UILayoutGuide* _widthLayoutGuide;
  // The constraint for the content view height.
  NSLayoutConstraint* _contentViewHeightConstraint;
}

- (instancetype)init {
  return [self initWithConfiguration:[[ButtonStackConfiguration alloc] init]];
}

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _configuration = configuration;
    _scrollEnabled = YES;
    _showsVerticalScrollIndicator = YES;
    _addsContentViewBottomInset = YES;
    _showsGradientView = YES;
    _actionStackBottomMargin = kButtonStackBottomMargin;
    _contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentAlways;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  _scrollContainerView = [self createScrollContainerView];
  [self.view addSubview:_scrollContainerView];

  _scrollView = [self createScrollView];
  _scrollView.contentInsetAdjustmentBehavior = _contentInsetAdjustmentBehavior;
  [_scrollContainerView addSubview:_scrollView];

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_contentView];

  [self createButtons];
  [self setupConstraints];
  [self reconfigureButtons];
  [self updateButtonState];
}

- (void)viewIsAppearing:(BOOL)animated {
  [super viewIsAppearing:animated];
  if (self.navigationController.navigationBar) {
    // On iOS 26, the navigation bar is positioned differently in viewDidLoad
    // and after. Make sure that the right position is taken into account.
    [self.sheetPresentationController invalidateDetents];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_scrollView flashScrollIndicators];
  [self updateGradientVisibility];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // Update the content view height constraint to account for the adjusted
  // content inset (e.g. navigation bar).
  _contentViewHeightConstraint.constant =
      -(_scrollView.adjustedContentInset.top +
        _scrollView.adjustedContentInset.bottom);

  [self updateGradientVisibility];
}

#pragma mark - Public

- (BOOL)isScrolledToBottom {
  CGFloat scrollPosition =
      _scrollView.contentOffset.y + _scrollView.frame.size.height;
  CGFloat scrollLimit =
      _scrollView.contentSize.height + _scrollView.adjustedContentInset.bottom;
  return scrollPosition >= scrollLimit;
}

- (void)scrollToBottom {
  CGFloat scrollLimit = _scrollView.contentSize.height -
                        _scrollView.bounds.size.height +
                        _scrollView.adjustedContentInset.bottom;
  [_scrollView setContentOffset:CGPointMake(0, scrollLimit) animated:YES];
}

- (BOOL)hasVisibleButtons {
  if (self.configuration.hideButtons) {
    return NO;
  }
  return !(_primaryActionButton.hidden && _secondaryActionButton.hidden &&
           _tertiaryActionButton.hidden);
}

- (UISheetPresentationControllerDetent*)preferredHeightDetent {
  __typeof(self) __weak weakSelf = self;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf detentForPreferredHeightInContext:context];
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:@"preferred_height"
                        resolver:resolver];
}

- (CGFloat)preferredHeightForContent {
  // Calculate the available width for the content from the layout guide.
  // This is more reliable than self.stackView.bounds.size.width before a layout
  // pass.
  CGFloat availableWidth = _widthLayoutGuide.layoutFrame.size.width;
  CGSize fittingSize =
      CGSizeMake(availableWidth, UILayoutFittingCompressedSize.height);

  // Calculate the height of the content view constrained by the available
  // width.
  CGFloat height =
      [self.contentView
            systemLayoutSizeFittingSize:fittingSize
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;
  height += _scrollView.adjustedContentInset.bottom;

  // Add the height of the button stack.
  height += [self buttonStackHeight];

  // Add the scroll view's top padding, which includes the navigation bar unless
  // automatic content inset adjustments are disabled.
  if (self.navigationController) {
    height += _scrollView.adjustedContentInset.top;
  }

  return height;
}

#pragma mark - ButtonStackConsumer

- (void)setLoading:(BOOL)loading {
  if (self.configuration.isLoading == loading) {
    return;
  }
  self.configuration.loading = loading;
  if (self.configuration.isLoading) {
    // isLoading and isConfirmed are mutually exclusive.
    self.configuration.confirmed = NO;
  }
  [self updateButtonState];
}

- (void)setConfirmed:(BOOL)confirmed {
  if (self.configuration.isConfirmed == confirmed) {
    return;
  }
  self.configuration.confirmed = confirmed;
  if (self.configuration.isConfirmed) {
    // isLoading and isConfirmed are mutually exclusive.
    self.configuration.loading = NO;
  }
  [self updateButtonState];
}

- (void)updateConfiguration:(ButtonStackConfiguration*)configuration {
  self.configuration = configuration;
  [self reloadConfiguration];
}

- (void)reloadConfiguration {
  if (!self.isViewLoaded) {
    return;
  }
  [self reconfigureButtons];
  [self updateButtonState];
}

#pragma mark - Setters

- (void)setScrollEnabled:(BOOL)scrollEnabled {
  _scrollEnabled = scrollEnabled;
  _scrollView.scrollEnabled = scrollEnabled;
}

- (void)setShowsVerticalScrollIndicator:(BOOL)showsVerticalScrollIndicator {
  _showsVerticalScrollIndicator = showsVerticalScrollIndicator;
  _scrollView.showsVerticalScrollIndicator = showsVerticalScrollIndicator;
}

- (void)setShowsGradientView:(BOOL)showsGradientView {
  _showsGradientView = showsGradientView;
  [self updateGradientVisibility];
}

- (void)setContentInsetAdjustmentBehavior:
    (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior {
  _contentInsetAdjustmentBehavior = contentInsetAdjustmentBehavior;
  if (self.isViewLoaded) {
    _scrollView.contentInsetAdjustmentBehavior = contentInsetAdjustmentBehavior;
  }
}

- (void)setActionStackBottomMargin:(CGFloat)actionStackBottomMargin {
  _actionStackBottomMargin = actionStackBottomMargin;
  [self reconfigureBottomConstraint];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateGradientVisibility];
}

#pragma mark - Private

// Returns the height of the button stack view.
- (CGFloat)buttonStackHeight {
  // Calculate the size the stack view needs without forcing a full layout pass.
  CGFloat stackHeight =
      [_actionStackView
          systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
  return stackHeight + self.actionStackBottomMargin;
}

// Returns the bottom inset for the content view.
- (CGFloat)contentViewBottomInset {
  return _addsContentViewBottomInset ? kContentViewBottomInset : 0;
}

// Creates the scroll container view.
- (UIView*)createScrollContainerView {
  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;

  // The container view is given a low priority, allowing it to stretch
  // vertically. The action stack view is given a required priority, forcing it
  // to maintain its intrinsic height.
  [containerView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                   forAxis:UILayoutConstraintAxisVertical];
  [containerView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  return containerView;
}

// Creates the scroll view.
- (UIScrollView*)createScrollView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.scrollEnabled = self.scrollEnabled;
  scrollView.showsVerticalScrollIndicator = self.showsVerticalScrollIndicator;
  scrollView.delegate = self;
  scrollView.contentInset =
      UIEdgeInsetsMake(0, 0, [self contentViewBottomInset], 0);
  return scrollView;
}

// Creates the button stack and the buttons.
- (void)createButtons {
  _actionStackView = [[UIStackView alloc] init];
  _actionStackView.axis = UILayoutConstraintAxisVertical;
  _actionStackView.spacing = kButtonSpacing;
  _actionStackView.distribution = UIStackViewDistributionFillProportionally;
  _actionStackView.translatesAutoresizingMaskIntoConstraints = NO;

  // The action stack view is given a required priority, forcing it to maintain
  // its intrinsic height and preventing it from stretching.
  [_actionStackView setContentHuggingPriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [_actionStackView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  // Create buttons for all possible actions. The `reconfigureButtons` method
  // will handle showing/hiding them based on the current configuration.
  self.tertiaryActionButton =
      [self createButtonForStyle:_configuration.tertiaryButtonStyle];
  self.tertiaryActionButton.accessibilityIdentifier =
      kButtonStackTertiaryActionAccessibilityIdentifier;
  [self.tertiaryActionButton addTarget:self
                                action:@selector(handleTertiaryAction)
                      forControlEvents:UIControlEventTouchUpInside];

  self.primaryActionButton =
      [self createButtonForStyle:_configuration.primaryButtonStyle];
  self.primaryActionButton.accessibilityIdentifier =
      kButtonStackPrimaryActionAccessibilityIdentifier;
  [self.primaryActionButton addTarget:self
                               action:@selector(handlePrimaryAction)
                     forControlEvents:UIControlEventTouchUpInside];

  self.secondaryActionButton =
      [self createButtonForStyle:_configuration.secondaryButtonStyle];
  self.secondaryActionButton.accessibilityIdentifier =
      kButtonStackSecondaryActionAccessibilityIdentifier;
  [self.secondaryActionButton addTarget:self
                                 action:@selector(handleSecondaryAction)
                       forControlEvents:UIControlEventTouchUpInside];

  // The order of the primary and secondary buttons can be swapped based on this
  // feature flag.
  if (app_group::IsConfirmationButtonSwapOrderEnabled()) {
    [_actionStackView addArrangedSubview:self.tertiaryActionButton];
    [_actionStackView addArrangedSubview:self.secondaryActionButton];
    [_actionStackView addArrangedSubview:self.primaryActionButton];
  } else {
    [_actionStackView addArrangedSubview:self.tertiaryActionButton];
    [_actionStackView addArrangedSubview:self.primaryActionButton];
    [_actionStackView addArrangedSubview:self.secondaryActionButton];
  }

  [self.view addSubview:_actionStackView];
}

// Updates the buttons' visibility, titles, and images based on the current
// configuration.
- (void)reconfigureButtons {
  _actionStackView.hidden = _configuration.hideButtons;
  [self configureButtonForPosition:ButtonStackButtonPositionPrimary
                        withString:_configuration.primaryActionString
                             style:_configuration.primaryButtonStyle];
  [self configureButtonForPosition:ButtonStackButtonPositionSecondary
                        withString:_configuration.secondaryActionString
                             style:_configuration.secondaryButtonStyle];
  [self configureButtonForPosition:ButtonStackButtonPositionTertiary
                        withString:_configuration.tertiaryActionString
                             style:_configuration.tertiaryButtonStyle];

  BOOL useLegacyBottomMargin = NO;
  if (@available(iOS 26, *)) {
  } else {
    if (!app_group::IsConfirmationButtonSwapOrderEnabled()) {
      if (!_secondaryActionButton.hidden) {
        useLegacyBottomMargin = YES;
      }
    }
  }
  self.actionStackBottomMargin = useLegacyBottomMargin
                                     ? kLegacyButtonStackBottomMargin
                                     : kButtonStackBottomMargin;

  [self updateScrollContainerBottomConstraintPriority];
  [self reconfigureBottomConstraint];
}

// Sets the constant to the two constraints at the bottom of the button stack.
- (void)reconfigureBottomConstraint {
  CGFloat contraintConstant = -self.actionStackBottomMargin;
  if (![self hasVisibleButtons]) {
    contraintConstant = 0;
  }
  _actionStackSafeAreaBottomConstraint.constant = contraintConstant;
  _actionStackBottomConstraint.constant = contraintConstant;
}

// empty actions state is removed.
// Dynamically updates the priority of the scroll container's bottom constraint.
- (void)updateScrollContainerBottomConstraintPriority {
  if ([self hasVisibleButtons]) {
    _scrollContainerBottomToSafeAreaBottomConstraint.priority =
        UILayoutPriorityDefaultLow - 1;
  } else {
    _scrollContainerBottomToSafeAreaBottomConstraint.priority =
        UILayoutPriorityDefaultHigh;
  }
}

// Configures a button with the given properties.
- (void)configureButtonForPosition:(ButtonStackButtonPosition)position
                        withString:(NSString*)string
                             style:(ChromeButtonStyle)style {
  ChromeButton* button;
  UIImage* image = nil;

  switch (position) {
    case ButtonStackButtonPositionPrimary:
      button = _primaryActionButton;
      break;
    case ButtonStackButtonPositionSecondary: {
      button = _secondaryActionButton;
      image = _configuration.secondaryActionImage;
      UIButtonConfiguration* config = button.configuration;
      config.image = image;
      button.configuration = config;
      break;
    }
    case ButtonStackButtonPositionTertiary:
      button = _tertiaryActionButton;
      break;
  }

  BOOL hasAction = string.length > 0 || image;
  button.hidden = !hasAction;
  if (hasAction) {
    if (button.style != style) {
      button.style = style;
    }
    button.title = string;
  }
}

// Creates a button with the given style.
- (ChromeButton*)createButtonForStyle:(ChromeButtonStyle)style {
  ChromeButton* button = [[ChromeButton alloc] initWithStyle:style];
  [button setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisVertical];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  return button;
}

// Sets up the layout constraints for the contentView and actionStackView.
- (void)setupConstraints {
  UILayoutGuide* safeAreaLayoutGuide = self.view.safeAreaLayoutGuide;
  UIView* view = self.view;

  // Scroll container view constraints.
  // Ensure the scroll container expands to the bottom of the safe area when the
  // action stack is empty.
  _scrollContainerBottomToSafeAreaBottomConstraint =
      [_scrollContainerView.bottomAnchor
          constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [_scrollContainerView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_scrollContainerView.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor],
    [_scrollContainerView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [_scrollContainerView.bottomAnchor
        constraintEqualToAnchor:_actionStackView.topAnchor],
    _scrollContainerBottomToSafeAreaBottomConstraint,
  ]];
  AddSameConstraints(_scrollView, _scrollContainerView);

  // contentView view constraints.
  _widthLayoutGuide = AddButtonStackContentWidthLayoutGuide(self.view);
  [NSLayoutConstraint activateConstraints:@[
    [_contentView.leadingAnchor
        constraintEqualToAnchor:_widthLayoutGuide.leadingAnchor],
    [_contentView.trailingAnchor
        constraintEqualToAnchor:_widthLayoutGuide.trailingAnchor],
    [_contentView.topAnchor constraintEqualToAnchor:_scrollView.topAnchor],
    [_contentView.bottomAnchor
        constraintEqualToAnchor:_scrollView.bottomAnchor],
  ]];

  // Ensures the content view either fills the scroll view (for short content)
  // or expands to enable scrolling (for long content).
  _contentViewHeightConstraint = [_contentView.heightAnchor
      constraintGreaterThanOrEqualToAnchor:_scrollView.heightAnchor];
  _contentViewHeightConstraint.priority = UILayoutPriorityDefaultLow;

  _actionStackSafeAreaBottomConstraint = [_actionStackView.bottomAnchor
      constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor];
  // Lower priority to avoid conflicts when the safe area bottom inset is zero.
  _actionStackSafeAreaBottomConstraint.priority = UILayoutPriorityDefaultHigh;

  _actionStackBottomConstraint = [_actionStackView.bottomAnchor
      constraintLessThanOrEqualToAnchor:view.bottomAnchor];

  // Make sure that both constraints have the right constant.
  [self reconfigureBottomConstraint];

  // Action stack view constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_actionStackView.leadingAnchor
        constraintEqualToAnchor:_widthLayoutGuide.leadingAnchor],
    [_actionStackView.trailingAnchor
        constraintEqualToAnchor:_widthLayoutGuide.trailingAnchor],
    _contentViewHeightConstraint,
    _actionStackBottomConstraint,
    _actionStackSafeAreaBottomConstraint,
  ]];
}

// Updates the buttons appearance and enabled state based on the current
// `isLoading` and `isConfirmed` flags.
- (void)updateButtonState {
  const BOOL showingProgressState =
      self.configuration.isLoading || self.configuration.isConfirmed;
  _primaryActionButton.enabled =
      self.configuration.primaryActionEnabled && !showingProgressState;
  _secondaryActionButton.enabled = !showingProgressState;
  _tertiaryActionButton.enabled = !showingProgressState;

  _primaryActionButton.imageView.accessibilityIdentifier = nil;
  _primaryActionButton.tunedDownStyle = self.configuration.isConfirmed;
  if (self.configuration.isLoading) {
    _primaryActionButton.primaryButtonImage = PrimaryButtonImageSpinner;
  } else if (self.configuration.isConfirmed) {
    _primaryActionButton.primaryButtonImage = PrimaryButtonImageCheckmark;
    _primaryActionButton.imageView.accessibilityIdentifier =
        kButtonStackCheckmarkSymbolAccessibilityIdentifier;
  } else {
    _primaryActionButton.primaryButtonImage = PrimaryButtonImageNone;
  }

  _primaryActionButton.title =
      showingProgressState ? @"" : self.configuration.primaryActionString;
}

// Handles the tap event for the primary action button.
- (void)handlePrimaryAction {
  [self.actionDelegate didTapPrimaryActionButton];
}

// Handles the tap event for the secondary action button.
- (void)handleSecondaryAction {
  [self.actionDelegate didTapSecondaryActionButton];
}

// Handles the tap event for the tertiary action button.
- (void)handleTertiaryAction {
  [self.actionDelegate didTapTertiaryActionButton];
}

// Calculates the detent height for the given context.
- (CGFloat)detentForPreferredHeightInContext:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  // Only activate this detent in portrait orientation on iPhone.
  UITraitCollection* traitCollection = context.containerTraitCollection;
  if (traitCollection.horizontalSizeClass != UIUserInterfaceSizeClassCompact ||
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassRegular) {
    return UISheetPresentationControllerDetentInactive;
  }

  CGFloat height = [self preferredHeightForContent];

  // Make sure detent is not larger than 75% of the maximum detent value but at
  // least as large as 25% of the maximum detent value.
  height = MIN(height, kMaxDetentMultiplier * context.maximumDetentValue);
  height = MAX(height, kMinDetentMultiplier * context.maximumDetentValue);
  return height;
}

// Updates the visibility of the gradient view based on scroll position.
- (void)updateGradientVisibility {
  // Determine if the gradient should be visible.
  CGFloat visibleHeight = _scrollView.bounds.size.height -
                          _scrollView.adjustedContentInset.top -
                          _scrollView.adjustedContentInset.bottom;
  BOOL scrollable = _scrollView.contentSize.height > visibleHeight;
  BOOL shouldShowGradient = _showsGradientView && scrollable;

  if (!shouldShowGradient) {
    _scrollContainerView.layer.mask = nil;
    return;
  }

  // Create mask if it doesn't exist.
  if (!_gradientMask) {
    _gradientMask = [CAGradientLayer layer];
    _gradientMask.endPoint = CGPointMake(0.0, 1.0);
    _gradientMask.colors = @[
      (id)self.view.backgroundColor.CGColor, (id)UIColor.clearColor.CGColor
    ];
  }

  // Apply mask and calculate geometry.
  _scrollContainerView.layer.mask = _gradientMask;
  _gradientMask.frame = _scrollContainerView.bounds;

  CGFloat startY =
      (kGradientHeight >= _gradientMask.frame.size.height)
          ? 0.0
          : 1.0 - (kGradientHeight / _gradientMask.frame.size.height);
  _gradientMask.startPoint = CGPointMake(0.0, startY);
}

@end
