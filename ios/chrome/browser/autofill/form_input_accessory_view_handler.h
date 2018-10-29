// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_HANDLER_H_

#import "ios/chrome/browser/autofill/form_input_accessory_view_delegate.h"

@class JsSuggestionManager;

// This handles user actions in the default keyboard accessory view buttons.
@interface FormInputAccessoryViewHandler
    : NSObject<FormInputAccessoryViewDelegate>

// The JS manager for interacting with the underlying form.
@property(nonatomic, weak) JsSuggestionManager* JSSuggestionManager;

// Resets the metrics logger of the instance.
- (void)reset;

// Sets the frameId of the frame containing the form with the latest focus.
- (void)setLastFocusFormActivityWebFrameID:(NSString*)frameID;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_HANDLER_H_
