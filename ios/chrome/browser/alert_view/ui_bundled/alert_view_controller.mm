// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/alert_view/ui_bundled/alert_view_controller.h"

#import <ostream>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/text_field_configuration.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Tag for the button stack view.
constexpr NSInteger kButtonStackViewTag = 9998;

// Properties of the alert shadow.
constexpr CGFloat kShadowOffsetX = 0;
constexpr CGFloat kShadowOffsetY = 15;
constexpr CGFloat kShadowRadius = 13;
constexpr float kShadowOpacity = 0.12;

// Properties of the alert view.
constexpr CGFloat kLegacyCornerRadius = 14;

constexpr CGFloat kCornerRadius = 34;
constexpr CGFloat kAlertWidth = 270;
constexpr CGFloat kAlertWidthAccessibility = 402;
constexpr CGFloat kTextFieldCornerRadius = 5;
constexpr CGFloat kMinimumHeight = 30;
constexpr CGFloat kMinimumMargin = 4;

// Inset of the alert content.
constexpr CGFloat kAlertMarginTop = 22;

constexpr CGFloat kAlertMarginBottom = 16;

// Space before the actions and everything else.
constexpr CGFloat kAlertActionsSpacing = 12;

// Insets for the content in the alert view.
constexpr CGFloat kTitleInsetLeading = 20;
constexpr CGFloat kTitleInsetBottom = 9;
constexpr CGFloat kTitleInsetTrailing = 20;

constexpr CGFloat kTitleHorizontalInset = 30;

constexpr CGFloat kSpinnerInsetTop = 12;
constexpr CGFloat kSpinnerInsetBottom = 14;

constexpr CGFloat kConfirmationSymbolPointSize = 22;

constexpr CGFloat kMessageInsetLeading = 20;
constexpr CGFloat kMessageInsetBottom = 6;
constexpr CGFloat kMessageInsetTrailing = 20;

constexpr CGFloat kMessageHorizontalInset = 30;

constexpr CGFloat kLottieImageAspectRatio = 105.0f / 270.0f;

constexpr CGFloat kButtonInsetTop = 13;
constexpr CGFloat kButtonInsetLeading = 20;
constexpr CGFloat kButtonInsetBottom = 13;
constexpr CGFloat kButtonInsetTrailing = 20;

constexpr CGFloat kButtonHorizontalInnerInset = 12;
constexpr CGFloat kButtonVerticalInnerInset = 15.5;
constexpr CGFloat kButtonHorizontalInset = 16;
constexpr CGFloat kButtonCornerRadius = 24;
constexpr CGFloat kButtonStackViewSpacing = 6;

constexpr CGFloat kTextfieldStackInsetTop = 12;
constexpr CGFloat kTextfieldStackInsetLeading = 12;
constexpr CGFloat kTextfieldStackInsetTrailing = 12;

constexpr CGFloat kTextfieldInset = 8;

// This is how many bits UIViewAnimationCurve needs to be shifted to be in
// UIViewAnimationOptions format. Must match the one in UIView.h.
constexpr NSUInteger kUIViewAnimationCurveToOptionsShift = 16;

// The amount of time (in seconds) to wait before enabling the action buttons.
// This is only used if `actionButtonsAreInitiallyDisabled` is true.
constexpr NSTimeInterval kEnableActionButtonsDelay = 0.5;

// String text provider identifiers for the Lottie context image.
NSString* const kLockScreen = @"lock_screen";
NSString* const kNotificationCenter = @"notification_center";
NSString* const kBanners = @"banners";

// Returns the width and height of a single pixel in point.
CGFloat GetPixelLength() {
  return 1.0 / [UIScreen mainScreen].scale;
}

// Returns the width of the alert.
CGFloat GetAlertWidth() {
  BOOL is_a11y_content_size = UIContentSizeCategoryIsAccessibilityCategory(
      [UIApplication sharedApplication].preferredContentSizeCategory);
  return is_a11y_content_size ? kAlertWidthAccessibility : kAlertWidth;
}

// Positions the content view on the screen.
void PositionContentViewInParentView(UIView* contentView, UIView* parentView) {
  [NSLayoutConstraint activateConstraints:@[
    [contentView.centerXAnchor
        constraintEqualToAnchor:parentView.safeAreaLayoutGuide.centerXAnchor],
    [contentView.centerYAnchor
        constraintEqualToAnchor:parentView.safeAreaLayoutGuide.centerYAnchor],

    // Minimum Size.
    [contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kMinimumHeight],

    // Maximum Size.
    [contentView.topAnchor
        constraintGreaterThanOrEqualToAnchor:parentView.safeAreaLayoutGuide
                                                 .topAnchor
                                    constant:kMinimumMargin],
    [contentView.bottomAnchor
        constraintLessThanOrEqualToAnchor:parentView.safeAreaLayoutGuide
                                              .bottomAnchor
                                 constant:-kMinimumMargin],
    [contentView.trailingAnchor
        constraintLessThanOrEqualToAnchor:parentView.safeAreaLayoutGuide
                                              .trailingAnchor
                                 constant:-kMinimumMargin],
    [contentView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:parentView.safeAreaLayoutGuide
                                                 .leadingAnchor
                                    constant:kMinimumMargin],
  ]];
}

// Adds a grey line with a thickness of 1px to `stackView`, used to create a
// separator that visually separates different elements.
void AddSeparatorToStackView(UIStackView* stackView) {
  if (@available(iOS 26, *)) {
    return;
  }

  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  [stackView addArrangedSubview:separator];
  if (stackView.axis == UILayoutConstraintAxisHorizontal) {
    [separator.widthAnchor constraintEqualToConstant:GetPixelLength()].active =
        YES;
    AddSameConstraintsToSides(stackView, separator,
                              LayoutSides::kTop | LayoutSides::kBottom);
  } else {
    [separator.heightAnchor constraintEqualToConstant:GetPixelLength()].active =
        YES;
    AddSameConstraintsToSides(stackView, separator,
                              LayoutSides::kTrailing | LayoutSides::kLeading);
  }
}

// Returns the color for the given button `style` and `enabled` state.
UIColor* ColorForActionStyle(UIAlertActionStyle style, BOOL enabled) {
  UIColor* enabledStateDefaultColor = [UIColor colorNamed:kBlueColor];

  if (@available(iOS 26, *)) {
    enabledStateDefaultColor = [UIColor colorNamed:kTextPrimaryColor];
  }

  UIColor* enabledStateDestructiveColor = [UIColor colorNamed:kRedColor];
  UIColor* disabledStateColor = [UIColor lightGrayColor];

  if (!enabled) {
    return disabledStateColor;
  }

  switch (style) {
    case UIAlertActionStyleDefault:
      return enabledStateDefaultColor;
    case UIAlertActionStyleCancel:
      return enabledStateDefaultColor;
    case UIAlertActionStyleDestructive:
      return enabledStateDestructiveColor;
  }
}

// Returns the background color for the given button `state`.
UIColor* BackgroundColorForState(UIControlState state) {
  if (@available(iOS 26, *)) {
    switch (state) {
      case UIControlStateNormal:
        return UIColor.tertiarySystemFillColor;
      case UIControlStateHighlighted:
      case UIControlStateFocused:
      case UIControlStateSelected:
        return UIColor.quaternarySystemFillColor;
      case UIControlStateApplication:
      case UIControlStateReserved:
        break;
    }
  }

  return UIColor.clearColor;
}

// Update the button foreground color depending on its state.
void UpdateButtonColorDependingOnEnabledState(UIAlertActionStyle style,
                                              UIButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  if (!configuration) {
    return;
  }

  UIColor* color = ColorForActionStyle(style, button.enabled);
  UIColor* backgroundColor = BackgroundColorForState(button.state);
  if (![configuration.baseForegroundColor isEqual:color] ||
      ![configuration.background.backgroundColor isEqual:backgroundColor]) {
    configuration = [configuration copy];
    configuration.baseForegroundColor = color;
    UIBackgroundConfiguration* backgroundConfiguration =
        configuration.background;
    backgroundConfiguration.backgroundColor = backgroundColor;
    configuration.background = backgroundConfiguration;
    button.configuration = configuration;
  }
}

// Returns a button to be added to the alert for `action` for ios 18 or lower.
UIButton* GetLegacyButtonForAction(AlertAction* action) {
  UIFont* font = nil;

  if (action.style == UIAlertActionStyleCancel) {
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  } else {
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  }

  UIButtonConfiguration* buttonConfiguration;
  buttonConfiguration = [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets =
      NSDirectionalEdgeInsetsMake(kButtonInsetTop, kButtonInsetLeading,
                                  kButtonInsetBottom, kButtonInsetTrailing);

  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSAttributedString* title =
      [[NSAttributedString alloc] initWithString:action.title
                                      attributes:attributes];
  buttonConfiguration.attributedTitle = title;
  buttonConfiguration.baseForegroundColor =
      ColorForActionStyle(action.style, action.enabled);

  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:nil];

  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.tag = action.uniqueIdentifier;
  button.enabled = action.enabled;

  UIAlertActionStyle style = action.style;
  button.configurationUpdateHandler = ^(UIButton* updatedButton) {
    UpdateButtonColorDependingOnEnabledState(style, updatedButton);
  };

  return button;
}

// Returns a button to be added to the alert for `action`.
UIButton* GetButtonForAction(AlertAction* action) {
  if (@available(iOS 26, *)) {
    UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        kButtonVerticalInnerInset, kButtonHorizontalInnerInset,
        kButtonVerticalInnerInset, kButtonHorizontalInnerInset);
    buttonConfiguration.background.cornerRadius = kButtonCornerRadius;
    buttonConfiguration.baseForegroundColor =
        ColorForActionStyle(action.style, action.enabled);

    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSAttributedString* title =
        [[NSAttributedString alloc] initWithString:action.title
                                        attributes:attributes];
    buttonConfiguration.attributedTitle = title;

    UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                           primaryAction:nil];
    button.pointerInteractionEnabled = YES;
    button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();
    button.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentCenter;
    button.translatesAutoresizingMaskIntoConstraints = NO;

    button.tag = action.uniqueIdentifier;
    button.enabled = action.enabled;

    UIAlertActionStyle style = action.style;
    button.configurationUpdateHandler = ^(UIButton* updatedButton) {
      UpdateButtonColorDependingOnEnabledState(style, updatedButton);
    };
    return button;

  } else {
    return GetLegacyButtonForAction(action);
  }
}

}  // namespace

@interface AlertViewController () <UITextFieldDelegate,
                                   UIGestureRecognizerDelegate>

// The actions for to this alert. `copy` for safety against mutable objects.
@property(nonatomic, copy) NSArray<NSArray<AlertAction*>*>* actions;

// This maps UIButtons' tags with AlertActions' uniqueIdentifiers.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, AlertAction*>* buttonAlertActionsDictionary;

// This is the view with the shadow, white background and round corners.
// Everything will be added here.
@property(nonatomic, strong) UIView* contentView;

// The message of the alert, will appear after the title.
@property(nonatomic, copy) NSString* message;

// Text field configurations for this alert. One text field will be created for
// each `TextFieldConfiguration`. `copy` for safety against mutable objects.
@property(nonatomic, copy)
    NSArray<TextFieldConfiguration*>* textFieldConfigurations;

// The alert view's accessibility identifier.
@property(nonatomic, copy) NSString* alertAccessibilityIdentifier;

// The text fields that had been added to this alert.
@property(nonatomic, strong) NSArray<UITextField*>* textFields;

// Recognizer used to dismiss the keyboard when tapping outside the container
// view.
@property(nonatomic, strong) UITapGestureRecognizer* tapRecognizer;

// Recognizer used to dismiss the keyboard swipping down the alert.
@property(nonatomic, strong) UISwipeGestureRecognizer* swipeRecognizer;

// This is the last focused text field, the gestures to dismiss the keyboard
// will end up calling `resignFirstResponder` on this.
@property(nonatomic, weak) UITextField* lastFocusedTextField;

// This holds the text field stack view.
@property(nonatomic, strong) UIView* textFieldStackHolder;

// Whether the activity indicator should be visible in the alert view.
@property(nonatomic, assign) BOOL shouldShowActivityIndicator;

// Whether the action buttons should initially be disabled.
@property(nonatomic, assign) BOOL actionButtonsAreInitiallyDisabled;

// The Lottie image names for the image in the alert.
@property(nonatomic, copy) NSString* imageLottieName;
@property(nonatomic, copy) NSString* imageDarkModeLottieName;

// Custom animation view used for the image in this alert.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapper;

// Custom animation view used for the image in this alert in dark mode.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapperDarkMode;

@end

@implementation AlertViewController {
  // The spinner view shown between the title and the content message, it is
  // shown only when shouldShowActivityIndicator is true.
  UIActivityIndicatorView* _spinner;
  // The checkmark shown when the pending state suggested by the _spinner ends.
  // It replaces the _spinner in the view.
  UIImageView* _checkmark;

  UIView* _progressIndicatorContainerView;
}

#pragma mark - Public

- (void)loadView {
  [super loadView];
  self.view.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  self.view.accessibilityViewIsModal = YES;

  self.tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(dismissKeyboard)];
  self.tapRecognizer.enabled = NO;
  self.tapRecognizer.delegate = self;
  [self.view addGestureRecognizer:self.tapRecognizer];

  [self configureContentView];
  self.swipeRecognizer = [[UISwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(dismissKeyboard)];
  self.swipeRecognizer.direction = UISwipeGestureRecognizerDirectionDown;
  self.swipeRecognizer.enabled = NO;
  [self.contentView addGestureRecognizer:self.swipeRecognizer];

  NSLayoutConstraint* widthConstraint =
      [self.contentView.widthAnchor constraintEqualToConstant:GetAlertWidth()];
  widthConstraint.priority = UILayoutPriorityRequired - 1;

  [[NSNotificationCenter defaultCenter]
      addObserverForName:UIContentSizeCategoryDidChangeNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification* _Nonnull note) {
                widthConstraint.constant = GetAlertWidth();
              }];
  widthConstraint.active = YES;
  PositionContentViewInParentView(self.contentView, self.view);

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.delaysContentTouches = NO;
  scrollView.showsVerticalScrollIndicator = YES;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.scrollEnabled = YES;
  scrollView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentAlways;
  [self.contentView addSubview:scrollView];
  AddSameConstraints(scrollView, self.contentView);

  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentCenter;
  [scrollView addSubview:stackView];

  NSLayoutConstraint* heightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.heightAnchor];
  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  NSDirectionalEdgeInsets stackViewInsets =
      NSDirectionalEdgeInsetsMake(kAlertMarginTop, 0, 0, 0);

  if (@available(iOS 26, *)) {
    stackViewInsets =
        NSDirectionalEdgeInsetsMake(kAlertMarginTop, 0, kAlertMarginBottom, 0);
  }

  AddSameConstraintsWithInsets(stackView, scrollView, stackViewInsets);

  if (self.title.length) {
    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.numberOfLines = 0;
    titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    titleLabel.adjustsFontForContentSizeCategory = YES;
    titleLabel.textAlignment = NSTextAlignmentCenter;

    if (@available(iOS 26, *)) {
      titleLabel.textAlignment = NSTextAlignmentNatural;
    }

    titleLabel.text = self.title;
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    [stackView addArrangedSubview:titleLabel];
    [stackView setCustomSpacing:self.shouldShowActivityIndicator
                                    ? kTitleInsetBottom + kSpinnerInsetTop
                                    : kTitleInsetBottom
                      afterView:titleLabel];

    NSDirectionalEdgeInsets titleInsets = NSDirectionalEdgeInsetsMake(
        0, kTitleInsetLeading, 0, kTitleInsetTrailing);

    if (@available(iOS 26, *)) {
      titleInsets = NSDirectionalEdgeInsetsMake(0, kTitleHorizontalInset, 0,
                                                kTitleHorizontalInset);
    }

    AddSameConstraintsToSidesWithInsets(
        titleLabel, self.contentView,
        LayoutSides::kTrailing | LayoutSides::kLeading, titleInsets);
  }

  if (self.shouldShowActivityIndicator) {
    _progressIndicatorContainerView = [[UIView alloc] init];
    _progressIndicatorContainerView.translatesAutoresizingMaskIntoConstraints =
        NO;
    [stackView addArrangedSubview:_progressIndicatorContainerView];

    _spinner = GetLargeUIActivityIndicatorView();
    _spinner.translatesAutoresizingMaskIntoConstraints = NO;

    _checkmark = [[UIImageView alloc] init];
    _checkmark.image = DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                                  kConfirmationSymbolPointSize);
    _checkmark.tintColor = [UIColor systemGreenColor];
    _checkmark.translatesAutoresizingMaskIntoConstraints = NO;

    [_progressIndicatorContainerView addSubview:_spinner];
    [_progressIndicatorContainerView addSubview:_checkmark];

    [NSLayoutConstraint activateConstraints:@[
      [_progressIndicatorContainerView.heightAnchor
          constraintEqualToAnchor:_spinner.heightAnchor],
      [_progressIndicatorContainerView.widthAnchor
          constraintEqualToAnchor:_spinner.widthAnchor],
      [_spinner.centerXAnchor
          constraintEqualToAnchor:_progressIndicatorContainerView
                                      .centerXAnchor],
      [_spinner.centerYAnchor
          constraintEqualToAnchor:_progressIndicatorContainerView
                                      .centerYAnchor],

      [_checkmark.centerXAnchor
          constraintEqualToAnchor:_progressIndicatorContainerView
                                      .centerXAnchor],
      [_checkmark.centerYAnchor
          constraintEqualToAnchor:_progressIndicatorContainerView.centerYAnchor]
    ]];

    [stackView setCustomSpacing:kSpinnerInsetBottom
                      afterView:_progressIndicatorContainerView];

    [self setProgressState:ProgressIndicatorStateActivity];
  }

  if (self.message.length) {
    UILabel* messageLabel = [[UILabel alloc] init];
    messageLabel.numberOfLines = 0;
    messageLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    messageLabel.adjustsFontForContentSizeCategory = YES;
    messageLabel.textAlignment = NSTextAlignmentCenter;

    if (@available(iOS 26, *)) {
      messageLabel.textAlignment = NSTextAlignmentNatural;
    }

    messageLabel.text = self.message;
    messageLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:messageLabel];
    [stackView setCustomSpacing:kMessageInsetBottom afterView:messageLabel];

    NSDirectionalEdgeInsets messageInsets = NSDirectionalEdgeInsetsMake(
        0, kMessageInsetLeading, 0, kMessageInsetTrailing);

    if (@available(iOS 26, *)) {
      messageInsets = NSDirectionalEdgeInsetsMake(0, kMessageHorizontalInset, 0,
                                                  kMessageHorizontalInset);
    }

    AddSameConstraintsToSidesWithInsets(
        messageLabel, self.contentView,
        LayoutSides::kTrailing | LayoutSides::kLeading, messageInsets);
  }

  if (self.imageLottieName) {
    [self configureAnimationViewWrapper];
    [stackView addArrangedSubview:self.animationViewWrapper.animationView];
    [stackView addSubview:self.animationViewWrapperDarkMode.animationView];
    AddSameConstraints(self.animationViewWrapperDarkMode.animationView,
                       self.animationViewWrapper.animationView);
    // Ensure the image can expand to fill space for larger font sizes.
    [NSLayoutConstraint activateConstraints:@[
      [self.animationViewWrapper.animationView.heightAnchor
          constraintEqualToAnchor:self.animationViewWrapper.animationView
                                      .widthAnchor
                       multiplier:kLottieImageAspectRatio],
      [self.animationViewWrapper.animationView.widthAnchor
          constraintEqualToAnchor:self.contentView.widthAnchor]
    ]];

    [self selectImageForCurrentStyle];
  }

  if (self.textFieldConfigurations.count) {
    // Updates the custom space before the text fields to account for their
    // inset.
    UIView* previousView = stackView.arrangedSubviews.lastObject;
    if (previousView) {
      CGFloat spaceBefore = [stackView customSpacingAfterView:previousView];
      [stackView setCustomSpacing:kTextfieldStackInsetTop + spaceBefore
                        afterView:previousView];
    }
    [stackView addArrangedSubview:self.textFieldStackHolder];
    NSDirectionalEdgeInsets stackHolderContentInsets =
        NSDirectionalEdgeInsetsMake(0, kTextfieldStackInsetLeading, 0,
                                    kTextfieldStackInsetTrailing);
    AddSameConstraintsToSidesWithInsets(
        self.textFieldStackHolder, self.contentView,
        LayoutSides::kTrailing | LayoutSides::kLeading,
        stackHolderContentInsets);
  }

  UIView* lastArrangedView = stackView.arrangedSubviews.lastObject;
  if (lastArrangedView && !self.imageLottieName) {
    [stackView setCustomSpacing:kAlertActionsSpacing
                      afterView:lastArrangedView];
  }

  if ([self.actions count] > 0) {
    UIStackView* buttonStackView = [self createButtonStackView];
    buttonStackView.tag = kButtonStackViewTag;
    [stackView addArrangedSubview:buttonStackView];

    NSDirectionalEdgeInsets buttonStackHorizontalInsets =
        NSDirectionalEdgeInsetsZero;

    if (@available(iOS 26, *)) {
      buttonStackHorizontalInsets = NSDirectionalEdgeInsetsMake(
          0, kButtonHorizontalInset, 0, kButtonHorizontalInset);
    }

    AddSameConstraintsToSidesWithInsets(
        buttonStackView, self.contentView,
        LayoutSides::kLeading | LayoutSides::kTrailing,
        buttonStackHorizontalInsets);
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleKeyboardWillShow:)
             name:UIKeyboardWillShowNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleKeyboardWillHide:)
             name:UIKeyboardWillHideNotification
           object:nil];

  NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
    UITraitUserInterfaceIdiom.class, UITraitUserInterfaceStyle.class,
    UITraitDisplayGamut.class, UITraitAccessibilityContrast.class,
    UITraitUserInterfaceLevel.class
  ]);
  __weak __typeof(self) weakSelf = self;
  UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                   UITraitCollection* previousCollection) {
    [weakSelf updateBorderColorOnTraitChange:previousCollection];
  };
  [self registerForTraitChanges:traits withHandler:handler];

  traits = TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
  [self registerForTraitChanges:traits
                     withAction:@selector(selectImageForCurrentStyle)];
}

#pragma mark - Getters

- (NSArray<UITextField*>*)textFields {
  if (!_textFields) {
    NSMutableArray<UITextField*>* textFields = [[NSMutableArray alloc]
        initWithCapacity:self.textFieldConfigurations.count];
    for (TextFieldConfiguration* textFieldConfiguration in self
             .textFieldConfigurations) {
      UITextField* textField = [[UITextField alloc] init];
      textField.text = textFieldConfiguration.text;
      textField.placeholder = textFieldConfiguration.placeholder;
      textField.autocapitalizationType =
          textFieldConfiguration.autocapitalizationType;
      textField.secureTextEntry = textFieldConfiguration.secureTextEntry;
      textField.accessibilityIdentifier =
          textFieldConfiguration.accessibilityIdentifier;
      textField.translatesAutoresizingMaskIntoConstraints = NO;
      textField.delegate = self;
      textField.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      textField.adjustsFontForContentSizeCategory = YES;
      [textFields addObject:textField];
    }
    _textFields = textFields;
  }
  return _textFields;
}

- (NSArray<NSString*>*)textFieldResults {
  if (!self.textFields) {
    return nil;
  }
  NSMutableArray<NSString*>* results =
      [[NSMutableArray alloc] initWithCapacity:self.textFields.count];
  for (UITextField* textField in self.textFields) {
    [results addObject:textField.text];
  }
  return results;
}

- (UIView*)textFieldStackHolder {
  if (!_textFieldStackHolder) {
    // `stackHolder` has the background, border and round corners of the stacked
    // fields.
    _textFieldStackHolder = [[UIView alloc] init];
    _textFieldStackHolder.layer.cornerRadius = kTextFieldCornerRadius;
    _textFieldStackHolder.layer.borderColor =
        [UIColor colorNamed:kSeparatorColor].CGColor;
    // Use performAsCurrentTraitCollection to get the correct CGColor for the
    // given dynamic color and current userInterfaceStyle.
    [self.traitCollection performAsCurrentTraitCollection:^{
      _textFieldStackHolder.layer.borderColor =
          [UIColor colorNamed:kSeparatorColor].CGColor;
    }];
    _textFieldStackHolder.layer.borderWidth = GetPixelLength();
    _textFieldStackHolder.clipsToBounds = YES;
    _textFieldStackHolder.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];
    _textFieldStackHolder.translatesAutoresizingMaskIntoConstraints = NO;

    // Add text field configurations.
    UIStackView* fieldStack = [[UIStackView alloc] init];
    fieldStack.axis = UILayoutConstraintAxisVertical;
    fieldStack.translatesAutoresizingMaskIntoConstraints = NO;
    fieldStack.spacing = kTextfieldInset;
    fieldStack.alignment = UIStackViewAlignmentCenter;
    [_textFieldStackHolder addSubview:fieldStack];
    NSDirectionalEdgeInsets fieldStackContentInsets =
        NSDirectionalEdgeInsetsMake(kTextfieldInset, 0.0, kTextfieldInset, 0.0);
    AddSameConstraintsWithInsets(fieldStack, _textFieldStackHolder,
                                 fieldStackContentInsets);
    for (UITextField* textField in self.textFields) {
      if (textField != [self.textFields firstObject]) {
        AddSeparatorToStackView(fieldStack);
      }
      [fieldStack addArrangedSubview:textField];
      NSDirectionalEdgeInsets fieldInsets = NSDirectionalEdgeInsetsMake(
          0.0, kTextfieldInset, 0.0, kTextfieldInset);
      AddSameConstraintsToSidesWithInsets(
          textField, fieldStack, LayoutSides::kTrailing | LayoutSides::kLeading,
          fieldInsets);
    }
  }
  return _textFieldStackHolder;
}

- (NSDictionary<NSNumber*, AlertAction*>*)buttonAlertActionsDictionary {
  if (!_buttonAlertActionsDictionary) {
    NSMutableDictionary<NSNumber*, AlertAction*>* buttonAlertActionsDictionary =
        [[NSMutableDictionary alloc] init];
    for (NSArray<AlertAction*>* rowOfActions in self.actions) {
      for (AlertAction* action in rowOfActions) {
        buttonAlertActionsDictionary[@(action.uniqueIdentifier)] = action;
      }
    }
    _buttonAlertActionsDictionary = buttonAlertActionsDictionary;
  }
  return _buttonAlertActionsDictionary;
}

#pragma mark - AlertConsumer

- (void)setImageLottieName:(NSString*)imageLottieName
        darkModeLottieName:(NSString*)imageDarkModeLottieName {
  _imageLottieName = [imageLottieName copy];
  _imageDarkModeLottieName = [imageDarkModeLottieName copy];
}

- (void)updateProgressViewsForCurrentState {
  if (!_shouldShowActivityIndicator || !self.isViewLoaded || !_checkmark ||
      !_spinner) {
    return;
  }
  if (_progressState == ProgressIndicatorStateActivity) {
    _checkmark.hidden = YES;
    _spinner.hidden = NO;
    [_spinner startAnimating];
  } else if (_progressState == ProgressIndicatorStateSuccess) {
    _spinner.hidden = YES;
    [_spinner stopAnimating];
    _checkmark.hidden = NO;
    _checkmark.accessibilityLabel = self.confirmationAccessibilityLabel;
    _checkmark.isAccessibilityElement =
        (self.confirmationAccessibilityLabel.length > 0);
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    _checkmark);
  } else {
    _spinner.hidden = YES;
    [_spinner stopAnimating];
    _checkmark.hidden = YES;
  }
}

- (void)setActions:(NSArray<NSArray<AlertAction*>*>*)newActions {
  if ([_actions isEqual:newActions]) {
    return;
  }

  _actions = [newActions copy];
  _buttonAlertActionsDictionary = nil;

  if (!self.isViewLoaded) {
    return;
  }

  UIStackView* mainContentStackView = [self mainContentStackView];

  if (!mainContentStackView) {
    return;
  }

  UIView* oldButtonStackContainer =
      [mainContentStackView viewWithTag:kButtonStackViewTag];
  if (oldButtonStackContainer) {
    [oldButtonStackContainer removeFromSuperview];
  }

  if (_actions.count > 0) {
    UIStackView* newButtonStackContainer = [self createButtonStackView];
    newButtonStackContainer.tag = kButtonStackViewTag;
    [mainContentStackView addArrangedSubview:newButtonStackContainer];
    AddSameConstraintsToSides(newButtonStackContainer, self.contentView,
                              (LayoutSides::kTrailing | LayoutSides::kLeading));
  }
}

- (UIStackView*)mainContentStackView {
  for (UIView* subview_content in self.contentView.subviews) {
    if (![subview_content isKindOfClass:[UIScrollView class]]) {
      continue;
    }

    UIScrollView* scrollView =
        base::apple::ObjCCastStrict<UIScrollView>(subview_content);
    for (UIView* subview_scroll in scrollView.subviews) {
      if ([subview_scroll isKindOfClass:[UIStackView class]]) {
        return base::apple::ObjCCastStrict<UIStackView>(subview_scroll);
      }
    }
  }
  return nil;
}

- (void)setProgressState:(ProgressIndicatorState)progressState {
  if (_progressState != progressState) {
    _progressState = progressState;
    [self updateProgressViewsForCurrentState];
  }
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  if (self.tapRecognizer != gestureRecognizer) {
    return YES;
  }
  CGPoint locationInContentView = [touch locationInView:self.contentView];
  return !CGRectContainsPoint(self.contentView.bounds, locationInContentView);
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidBeginEditing:(UITextField*)textField {
  self.lastFocusedTextField = textField;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  NSUInteger index = [self.textFields indexOfObject:textField];
  if (index + 1 < self.textFields.count) {
    [self.textFields[index + 1] becomeFirstResponder];
  } else {
    [textField resignFirstResponder];
  }
  return NO;
}

#pragma mark - Private

// Configures the image.
- (void)configureAnimationViewWrapper {
  self.animationViewWrapper = [self createAnimation:self.imageLottieName];
  self.animationViewWrapperDarkMode =
      [self createAnimation:self.imageDarkModeLottieName];

  // Set the text localization.
  NSDictionary* textProvider = @{
    kLockScreen : l10n_util::GetNSString(
        IDS_IOS_PROMINENCE_NOTIFICATION_SETTINGS_LOCK_SCREEN_TEXT),
    kNotificationCenter : l10n_util::GetNSString(
        IDS_IOS_PROMINENCE_NOTIFICATION_SETTINGS_NOTIFICATION_CENTER_TEXT),
    kBanners : l10n_util::GetNSString(
        IDS_IOS_PROMINENCE_NOTIFICATION_SETTINGS_BANNERS_TEXT)
  };
  [self.animationViewWrapper setDictionaryTextProvider:textProvider];
  [self.animationViewWrapperDarkMode setDictionaryTextProvider:textProvider];

  // Layout the animation view to take up the top half of the view.
  self.animationViewWrapper.animationView.contentMode =
      UIViewContentModeScaleAspectFit;
  self.animationViewWrapperDarkMode.animationView.contentMode =
      UIViewContentModeScaleAspectFit;
  self.animationViewWrapperDarkMode.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
}

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.shouldLoop = YES;
  return ios::provider::GenerateLottieAnimation(config);
}

// Selects regular or dark mode animation based on the given style.
- (void)selectImageForCurrentStyle {
  if (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
    self.animationViewWrapperDarkMode.animationView.hidden = NO;
  } else {
    self.animationViewWrapperDarkMode.animationView.hidden = YES;
  }
}

// Displays the keyboard.
- (void)handleKeyboardWillShow:(NSNotification*)notification {
  CGRect keyboardFrame =
      [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  CGRect viewFrameInWindow = [self.view convertRect:self.view.bounds
                                             toView:nil];
  CGRect intersectedFrame =
      CGRectIntersection(keyboardFrame, viewFrameInWindow);

  CGFloat additionalBottomInset =
      intersectedFrame.size.height - self.view.safeAreaInsets.bottom;

  if (additionalBottomInset > 0) {
    self.additionalSafeAreaInsets =
        UIEdgeInsetsMake(0, 0, additionalBottomInset, 0);
    [self animateLayoutFromKeyboardNotification:notification];
  }

  self.tapRecognizer.enabled = YES;
  self.swipeRecognizer.enabled = YES;
}

// Hides the keyboard.
- (void)handleKeyboardWillHide:(NSNotification*)notification {
  self.additionalSafeAreaInsets = UIEdgeInsetsZero;
  [self animateLayoutFromKeyboardNotification:notification];

  self.tapRecognizer.enabled = NO;
  self.swipeRecognizer.enabled = NO;
}

// Helper method that displays the keyboard with an animation.
- (void)animateLayoutFromKeyboardNotification:(NSNotification*)notification {
  double duration =
      [notification.userInfo[UIKeyboardAnimationDurationUserInfoKey]
          doubleValue];
  UIViewAnimationCurve animationCurve = static_cast<UIViewAnimationCurve>(
      [notification.userInfo[UIKeyboardAnimationCurveUserInfoKey]
          integerValue]);
  UIViewAnimationOptions options = animationCurve
                                   << kUIViewAnimationCurveToOptionsShift;

  [UIView animateWithDuration:duration
                        delay:0
                      options:options
                   animations:^{
                     [self.view layoutIfNeeded];
                   }
                   completion:nil];
}

// Returns a stack of formatted buttons to be added to the bottom of the alert.
- (UIStackView*)createButtonStackView {
  UIStackView* buttons = [[UIStackView alloc] init];
  buttons.axis = UILayoutConstraintAxisVertical;
  buttons.translatesAutoresizingMaskIntoConstraints = NO;
  buttons.alignment = UIStackViewAlignmentCenter;

  if (@available(iOS 26, *)) {
    buttons.spacing = kButtonStackViewSpacing;
  }

  for (NSArray<AlertAction*>* rowOfActions in self.actions) {
    DCHECK_GT([rowOfActions count], 0U);
    AddSeparatorToStackView(buttons);
    // Calculate the axis for the sub-stackview.
    CGFloat maxWidth = 0;
    NSMutableArray<UIButton*>* rowOfButtons = [[NSMutableArray alloc] init];
    for (AlertAction* action in rowOfActions) {
      UIButton* button = GetButtonForAction(action);
      if (self.actionButtonsAreInitiallyDisabled) {
        button.enabled = NO;
        [self performSelector:@selector(updateButtonEnabledState:)
                   withObject:button
                   afterDelay:kEnableActionButtonsDelay];
      } else {
        [self updateButtonEnabledState:button];
      }
      [button addTarget:self
                    action:@selector(didSelectActionForButton:)
          forControlEvents:UIControlEventTouchUpInside];
      [rowOfButtons addObject:button];
      maxWidth = MAX(maxWidth, button.intrinsicContentSize.width);
    }
    UILayoutConstraintAxis axis =
        maxWidth > GetAlertWidth() / rowOfActions.count
            ? UILayoutConstraintAxisVertical
            : UILayoutConstraintAxisHorizontal;
    // Actually creates and adds the stack view to the view, and position the
    // buttons.
    UIStackView* rowOfButtonStackView = [[UIStackView alloc] init];
    rowOfButtonStackView.axis = axis;
    rowOfButtonStackView.alignment = UIStackViewAlignmentCenter;
    UIButton* firstButton = [rowOfButtons firstObject];
    UIButton* lastButton = [rowOfButtons lastObject];
    for (UIButton* button in rowOfButtons) {
      [rowOfButtonStackView addArrangedSubview:button];
      if (button != lastButton) {
        AddSeparatorToStackView(rowOfButtonStackView);
      }
      if (axis == UILayoutConstraintAxisHorizontal) {
        [button.widthAnchor constraintEqualToAnchor:firstButton.widthAnchor]
            .active = YES;
        AddSameConstraintsToSides(button, rowOfButtonStackView,
                                  (LayoutSides::kTop | LayoutSides::kBottom));
      } else {
        AddSameConstraintsToSides(
            button, rowOfButtonStackView,
            (LayoutSides::kTrailing | LayoutSides::kLeading));
      }
    }
    [buttons addArrangedSubview:rowOfButtonStackView];
    AddSameConstraintsToSides(rowOfButtonStackView, buttons,
                              (LayoutSides::kTrailing | LayoutSides::kLeading));
  }
  return buttons;
}

// React to user taps on `button`.
- (void)didSelectActionForButton:(UIButton*)button {
  // Prevent further taps on this button.
  button.enabled = NO;
  AlertAction* action = self.buttonAlertActionsDictionary[@(button.tag)];
  if (action && action.handler) {
    action.handler(action);
  }
}

// Dismiss the keyboard, if visible.
- (void)dismissKeyboard {
  [self.lastFocusedTextField resignFirstResponder];
}

- (void)updateButtonEnabledState:(UIButton*)button {
  AlertAction* action = self.buttonAlertActionsDictionary[@(button.tag)];
  if (action) {
    button.enabled = action.enabled;
  }
}

// Updates the `textFieldStackHolder`'s border color when the view controller's
// UITraits are modified.
- (void)updateBorderColorOnTraitChange:
    (UITraitCollection*)previousTraitCollection {
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    self.textFieldStackHolder.layer.borderColor =
        [UIColor colorNamed:kSeparatorColor].CGColor;
  }
}

// Configures the contentView and add it to the view hierarchy for ios 18 or
// lower.
- (void)configureContentViewLegacy {
  self.contentView = [[UIView alloc] init];
  self.contentView.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  self.contentView.layer.shadowOffset =
      CGSizeMake(kShadowOffsetX, kShadowOffsetY);
  self.contentView.layer.shadowRadius = kShadowRadius;
  self.contentView.layer.shadowOpacity = kShadowOpacity;
  self.contentView.layer.cornerRadius = kLegacyCornerRadius;
  [self.view addSubview:self.contentView];

  self.contentView.accessibilityIdentifier = self.alertAccessibilityIdentifier;
  self.contentView.clipsToBounds = YES;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
}

// Configures the contentView and add it to the view hierarchy.
- (void)configureContentView {
  if (@available(iOS 26, *)) {
    UIGlassEffect* glassEffect = [[UIGlassEffect alloc] init];
    glassEffect.interactive = NO;
    UIVisualEffectView* backgroundView =
        [[UIVisualEffectView alloc] initWithEffect:glassEffect];
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:backgroundView];
    backgroundView.cornerConfiguration = [UICornerConfiguration
        capsuleConfigurationWithMaximumRadius:kCornerRadius];
    self.contentView = backgroundView.contentView;
    AddSameConstraints(self.contentView, backgroundView);

    self.contentView.accessibilityIdentifier =
        self.alertAccessibilityIdentifier;
    self.contentView.clipsToBounds = YES;
    self.contentView.translatesAutoresizingMaskIntoConstraints = NO;

  } else {
    return [self configureContentViewLegacy];
  }
}

@end
