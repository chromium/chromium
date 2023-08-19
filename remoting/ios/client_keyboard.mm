// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/client_keyboard.h"

#include "remoting/client/input/keycode_map.h"

// TODO(nicholss): Look into inputAccessoryView to get the top bar for sending
// special keys.
// TODO(nicholss): Look into inputView - The custom input view to display when
// the receiver becomes the first responder

@interface ClientKeyboard () {
  UIView* _inputView;
}
@end

@implementation ClientKeyboard

@synthesize autocapitalizationType = _autocapitalizationType;
@synthesize autocorrectionType = _autocorrectionType;
@synthesize keyboardAppearance = _keyboardAppearance;
@synthesize keyboardType = _keyboardType;
@synthesize spellCheckingType = _spellCheckingType;

@synthesize selectedTextRange = _selectedTextRange;
@synthesize delegate = _delegate;

// TODO(nicholss): For physical keyboard, look at UIKeyCommand
// https://developer.apple.com/reference/uikit/uikeycommand?language=objc

- (instancetype)init {
  self = [super init];
  if (self) {
    _autocapitalizationType = UITextAutocapitalizationTypeNone;
    _autocorrectionType = UITextAutocorrectionTypeNo;
    _keyboardAppearance = UIKeyboardAppearanceDefault;
    _keyboardType = UIKeyboardTypeASCIICapable;
    _spellCheckingType = UITextSpellCheckingTypeNo;

    self.showsSoftKeyboard = NO;
  }
  return self;
}

#pragma mark - UIKeyInput

- (void)insertText:(NSString*)text {
  if (text.length == 1) {
    // TODO(yuweih): KeyboardLayout should be configurable.
    remoting::KeypressInfo keypress =
        remoting::KeypressFromUnicode([text characterAtIndex:0]);
    if (keypress.dom_code != ui::DomCode::NONE) {
      [_delegate clientKeyboardShouldSendKey:keypress];
      return;
    }
  }
  // Fallback to text injection.
  [_delegate clientKeyboardShouldSend:text];
}

- (void)deleteBackward {
  [_delegate clientKeyboardShouldDelete];
}

- (BOOL)hasText {
  return NO;
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (BOOL)resignFirstResponder {
  if (self.showsSoftKeyboard) {
    // This translates the action of resigning first responder when the keyboard
    // is showing into hiding the soft keyboard while keeping the view first
    // responder. This is to allow the hide keyboard button on the soft keyboard
    // to work properly with ClientKeyboard's soft keyboard logic, which calls
    // resignFirstResponder.
    // This may cause weird behavior if the superview has multiple responders
    // (text views).
    self.showsSoftKeyboard = NO;
    return NO;
  }
  return [super resignFirstResponder];
}

- (UIView*)inputAccessoryView {
  return nil;
}

- (UIView*)inputView {
  return _inputView;
}

#pragma mark - UITextInputTraits

#pragma mark - Properties

- (void)setShowsSoftKeyboard:(BOOL)showsSoftKeyboard {
  if (self.showsSoftKeyboard == showsSoftKeyboard) {
    return;
  }

  // Returning nil for inputView will fallback to the system soft keyboard.
  // Returning an empty view will effectively hide it.
  _inputView =
      showsSoftKeyboard ? nil : [[UIView alloc] initWithFrame:CGRectZero];

  [self reloadInputViews];
}

- (BOOL)showsSoftKeyboard {
  return _inputView == nil;
}

@end
