// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_CLIENT_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_CLIENT_H_

#import <Foundation/Foundation.h>

@class FormSuggestion;

// Handles user interaction with a FormSuggestion.
@protocol FormSuggestionClient<NSObject>

// Called when a suggestion is selected.
- (void)didSelectSuggestion:(FormSuggestion*)suggestion;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_CLIENT_H_
