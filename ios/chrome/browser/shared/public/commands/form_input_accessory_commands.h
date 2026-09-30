// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FORM_INPUT_ACCESSORY_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FORM_INPUT_ACCESSORY_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands related to the form input accessory view.
@protocol FormInputAccessoryCommands <NSObject>

// Resets the autofill suggestions loading states.
- (void)resetAutofillSuggestionsLoadingStates;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FORM_INPUT_ACCESSORY_COMMANDS_H_
