// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_only_mode_error.h"

#import "base/logging.h"

const NSErrorDomain kHttpsOnlyModeErrorDomain =
    @"com.google.chrome.https_only_mode";
const NSInteger kHttpsOnlyModeErrorCode = -1003;

web::WebStatePolicyDecider::PolicyDecision CreateHttpsOnlyModeErrorDecision() {
  return web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
      [NSError errorWithDomain:kHttpsOnlyModeErrorDomain
                          code:kHttpsOnlyModeErrorCode
                      userInfo:nil]);
}