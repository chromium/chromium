// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_API_H_

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_configuration.h"

namespace ios {
namespace provider {

// Returns true if user feedback is supported.
bool IsUserFeedbackSupported();

// Returns a view controller to present to the user to collect their
// feedback. The information required to construct the user feedback
// and the objects used to interact with the application are passed
// via the `configuration` object.
//
// This function must only be called if `IsUserFeedbackSupported()`
// returns true.
UIViewController* CreateUserFeedbackViewController(
    UserFeedbackConfiguration* configuration);

// Returns whether the `StartUserFeedbackFlow` function is supported.
bool CanUseStartUserFeedbackFlow();

// Asks the provider to start the user feedback flow presented off of the
// provided `presenting_view_controller`. The information required to construct
// the user feedback and the objects used to interact with the application are
// passed via the `configuration` object and errors are returned in `error`.
//
// This function must only be called if `CanUseStartUserFeedbackFlow()`
// returns `true`.
bool StartUserFeedbackFlow(UserFeedbackConfiguration* configuration,
                           UIViewController* presenting_view_controller,
                           NSError** error);

// Uploads all pending user feedbacks.
//
// This function must only be called if `IsUserFeedbackSupported()`
// returns true.
void UploadAllPendingUserFeedback();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_API_H_
