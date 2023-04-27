// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"

#import <ostream>

#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

bool IsUserFeedbackSupported() {
  return false;
}

UIViewController* CreateUserFeedbackViewController(
    UserFeedbackConfiguration* configuration) {
  NOTREACHED_NORETURN() << "User feedback reporting not supported.";
}

void UploadAllPendingUserFeedback() {
  NOTREACHED_NORETURN() << "User feedback reporting not supported.";
}

bool CanUseStartUserFeedbackFlow() {
  NOTREACHED_NORETURN() << "User feedback reporting not supported.";
}

bool StartUserFeedbackFlow(UserFeedbackConfiguration* configuration,
                           UIViewController* presenting_view_controller,
                           NSError** error) {
  NOTREACHED_NORETURN() << "User feedback reporting not supported.";
}

}  // namespace provider
}  // namespace ios
