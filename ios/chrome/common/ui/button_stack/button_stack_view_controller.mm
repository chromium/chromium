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
const CGFloat kButtonStackBottomMargin = 20.0;
const CGFloat kButtonStackHorizontalMargin = 16.0;

// The size of the checkmark symbol in the confirmation state on the primary
// button.
const CGFloat kSymbolConfirmationCheckmarkPointSize = 17;

// The position of a button in the stack.
typedef NS_ENUM(NSInteger, ButtonStackButtonPosition) {
  ButtonStackButtonPositionPrimary,
  ButtonStackButtonPositionSecondary,
  ButtonStackButtonPositionTertiary,
};

// Sets the activity indicator of the button in the button configuration.
void SetConfigurationActivityIndicator(ChromeButton* button,
                                       BOOL shows_activity_indicator) {
  UIButtonConfiguration* button_configuration = button.configuration;
  button_configuration.showsActivityIndicator = shows_activity_indicator;
  button.configuration = button_configuration;
}

}  // namespace

@interface ButtonStackViewController () <UIScrollViewDelegate>
// Publicly readable as `UIButton`, but privately writeable and backed by
// `ChromeButton` instances for internal use.
@property(nonatomic, strong, readwrite) UIView* contentView;
@property(nonatomic, strong, readwrite) ChromeButton* primaryActionButton;
@property(nonatomic, strong, readwrite) ChromeButton* secondaryActionButton;
@property(nonatomic, strong, readwrite) ChromeButton* tertiaryActionButton;
@end

@implementation ButtonStackViewController {
  // Configuration for the buttons.
  ButtonStackConfiguration* _configuration;
  // Stack view for the action buttons.
  UIStackView* _actionStackView;
  // The container for the scroll view.
  UIView* _scrollContainerView;
  // The scroll view containing the content.
  UIScrollView* _scrollView;
  // The gradient mask for the scroll view.
  CAGradientLayer* _gradientMask;

  // Whether the view is in the loading state.
  BOOL _isLoading;
  // Whether the view is in the confirmed state.
  BOOL _isConfirmed;
  // Whether the gradient view is shown.
  BOOL _showsGradientView;

  // The last applied styles for each button.
  ButtonStackButtonStyle _primaryButtonStyle;
  ButtonStackButtonStyle _secondaryButtonStyle;
  ButtonStackButtonStyle _tertiaryButtonStyle;
}

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _configuration = configuration;
    _isLoading = configuration.isLoading;
    _isConfirmed = configuration.isConfirmed;
    _scrollEnabled = YES;
    _showsVerticalScrollIndicator = YES;
    _showsGradientView = YES;

    // Set the default styles for the buttons.
    _primaryButtonStyle = configuration.primaryButtonStyle;
    _secondaryButtonStyle = configuration.secondaryButtonStyle;
    _tertiaryButtonStyle = configuration.tertiaryButtonStyle;
  }
  return self;
}

- (instancetype)init {
  return [self initWithConfiguration:[[ButtonStackConfiguration alloc] init]];
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

#pragma mark - ButtonStackConsumer

- (void)setLoading:(BOOL)loading {
  if (_isLoading == loading) {
    return;
  }
  _isLoading = loading;
  if (_isLoading) {
    // isLoading and isConfirmed are mutually exclusive.
    _isConfirmed = NO;
  }
  [self updateButtonState];
}

- (void)setConfirmed:(BOOL)confirmed {
  if (_isConfirmed == confirmed) {
    return;
  }
  _isConfirmed = confirmed;
  if (_isConfirmed) {
    // isLoading and isConfirmed are mutually exclusive.
    _isLoading = NO;
  }
  [self updateButtonState];
}

- (void)updateConfiguration:(ButtonStackConfiguration*)configuration {
  _configuration = configuration;
  _isLoading = configuration.isLoading;
  _isConfirmed = configuration.isConfirmed;
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
  self.tertiaryActionButton = [self createButtonForStyle:_tertiaryButtonStyle];
  [self.tertiaryActionButton addTarget:self
                                action:@selector(handleTertiaryAction)
                      forControlEvents:UIControlEventTouchUpInside];

  self.primaryActionButton = [self createButtonForStyle:_primaryButtonStyle];
  [self.primaryActionButton addTarget:self
                               action:@selector(handlePrimaryAction)
                     forControlEvents:UIControlEventTouchUpInside];

  self.secondaryActionButton =
      [self createButtonForStyle:_secondaryButtonStyle];
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
                             image:_configuration.primaryActionImage
                             style:_configuration.primaryButtonStyle];
  [self configureButtonForPosition:ButtonStackButtonPositionSecondary
                        withString:_configuration.secondaryActionString
                             image:_configuration.secondaryActionImage
                             style:_configuration.secondaryButtonStyle];
  [self configureButtonForPosition:ButtonStackButtonPositionTertiary
                        withString:_configuration.tertiaryActionString
                             image:_configuration.tertiaryActionImage
                             style:_configuration.tertiaryButtonStyle];
}

// Configures a button with the given properties.
- (void)configureButtonForPosition:(ButtonStackButtonPosition)position
                        withString:(NSString*)string
                             image:(UIImage*)image
                             style:(ButtonStackButtonStyle)style {
  ChromeButton* button;
  // Use a pointer to the cached style ivar to avoid repeating the update logic
  // for each button position. This allows the code to read from and write to
  // the correct ivar using a single, shared piece of logic.
  ButtonStackButtonStyle* cachedStyle;

  switch (position) {
    case ButtonStackButtonPositionPrimary:
      button = _primaryActionButton;
      cachedStyle = &_primaryButtonStyle;
      break;
    case ButtonStackButtonPositionSecondary:
      button = _secondaryActionButton;
      cachedStyle = &_secondaryButtonStyle;
      break;
    case ButtonStackButtonPositionTertiary:
      button = _tertiaryActionButton;
      cachedStyle = &_tertiaryButtonStyle;
      break;
  }

  BOOL hasAction = string.length > 0;
  button.hidden = !hasAction;
  if (hasAction) {
    if (*cachedStyle != style) {
      *cachedStyle = style;
      switch (style) {
        case ButtonStackButtonStylePrimary:
          UpdateButtonToMatchPrimaryAction(button);
          break;
        case ButtonStackButtonStyleSecondary:
          UpdateButtonToMatchSecondaryAction(button);
          break;
        case ButtonStackButtonStyleTertiary:
          UpdateButtonToMatchTertiaryAction(button);
          break;
        case ButtonStackButtonStylePrimaryDestructive:
          UpdateButtonToMatchPrimaryDestructiveAction(button);
          break;
      }
    }
    SetConfigurationTitle(button, string);
    SetConfigurationImage(button, image);
  }
}

// Creates a styled button using the factory functions from button_util.h.
- (ChromeButton*)createButtonForStyle:(ButtonStackButtonStyle)style {
  ChromeButton* button;
  switch (style) {
    case ButtonStackButtonStylePrimary:
      button = PrimaryActionButton();
      break;
    case ButtonStackButtonStyleSecondary:
      button = SecondaryActionButton();
      break;
    case ButtonStackButtonStyleTertiary:
      button = TertiaryActionButton();
      break;
    case ButtonStackButtonStylePrimaryDestructive:
      button = PrimaryDestructiveActionButton();
      break;
  }
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
  AddSameConstraints(_scrollView, _contentView);

  // Ensures the content view either fills the scroll view (for short content)
  // or expands to enable scrolling (for long content).
  NSLayoutConstraint* contentViewHeightConstraint = [_contentView.heightAnchor
      constraintEqualToAnchor:_scrollView.heightAnchor];
  contentViewHeightConstraint.priority = UILayoutPriorityDefaultLow;

  // Action stack view constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_actionStackView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:kButtonStackHorizontalMargin],
    [_actionStackView.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor
                       constant:-kButtonStackHorizontalMargin],
    [_actionStackView.bottomAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor
                       constant:-kButtonStackBottomMargin],
  ]];
}

// Updates the buttons appearance and enabled state based on the current
// `isLoading` and `isConfirmed` flags.
- (void)updateButtonState {
  const BOOL showingProgressState = _isLoading || _isConfirmed;
  _primaryActionButton.enabled = !showingProgressState;
  _secondaryActionButton.enabled = !showingProgressState;
  _tertiaryActionButton.enabled = !showingProgressState;

  if (_isConfirmed) {
    _primaryActionButton.tunedDownStyle = YES;
    // Use the system symbol name directly to avoid a dependency on the browser
    // layer's symbol helpers.
    UIImageSymbolConfiguration* symbol_configuration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:kSymbolConfirmationCheckmarkPointSize];
    SetConfigurationImage(_primaryActionButton,
                          [UIImage systemImageNamed:@"checkmark.circle.fill"
                                  withConfiguration:symbol_configuration]);
  } else {
    _primaryActionButton.tunedDownStyle = NO;
    SetConfigurationImage(_primaryActionButton,
                          _configuration.primaryActionImage);
  }
  SetConfigurationActivityIndicator(_primaryActionButton, _isLoading);

  SetConfigurationTitle(
      _primaryActionButton,
      showingProgressState ? @"" : _configuration.primaryActionString);
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
