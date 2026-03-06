// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_IOS_EXTENDED_TEXT_INPUT_TRAITS_H_
#define CONTENT_BROWSER_RENDERER_HOST_IOS_EXTENDED_TEXT_INPUT_TRAITS_H_

#import <BrowserEngineKit/BrowserEngineKit.h>
#import <UIKit/UIKit.h>

#include "ui/base/ime/mojom/text_input_state.mojom-forward.h"

@interface IOSExtendedTextInputTraits : NSObject <BEExtendedTextInputTraits>
@property(nonatomic) UITextAutocapitalizationType autocapitalizationType;
@property(nonatomic) UITextAutocorrectionType autocorrectionType;
@property(nonatomic) UITextSpellCheckingType spellCheckingType;
@property(nonatomic) UITextSmartQuotesType smartQuotesType;
@property(nonatomic) UITextSmartDashesType smartDashesType;
@property(nonatomic) UITextInlinePredictionType inlinePredictionType;
@property(nonatomic) UIKeyboardType keyboardType;
@property(nonatomic) UIKeyboardAppearance keyboardAppearance;
@property(nonatomic) UIReturnKeyType returnKeyType;
@property(nonatomic, getter=isSecureTextEntry) BOOL secureTextEntry;
@property(nonatomic, getter=isSingleLineDocument) BOOL singleLineDocument;
@property(nonatomic, getter=isTypingAdaptationEnabled)
    BOOL typingAdaptationEnabled;
@property(nonatomic, copy) UITextContentType textContentType;
@property(nonatomic, copy) UITextInputPasswordRules* passwordRules;
@property(nonatomic) UITextSmartInsertDeleteType smartInsertDeleteType;
@property(nonatomic) BOOL enablesReturnKeyAutomatically;
@property(nonatomic, strong) UIColor* insertionPointColor;
@property(nonatomic, strong) UIColor* selectionHandleColor;
@property(nonatomic, strong) UIColor* selectionHighlightColor;

- (BOOL)updateFromTextInputState:(const ui::mojom::TextInputState&)state;

@end

#endif  // CONTENT_BROWSER_RENDERER_HOST_IOS_EXTENDED_TEXT_INPUT_TRAITS_H_
