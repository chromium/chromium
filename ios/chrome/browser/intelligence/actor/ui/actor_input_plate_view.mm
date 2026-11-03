// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actor_input_plate_view.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using intelligence::actor::kSpacingLarge;
using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;

// Diameter of the send button; smaller than the plate's one-line height.
const CGFloat kSendButtonSize = 36.0;

// Point size of the send symbol.
const CGFloat kSendIconPointSize = 15.0;

// Shadow configuration of the plate.
const float kShadowOpacity = 0.1;
const CGFloat kShadowRadius = 8.0;
const CGSize kShadowOffset = {0.0, 2.0};

// Returns `text` without leading and trailing whitespace.
NSString* TrimmedText(NSString* text) {
  return [text
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
}

}  // namespace

@implementation ActorInputPlateView {
  UITextField* _textField;
  UIButton* _sendButton;
  UIStackView* _contentStack;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setupAppearance];
    [self setupSubviews];
    [self setupConstraints];
    [self updateSendButtonState];
  }
  return self;
}

#pragma mark - Properties

- (NSString*)text {
  return _textField.text;
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return [_textField canBecomeFirstResponder];
}

- (BOOL)becomeFirstResponder {
  return [_textField becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
  return [_textField resignFirstResponder];
}

- (BOOL)isFirstResponder {
  return [_textField isFirstResponder];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  CGFloat cornerRadius =
      MIN(CGRectGetHeight(self.bounds), CGRectGetWidth(self.bounds)) / 2.0;
  self.layer.cornerRadius = cornerRadius;
}

#pragma mark - Private

// Configures the plate's background and shadow.
- (void)setupAppearance {
  self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.layer.shadowColor = UIColor.blackColor.CGColor;
  self.layer.shadowOpacity = kShadowOpacity;
  self.layer.shadowRadius = kShadowRadius;
  self.layer.shadowOffset = kShadowOffset;
}

// Creates and adds the text field and send button.
- (void)setupSubviews {
  _textField = [[UITextField alloc] init];
  _textField.accessibilityIdentifier =
      kActorInputPlateTextFieldAccessibilityIdentifier;
  _textField.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _textField.adjustsFontForContentSizeCategory = YES;
  _textField.maximumContentSizeCategory =
      UIContentSizeCategoryAccessibilityLarge;
  _textField.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _textField.textAlignment = NSTextAlignmentNatural;
  _textField.placeholder =
      l10n_util::GetNSString(IDS_IOS_ACTOR_INPUT_PLATE_PLACEHOLDER);
  _textField.returnKeyType = UIReturnKeySend;
  [_textField addTarget:self
                 action:@selector(updateSendButtonState)
       forControlEvents:UIControlEventEditingChanged];
  // Fired by the keyboard's return key.
  [_textField addTarget:self
                 action:@selector(sendTextIfPossible)
       forControlEvents:UIControlEventPrimaryActionTriggered];

  UIButtonConfiguration* sendConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  sendConfiguration.image =
      SymbolTemplateWithPointSize(SymbolPaperplaneFill, kSendIconPointSize);
  sendConfiguration.baseBackgroundColor = [UIColor colorNamed:kGrey100Color];
  sendConfiguration.baseForegroundColor =
      [UIColor colorNamed:kTextPrimaryColor];
  sendConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  _sendButton = [UIButton buttonWithConfiguration:sendConfiguration
                                    primaryAction:nil];
  _sendButton.accessibilityIdentifier =
      kActorInputPlateSendButtonAccessibilityIdentifier;
  _sendButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_ACTOR_INPUT_PLATE_SEND_ACCESSIBILITY_LABEL);
  [_sendButton addTarget:self
                  action:@selector(sendTextIfPossible)
        forControlEvents:UIControlEventTouchUpInside];

  _contentStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _textField, _sendButton ]];
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  _contentStack.alignment = UIStackViewAlignmentCenter;
  _contentStack.spacing = kSpacingMedium;
  [self addSubview:_contentStack];
}

// Configures internal layout constraints.
- (void)setupConstraints {
  AddSquareConstraints(_sendButton, kSendButtonSize);
  AddSameConstraintsWithInsets(
      _contentStack, self,
      NSDirectionalEdgeInsetsMake(kSpacingSmall, kSpacingLarge, kSpacingSmall,
                                  kSpacingSmall));
}

// Enables the send button only when there is sendable text.
- (void)updateSendButtonState {
  _sendButton.enabled = TrimmedText(_textField.text).length > 0;
}

// Clears and sends the trimmed text if it is not empty.
- (void)sendTextIfPossible {
  NSString* text = TrimmedText(_textField.text);
  if (text.length == 0) {
    return;
  }
  _textField.text = nil;
  [self updateSendButtonState];
  // Notify the delegate last, as it may release the plate.
  [self.delegate actorInputPlateView:self didSendText:text];
}

@end
