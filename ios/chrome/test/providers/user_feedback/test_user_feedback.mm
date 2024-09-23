// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"

#import <ostream>

#import "base/notreached.h"

namespace ios {
namespace provider {

bool IsUserFeedbackSupported() {
  return false;
}

UIViewController* CreateUserFeedbackViewController(
    UserFeedbackConfiguration* configuration) {
  NOTREACHED() << "User feedback not supported in unit tests.";
}

void UploadAllPendingUserFeedback() {
  NOTREACHED() << "User feedback not supported in unit tests.";
}

bool CanUseStartUserFeedbackFlow() {
  // Supports user feedback flow in unit tests.
  return true;
}

bool StartUserFeedbackFlow(UserFeedbackConfiguration* configuration,
                           UIViewController* presenting_view_controller,
                           NSError** error) {
  // No-op for starting the feedback flow used to test configuration state only.
  return true;
}

}  // namespace provider
}  // namespace ios
