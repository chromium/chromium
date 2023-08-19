// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_error.h"

const NSErrorDomain kLookalikeUrlErrorDomain =
    @"com.google.chrome.lookalike_url";
const NSInteger kLookalikeUrlErrorCode = -1003;

web::WebStatePolicyDecider::PolicyDecision CreateLookalikeErrorDecision() {
  return web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
      [NSError errorWithDomain:kLookalikeUrlErrorDomain
                          code:kLookalikeUrlErrorCode
                      userInfo:nil]);
}
