// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_sheet_view.h"

#import "ios/chrome/browser/assistant/ui/assistant_bar_configuration.h"
#import "ios/chrome/browser/assistant/ui/assistant_bar_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Shadow styling.
constexpr float kShadowOpacity = 0.1f;
constexpr CGFloat kShadowRadius = 10.0;
constexpr CGSize kShadowOffset = {0, 5};

// Corner styling.
const CGFloat kCornerRadius = 22.0;

// Grabber styling.
constexpr CGFloat kGrabberWidth = 33.0;
constexpr CGFloat kGrabberHeight = 4.0;
constexpr CGFloat kGrabberTopMargin = 5.0;
constexpr CGFloat kGrabberAlpha = 0.24;

// Close button styling.
constexpr CGFloat kCloseButtonSize = 18.0;
constexpr CGFloat kCloseButtonMargin = 16.0;

// Header styling.
constexpr CGFloat kTitleVerticalMargin = 12.0;
constexpr CGFloat kTitleLeadingMargin = 18.0;

// Returns the background color for the sheet.
constexpr CGFloat kSheetBackgroundAlpha = 0.5;
UIColor* SheetBackgroundColor() {
  return [[UIColor colorNamed:kSecondaryBackgroundColor]
      colorWithAlphaComponent:kSheetBackgroundAlpha];
}

}  // namespace

@implementation AssistantSheetView {
  UILabel* _titleLabel;
  UIScrollView* _scrollView;
  UIButton* _closeButton;

  // Stack view for configurable leading buttons.
  UIStackView* _leadingButtonStackView;
  // Stack view for configurable trailing buttons and the close button.
  UIStackView* _trailingButtonStackView;

  // Constraints for title alignment that need to be updated dynamically.
  NSArray<NSLayoutConstraint*>* _titleAlignmentConstraints;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self configureSheetStyling];
    [self setUpSubviews];
  }
  return self;
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  // Update shadow path to match the view's bounds and corner radius.
  self.layer.shadowPath = [UIBezierPath bezierPathWithRoundedRect:self.bounds
                                                     cornerRadius:kCornerRadius]
                              .CGPath;
}

#pragma mark - Public

- (void)setTitle:(NSString*)title {
  _title = title;
  _titleLabel.text = _title;
}

// Sets the bar configuration and updates the UI.
- (void)setConfiguration:(AssistantBarConfiguration*)configuration {
  _configuration = configuration;
  [self setTitle:configuration.title];
  [self updateBarButtons];
}

- (CGFloat)preferredHeight {
  // Force layout to ensure subviews are sized correctly.
  [self layoutIfNeeded];

  CGFloat headerHeight =
      [_headerView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;

  // Calculate content height based on the width of the view.
  CGSize targetSize =
      CGSizeMake(self.bounds.size.width, UILayoutFittingCompressedSize.height);
  CGFloat contentHeight =
      [_contentView
            systemLayoutSizeFittingSize:targetSize
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;

  return headerHeight + contentHeight;
}

#pragma mark - Private

// Configures the visual styling of the sheet.
// TODO(crbug.com/390204874): Update the sheet styling to perfectly match the
// design.
- (void)configureSheetStyling {
  // Container for visual effects (Blur + Tint) that clips to corners.
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  UIVisualEffectView* blurView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blurView.translatesAutoresizingMaskIntoConstraints = NO;
  blurView.layer.cornerRadius = kCornerRadius;
  blurView.clipsToBounds = YES;
  [self insertSubview:blurView atIndex:0];

  AddSameConstraints(blurView, self);

  // Tint view overlaying the blur effect to provide the background color.
  UIView* tintView = [[UIView alloc] init];
  tintView.translatesAutoresizingMaskIntoConstraints = NO;
  tintView.backgroundColor = SheetBackgroundColor();
  [blurView.contentView
      addSubview:tintView];  // Add to visual effect contentView.

  AddSameConstraints(tintView, blurView);

  self.layer.shadowColor = [UIColor blackColor].CGColor;
  self.layer.shadowOpacity = kShadowOpacity;
  self.layer.shadowOffset = kShadowOffset;
  self.layer.shadowRadius = kShadowRadius;
}

// Sets up the view hierarchy by creating and adding subviews.
- (void)setUpSubviews {
  _headerView = [self createHeaderView];
  [self addSubview:_headerView];

  [self updateBarButtons];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_scrollView];

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_contentView];

  // Add a low-priority height constraint to allow content to fill the scroll
  // view if it's smaller, while still allowing it to grow larger.
  NSLayoutConstraint* contentViewHeightConstraint = [_contentView.heightAnchor
      constraintGreaterThanOrEqualToAnchor:_scrollView.heightAnchor];
  contentViewHeightConstraint.priority = UILayoutPriorityDefaultLow;

  [NSLayoutConstraint activateConstraints:@[
    // Header view constraints.
    [_headerView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_headerView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_headerView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],

    // Scroll view constraints.
    [_scrollView.topAnchor constraintEqualToAnchor:_headerView.bottomAnchor],
    [_scrollView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_scrollView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [_scrollView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],

    // Content view constraints.
    [_contentView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_contentView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [_contentView.topAnchor constraintEqualToAnchor:_scrollView.topAnchor],
    [_contentView.bottomAnchor
        constraintEqualToAnchor:_scrollView.bottomAnchor],
    contentViewHeightConstraint,
  ]];
}

// Creates the header view with Grabber and Title.
- (UIView*)createHeaderView {
  UIView* headerView = [[UIView alloc] init];
  headerView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* grabberView = [self createGrabberView];
  [headerView addSubview:grabberView];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [headerView addSubview:_titleLabel];

  [self setUpCloseButton];

  [NSLayoutConstraint activateConstraints:@[
    // Grabber.
    [grabberView.topAnchor constraintEqualToAnchor:headerView.topAnchor
                                          constant:kGrabberTopMargin],
    [grabberView.centerXAnchor
        constraintEqualToAnchor:headerView.centerXAnchor],
    [grabberView.widthAnchor constraintEqualToConstant:kGrabberWidth],
    [grabberView.heightAnchor constraintEqualToConstant:kGrabberHeight],

    // Title.
    [_titleLabel.topAnchor constraintEqualToAnchor:grabberView.bottomAnchor
                                          constant:kTitleVerticalMargin],
    [_titleLabel.bottomAnchor constraintEqualToAnchor:headerView.bottomAnchor
                                             constant:-kTitleVerticalMargin],
  ]];

  return headerView;
}

// Helper to create a standard horizontal stack view for bar buttons.
- (UIStackView*)createBarStackView {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.spacing = kCloseButtonMargin;
  stackView.alignment = UIStackViewAlignmentCenter;
  return stackView;
}

// Creates and configures the grabber view.
- (UIView*)createGrabberView {
  UIView* grabberView = [[UIView alloc] init];
  grabberView.translatesAutoresizingMaskIntoConstraints = NO;
  grabberView.backgroundColor =
      [[UIColor blackColor] colorWithAlphaComponent:kGrabberAlpha];
  grabberView.layer.cornerRadius = kGrabberHeight / 2.0;
  return grabberView;
}

// Configures the Close button.
- (void)setUpCloseButton {
  _closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [_closeButton
      setImage:DefaultSymbolWithPointSize(kXMarkSymbol, kCloseButtonSize)
      forState:UIControlStateNormal];
  _closeButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
}

// Updates the navigation bar buttons based on the current configuration.
- (void)updateBarButtons {
  [self resetAndCreateStackViews];

  // Populate leading stack.
  for (AssistantBarItem* buttonConfig in _configuration.leadingButtons) {
    UIButton* button = [self createButtonForConfig:buttonConfig];
    [_leadingButtonStackView addArrangedSubview:button];
  }

  // Populate trailing stack.
  for (AssistantBarItem* buttonConfig in _configuration.trailingButtons) {
    UIButton* button = [self createButtonForConfig:buttonConfig];
    [_trailingButtonStackView addArrangedSubview:button];
  }
  [_trailingButtonStackView addArrangedSubview:_closeButton];

  [NSLayoutConstraint activateConstraints:@[
    // Leading stack.
    [_leadingButtonStackView.leadingAnchor
        constraintEqualToAnchor:_headerView.leadingAnchor
                       constant:kCloseButtonMargin],
    [_leadingButtonStackView.centerYAnchor
        constraintEqualToAnchor:_titleLabel.centerYAnchor],

    // Trailing stack.
    [_trailingButtonStackView.trailingAnchor
        constraintEqualToAnchor:_headerView.trailingAnchor
                       constant:-kCloseButtonMargin],
    [_trailingButtonStackView.centerYAnchor
        constraintEqualToAnchor:_titleLabel.centerYAnchor],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_trailingButtonStackView.leadingAnchor
                                 constant:-kCloseButtonMargin],
  ]];

  [self updateTitleAlignment];
}

// Resets and recreates the stack views.
- (void)resetAndCreateStackViews {
  [_leadingButtonStackView removeFromSuperview];
  [_trailingButtonStackView removeFromSuperview];
  if (_titleAlignmentConstraints) {
    [NSLayoutConstraint deactivateConstraints:_titleAlignmentConstraints];
  }

  _leadingButtonStackView = [self createBarStackView];
  [_headerView addSubview:_leadingButtonStackView];

  _trailingButtonStackView = [self createBarStackView];
  [_headerView addSubview:_trailingButtonStackView];
}

// Updates title alignment based on configuration.
- (void)updateTitleAlignment {
  NSArray<NSLayoutConstraint*>* titleAlignmentConstraints;

  if (_configuration.leadingButtons.count > 0) {
    // Leading buttons exist: Center title + avoid leading stack.
    titleAlignmentConstraints = @[
      [_titleLabel.centerXAnchor
          constraintEqualToAnchor:_headerView.centerXAnchor],
      [_titleLabel.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:_leadingButtonStackView
                                                   .trailingAnchor
                                      constant:kCloseButtonMargin]
    ];
    _titleLabel.textAlignment = NSTextAlignmentCenter;
  } else {
    // No leading buttons: Lead title (ignore leading stack margin).
    titleAlignmentConstraints = @[ [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_headerView.leadingAnchor
                       constant:kTitleLeadingMargin] ];
    _titleLabel.textAlignment = NSTextAlignmentNatural;
  }

  _titleAlignmentConstraints = titleAlignmentConstraints;
  [NSLayoutConstraint activateConstraints:_titleAlignmentConstraints];
}

// Creates a button for the given configuration.
- (UIButton*)createButtonForConfig:(AssistantBarItem*)config {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setImage:config.image forState:UIControlStateNormal];
  button.accessibilityLabel = config.accessibilityLabel;
  button.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  UIAction* action = [UIAction actionWithHandler:^(UIAction* actionHandler) {
    if (config.action) {
      config.action();
    }
  }];
  [button addAction:action forControlEvents:UIControlEventTouchUpInside];

  return button;
}

@end
