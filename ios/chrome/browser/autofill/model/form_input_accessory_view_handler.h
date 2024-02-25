// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_INPUT_ACCESSORY_VIEW_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_INPUT_ACCESSORY_VIEW_HANDLER_H_

#import "ios/chrome/browser/autofill/model/form_input_navigator.h"

namespace web {
class WebState;
}  // namespace web

// This handles user actions in the default keyboard accessory view buttons.
@interface FormInputAccessoryViewHandler : NSObject <FormInputNavigator>

// The WebState for interacting with the underlying form.
@property(nonatomic) web::WebState* webState;

// Sets the frameId of the frame containing the form with the latest focus.
- (void)setLastFocusFormActivityWebFrameID:(NSString*)frameID;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_INPUT_ACCESSORY_VIEW_HANDLER_H_
