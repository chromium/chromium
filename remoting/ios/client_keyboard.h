// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_CLIENT_KEYBOARD_H_
#define REMOTING_IOS_CLIENT_KEYBOARD_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace remoting {
struct KeypressInfo;
}  // namespace remoting

@protocol ClientKeyboardDelegate<NSObject>
- (void)clientKeyboardShouldSend:(NSString*)text;
- (void)clientKeyboardShouldSendKey:(const remoting::KeypressInfo&)key;
- (void)clientKeyboardShouldDelete;
@end

// A class for capturing keyboard inputs and forwarding them to the delegate.
// It should remain first responder in order to capture all key inputs. Note
// that it will hide the soft keyboard and refuse to resign first responder if
// you try to resign first responder when the soft keyboard is showing.
@interface ClientKeyboard : UIView<UIKeyInput, UITextInputTraits>

@property(nonatomic) UIKeyboardAppearance keyboardAppearance;
@property(nonatomic) UIKeyboardType keyboardType;
@property(nonatomic) UITextAutocapitalizationType autocapitalizationType;
@property(nonatomic) UITextAutocorrectionType autocorrectionType;
@property(nonatomic) UITextSpellCheckingType spellCheckingType;

// This property comes from the UITextInput protocol. It is called when the user
// taps the arrow button on the soft keyboard. This property doesn't do anything
// but is just to prevent the app from crashing. It's probably Apple's bug to
// show the arrow buttons in the first place.
// TODO(yuweih): Implement this as arrow key injection once we get the non-text
// key injection working.
// TODO(yuweih): Implement the UITextInput protocol to support multi-stage input
// methods.
@property(readwrite, copy) UITextRange* selectedTextRange;

// Set to true to show the soft keyboard. Default value is NO.
@property(nonatomic) BOOL showsSoftKeyboard;

// This delegate is used to call back to handler key entry.
@property(weak, nonatomic) id<ClientKeyboardDelegate> delegate;

@end

#endif  // REMOTING_IOS_CLIENT_KEYBOARD_H_
