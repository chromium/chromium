// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_POLICY_DECIDER_TEST_UTIL_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_POLICY_DECIDER_TEST_UTIL_H_

#include "ios/web/public/navigation/web_state_policy_decider.h"

namespace web {

// Compares two web::WebStatePolicyDecider::RequestInfo. Used to implement
// Google Mock matcher for tests. This is needed because operator== is not
// implemented for web::WebStatePolicyDecider::RequestInfo.
bool RequestInfoMatch(WebStatePolicyDecider::RequestInfo expected,
                      WebStatePolicyDecider::RequestInfo got);

// Compares two web::WebStatePolicyDecider::ResponseInfo. Used to implement
// Google Mock matcher for tests. This is needed because operator== is not
// implemented for web::WebStatePolicyDecider::ResponseInfo.
bool ResponseInfoMatch(WebStatePolicyDecider::ResponseInfo expected,
                       WebStatePolicyDecider::ResponseInfo got);

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_POLICY_DECIDER_TEST_UTIL_H_
