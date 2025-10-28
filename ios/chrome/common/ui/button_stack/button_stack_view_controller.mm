// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

const CGFloat kButtonSpacing = 8.0;
const CGFloat kButtonStackHorizontalMargin = 16.0;

const CGFloat kButtonStackBottomMargin = 20.0;
const CGFloat kLegacyButtonStackBottomMargin = 0.0;

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
@property(nonatomic, strong, readwrite) ButtonStackConfiguration* configuration;
@property(nonatomic, strong, readwrite) ChromeButton* primaryActionButton;
@property(nonatomic, strong, readwrite) ChromeButton* secondaryActionButton;
@property(nonatomic, strong, readwrite) ChromeButton* tertiaryActionButton;

@end

@implementation ButtonStackViewController {
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
}

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _configuration = configuration;
    _scrollEnabled = YES;
    _showsVerticalScrollIndicator = YES;
    _showsGradientView = YES;
    _actionStackBottomMargin = kButtonStackBottomMargin;
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
  [_scrollContainerView addSubview:_scrollView];

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_contentView];

  [self createAndConfigureButtons];
  [self setupConstraints];
  [self updateButtonState];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_scrollView flashScrollIndicators];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  if (self.customGradientViewHeight > 0 && !_gradientMask) {
    _gradientMask = [CAGradientLayer layer];
    _gradientMask.endPoint = CGPointMake(0.0, 1.0);
    UIColor* bottomColor = BlendColors(
        [UIColor clearColor], self.view.backgroundColor,
        _scrollView.contentInset.bottom / self.customGradientViewHeight);
    _gradientMask.colors =
        @[ (id)self.view.backgroundColor.CGColor, (id)bottomColor.CGColor ];
    _scrollContainerView.layer.mask = _showsGradientView ? _gradientMask : nil;
  }

  if (_gradientMask) {
    CGFloat effectiveGradientHeight =
        self.customGradientViewHeight - _scrollView.contentInset.bottom;
    if (effectiveGradientHeight <= 0) {
      return;
    }

    _gradientMask.frame = _scrollContainerView.bounds;

    CGFloat startY =
        (effectiveGradientHeight >= _gradientMask.frame.size.height)
            ? 0.0
            : 1.0 - (effectiveGradientHeight / _gradientMask.frame.size.height);
    _gradientMask.startPoint = CGPointMake(0.0, startY);
  }
}

#pragma mark - Public

- (BOOL)isScrolledToBottom {
  CGFloat scrollPosition =
      _scrollView.contentOffset.y + _scrollView.frame.size.height;
  CGFloat scrollLimit =
      _scrollView.contentSize.height + _scrollView.contentInset.bottom;
  return scrollPosition >= scrollLimit;
}

- (void)scrollToBottom {
  CGFloat scrollLimit = _scrollView.contentSize.height -
                        _scrollView.bounds.size.height +
                        _scrollView.contentInset.bottom;
  [_scrollView setContentOffset:CGPointMake(0, scrollLimit) animated:YES];
}

- (CGFloat)buttonStackHeight {
  // Calculate the size the stack view needs without forcing a full layout pass.
  CGFloat stackHeight =
      [_actionStackView
          systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
  if (stackHeight > 0) {
    return stackHeight + self.actionStackBottomMargin;
  }
  return 0;
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
  _scrollContainerView.layer.mask = _showsGradientView ? _gradientMask : nil;
}

- (void)setActionStackBottomMargin:(CGFloat)actionStackBottomMargin {
  _actionStackBottomMargin = actionStackBottomMargin;
  if (_actionStackSafeAreaBottomConstraint) {
    _actionStackSafeAreaBottomConstraint.constant = -_actionStackBottomMargin;
  }
  if (_actionStackBottomConstraint) {
    _actionStackBottomConstraint.constant = -_actionStackBottomMargin;
  }
}

#pragma mark - Private

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
  return scrollView;
}

// Creates the button stack and configures the buttons based on the initial
// configuration.
- (void)createAndConfigureButtons {
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

  [self reconfigureButtons];
}

// Updates the buttons' visibility, titles, and images based on the current
// configuration.
- (void)reconfigureButtons {
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

  // Scroll container view constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_scrollContainerView.topAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor],
    [_scrollContainerView.bottomAnchor
        constraintEqualToAnchor:_actionStackView.topAnchor],
    [_scrollContainerView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor],
    [_scrollContainerView.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor],
  ]];
  AddSameConstraints(_scrollView, _scrollContainerView);

  // contentView view constraints.
  AddSameConstraints(_scrollView, _contentView);
  [NSLayoutConstraint activateConstraints:@[
    [_contentView.widthAnchor constraintEqualToAnchor:_scrollView.widthAnchor],
  ]];

  // Ensures the content view either fills the scroll view (for short content)
  // or expands to enable scrolling (for long content).
  NSLayoutConstraint* contentViewHeightConstraint = [_contentView.heightAnchor
      constraintEqualToAnchor:_scrollView.heightAnchor];
  contentViewHeightConstraint.priority = UILayoutPriorityDefaultLow;

  _actionStackSafeAreaBottomConstraint = [_actionStackView.bottomAnchor
      constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor
                     constant:-self.actionStackBottomMargin];
  // Lower priority to avoid conflicts when the safe area bottom inset is zero.
  _actionStackSafeAreaBottomConstraint.priority = UILayoutPriorityDefaultHigh;

  _actionStackBottomConstraint = [_actionStackView.bottomAnchor
      constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                               constant:-self.actionStackBottomMargin];

  // Action stack view constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_actionStackView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:kButtonStackHorizontalMargin],
    [_actionStackView.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor
                       constant:-kButtonStackHorizontalMargin],
    _actionStackBottomConstraint,
    _actionStackSafeAreaBottomConstraint,
  ]];
}

// Updates the buttons appearance and enabled state based on the current
// `isLoading` and `isConfirmed` flags.
- (void)updateButtonState {
  const BOOL showingProgressState =
      self.configuration.isLoading || self.configuration.isConfirmed;
  _primaryActionButton.enabled = !showingProgressState;
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

@end
