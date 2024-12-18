// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHANGE_PROFILE_CHANGE_PROFILE_SIGNOUT_CONTINUATION_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHANGE_PROFILE_CHANGE_PROFILE_SIGNOUT_CONTINUATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/change_profile/change_profile_continuation.h"

@class MDCSnackbarMessage;

namespace signin_metrics {
enum class ProfileSignout;
}  // namespace signin_metrics

@interface ChangeProfileSignoutContinuation
    : NSObject <ChangeProfileContinuation>

- (instancetype)initWithSignoutSourceMetric:
                    (signin_metrics::ProfileSignout)signoutSourceMetric
                             forceClearData:(BOOL)forceClearData
                   forceSnackbarOverToolbar:(BOOL)forceSnackbarOverToolbar
                            snackbarMessage:(MDCSnackbarMessage*)snackbarMessage
                          signoutCompletion:(ProceduralBlock)signoutCompletion;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHANGE_PROFILE_CHANGE_PROFILE_SIGNOUT_CONTINUATION_H_
