// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/alert_view/ui_bundled/alert_view_controller.h"

#import <ostream>

#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/gray_highlight_button.h"
#import "ios/chrome/browser/shared/ui/elements/text_field_configuration.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Properties of the alert shadow.
constexpr CGFloat kShadowOffsetX = 0;
constexpr CGFloat kShadowOffsetY = 15;
constexpr CGFloat kShadowRadius = 13;
constexpr float kShadowOpacity = 0.12;

// Properties of the alert view.
constexpr CGFloat kCornerRadius = 14;
constexpr CGFloat kAlertWidth = 270;
constexpr CGFloat kAlertWidthAccessibility = 402;
constexpr CGFloat kTextFieldCornerRadius = 5;
constexpr CGFloat kMinimumHeight = 30;
constexpr CGFloat kMinimumMargin = 4;

// Inset at the top of the alert. Is always present.
constexpr CGFloat kAlertMarginTop = 22;
// Space before the actions and everything else.
constexpr CGFloat kAlertActionsSpacing = 12;

// Insets for the content in the alert view.
constexpr CGFloat kTitleInsetLeading = 20;
constexpr CGFloat kTitleInsetBottom = 9;
constexpr CGFloat kTitleInsetTrailing = 20;

constexpr CGFloat kSpinnerInsetTop = 12;
constexpr CGFloat kSpinnerInsetBottom = 14;

constexpr CGFloat kMessageInsetLeading = 20;
constexpr CGFloat kMessageInsetBottom = 6;
constexpr CGFloat kMessageInsetTrailing = 20;

constexpr CGFloat kButtonInsetTop = 13;
constexpr CGFloat kButtonInsetLeading = 20;
constexpr CGFloat kButtonInsetBottom = 13;
constexpr CGFloat kButtonInsetTrailing = 20;

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

// Returns a GrayHighlightButton to be added to the alert for `action`.
GrayHighlightButton* GetButtonForAction(AlertAction* action) {
  UIFont* font = nil;
  UIColor* textColor = nil;
  if (action.style == UIAlertActionStyleDefault) {
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    textColor = [UIColor colorNamed:kBlueColor];
  } else if (action.style == UIAlertActionStyleCancel) {
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    textColor = [UIColor colorNamed:kBlueColor];
  } else {  // Style is UIAlertActionStyleDestructive
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    textColor = [UIColor colorNamed:kRedColor];
  }

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets =
      NSDirectionalEdgeInsetsMake(kButtonInsetTop, kButtonInsetLeading,
                                  kButtonInsetBottom, kButtonInsetTrailing);
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSAttributedString* title =
      [[NSAttributedString alloc] initWithString:action.title
                                      attributes:attributes];
  buttonConfiguration.attributedTitle = title;
  buttonConfiguration.baseForegroundColor = textColor;
  GrayHighlightButton* button =
      [GrayHighlightButton buttonWithConfiguration:buttonConfiguration
                                     primaryAction:nil];

  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.tag = action.uniqueIdentifier;
  return button;
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

@end

@implementation AlertViewController

#pragma mark - Public

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    self.textFieldStackHolder.layer.borderColor =
        [UIColor colorNamed:kSeparatorColor].CGColor;
  }
}

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

  self.contentView = [[UIView alloc] init];
  self.contentView.accessibilityIdentifier = self.alertAccessibilityIdentifier;
  self.contentView.clipsToBounds = YES;
  self.contentView.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  self.contentView.layer.cornerRadius = kCornerRadius;
  self.contentView.layer.shadowOffset =
      CGSizeMake(kShadowOffsetX, kShadowOffsetY);
  self.contentView.layer.shadowRadius = kShadowRadius;
  self.contentView.layer.shadowOpacity = kShadowOpacity;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.contentView];

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
  AddSameConstraintsWithInsets(stackView, scrollView, stackViewInsets);

  if (self.title.length) {
    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.numberOfLines = 0;
    titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    titleLabel.adjustsFontForContentSizeCategory = YES;
    titleLabel.textAlignment = NSTextAlignmentCenter;
    titleLabel.text = self.title;
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:titleLabel];
    [stackView setCustomSpacing:self.shouldShowActivityIndicator
                                    ? kTitleInsetBottom + kSpinnerInsetTop
                                    : kTitleInsetBottom
                      afterView:titleLabel];

    NSDirectionalEdgeInsets titleInsets = NSDirectionalEdgeInsetsMake(
        0, kTitleInsetLeading, 0, kTitleInsetTrailing);
    AddSameConstraintsToSidesWithInsets(
        titleLabel, self.contentView,
        LayoutSides::kTrailing | LayoutSides::kLeading, titleInsets);
  }

  if (self.shouldShowActivityIndicator) {
    UIActivityIndicatorView* spinner = GetLargeUIActivityIndicatorView();
    [spinner startAnimating];
    [stackView addArrangedSubview:spinner];
    [stackView setCustomSpacing:kSpinnerInsetBottom afterView:spinner];
  }

  if (self.message.length) {
    UILabel* messageLabel = [[UILabel alloc] init];
    messageLabel.numberOfLines = 0;
    messageLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    messageLabel.adjustsFontForContentSizeCategory = YES;
    messageLabel.textAlignment = NSTextAlignmentCenter;
    messageLabel.text = self.message;
    messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:messageLabel];
    [stackView setCustomSpacing:kMessageInsetBottom afterView:messageLabel];

    NSDirectionalEdgeInsets messageInsets = NSDirectionalEdgeInsetsMake(
        0, kMessageInsetLeading, 0, kMessageInsetTrailing);
    AddSameConstraintsToSidesWithInsets(
        messageLabel, self.contentView,
        LayoutSides::kTrailing | LayoutSides::kLeading, messageInsets);
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
  if (lastArrangedView) {
    [stackView setCustomSpacing:kAlertActionsSpacing
                      afterView:lastArrangedView];
  }

  if ([self.actions count] > 0) {
    UIStackView* buttonStackView = [self createButtonStackView];
    [stackView addArrangedSubview:buttonStackView];
    AddSameConstraintsToSides(buttonStackView, self.contentView,
                              LayoutSides::kTrailing | LayoutSides::kLeading);
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
  for (NSArray<AlertAction*>* rowOfActions in self.actions) {
    DCHECK_GT([rowOfActions count], 0U);
    AddSeparatorToStackView(buttons);
    // Calculate the axis for the sub-stackview.
    CGFloat maxWidth = 0;
    NSMutableArray<GrayHighlightButton*>* rowOfButtons =
        [[NSMutableArray alloc] init];
    for (AlertAction* action in rowOfActions) {
      GrayHighlightButton* button = GetButtonForAction(action);
      if (self.actionButtonsAreInitiallyDisabled) {
        button.enabled = NO;
        [self performSelector:@selector(enableActionButton:)
                   withObject:button
                   afterDelay:kEnableActionButtonsDelay];
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
    GrayHighlightButton* firstButton = [rowOfButtons firstObject];
    GrayHighlightButton* lastButton = [rowOfButtons lastObject];
    for (GrayHighlightButton* button in rowOfButtons) {
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
  AlertAction* action = self.buttonAlertActionsDictionary[@(button.tag)];
  if (action.handler) {
    action.handler(action);
  }
}

// Dismiss the keyboard, if visible.
- (void)dismissKeyboard {
  [self.lastFocusedTextField resignFirstResponder];
}

// Enables `button`.
- (void)enableActionButton:(UIButton*)actionButton {
  actionButton.enabled = YES;
}

@end
