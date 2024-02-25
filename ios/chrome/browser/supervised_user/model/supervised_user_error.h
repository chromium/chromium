// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_ERROR_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_ERROR_H_

#import "ios/web/public/navigation/web_state_policy_decider.h"

// The error domain for displaying the Supervised User error page.
extern const NSErrorDomain kSupervisedUserInterstitialErrorDomain;
// Error code for  displaying the Supervised User error page. Should be unique
// per domain.
extern const NSInteger kSupervisedUserInterstitialErrorCode;

// Creates a PolicyDecision that cancels a navigation and shows a Supervised
// User interstitial page.
web::WebStatePolicyDecider::PolicyDecision
CreateSupervisedUserInterstitialErrorDecision();

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_ERROR_H_
