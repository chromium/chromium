// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_METRICS_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_METRICS_TEST_HELPERS_H_

#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"

namespace blink {
namespace mobile_metrics_test_helpers {

class TestWebFrameClient : public frame_test_helpers::TestWebFrameClient {
 public:
  void DidChangeMobileFriendliness(const MobileFriendliness& mf) override {
    mobile_friendliness_ = mf;
  }
  const MobileFriendliness& GetMobileFriendliness() const {
    return mobile_friendliness_;
  }

 private:
  MobileFriendliness mobile_friendliness_;
};

}  // namespace mobile_metrics_test_helpers
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_METRICS_TEST_HELPERS_H_
