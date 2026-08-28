// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for FormSuggestionLabel used to locate the autofill
// suggestion label in automation.
extern NSString* const kFormSuggestionLabelAccessibilityIdentifier;

// Accessibility identifier for Open Settings action in context menu.
extern NSString* const kFormSuggestionLabelOpenSettingsAccessibilityIdentifier;

// Accessibility identifier for Edit action in context menu.
extern NSString* const kFormSuggestionLabelEditAccessibilityIdentifier;

// Accessibility identifier for View Sources action in context menu.
extern NSString* const kFormSuggestionLabelViewSourcesAccessibilityIdentifier;

// Accessibility identifier for Remove action in context menu.
extern NSString* const kFormSuggestionLabelRemoveAccessibilityIdentifier;

// Accessibility identifier for FormSuggestionView.
extern NSString* const kFormSuggestionsViewAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_CONSTANTS_H_
