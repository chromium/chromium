// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_error.h"

const NSErrorDomain kSupervisedUserInterstitialErrorDomain =
    @"com.google.chrome.supervised_user.show_intestitial";

const NSInteger kSupervisedUserInterstitialErrorCode = -1000;

web::WebStatePolicyDecider::PolicyDecision
CreateSupervisedUserInterstitialErrorDecision() {
  return web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
      [NSError errorWithDomain:kSupervisedUserInterstitialErrorDomain
                          code:kSupervisedUserInterstitialErrorCode
                      userInfo:nil]);
}
