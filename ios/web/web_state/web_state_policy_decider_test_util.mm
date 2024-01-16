// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_policy_decider_test_util.h"

namespace web {

bool RequestInfoMatch(WebStatePolicyDecider::RequestInfo expected,
                      WebStatePolicyDecider::RequestInfo got) {
  return ui::PageTransitionTypeIncludingQualifiersIs(
             got.transition_type, expected.transition_type) &&
         (got.target_frame_is_main == expected.target_frame_is_main) &&
         (got.target_frame_is_cross_origin ==
          expected.target_frame_is_cross_origin) &&
         (got.is_user_initiated == expected.is_user_initiated) &&
         (got.user_tapped_recently == expected.user_tapped_recently);
}

bool ResponseInfoMatch(WebStatePolicyDecider::ResponseInfo expected,
                       WebStatePolicyDecider::ResponseInfo got) {
  return (got.for_main_frame == expected.for_main_frame);
}

}  // namespace web
