// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_PUBLIC_AUTOFILL_SUGGESTION_CONTEXT_MENU_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_PUBLIC_AUTOFILL_SUGGESTION_CONTEXT_MENU_HANDLER_H_

#import <Foundation/Foundation.h>

@class FormSuggestion;

// Protocol to handle context menu actions on Autofill suggestions.
@protocol AutofillSuggestionContextMenuHandler <NSObject>

// Invoked when the user selects to open settings page from the suggestion.
- (void)openSettingsForSuggestion:(FormSuggestion*)suggestion;

// Invoked when the user selects to edit the suggestion.
- (void)openEditForSuggestion:(FormSuggestion*)suggestion;

// Returns whether the suggestion is from Autofill AI personal context.
- (BOOL)isPersonalContextSuggestion:(FormSuggestion*)suggestion;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_PUBLIC_AUTOFILL_SUGGESTION_CONTEXT_MENU_HANDLER_H_
