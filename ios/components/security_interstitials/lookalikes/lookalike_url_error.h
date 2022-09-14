// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_ERROR_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_ERROR_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/navigation/web_state_policy_decider.h"

// The error domain for lookalike URL errors.
extern const NSErrorDomain kLookalikeUrlErrorDomain;

// Error code for navigations to lookalike URLs.
extern const NSInteger kLookalikeUrlErrorCode;

// Creates a PolicyDecision that cancels a navigation to show a lookalike
// error.
web::WebStatePolicyDecider::PolicyDecision CreateLookalikeErrorDecision();

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_ERROR_H_
