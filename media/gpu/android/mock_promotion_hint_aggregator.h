// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MOCK_PROMOTION_HINT_AGGREGATOR_H_
#define MEDIA_GPU_ANDROID_MOCK_PROMOTION_HINT_AGGREGATOR_H_

#include "media/gpu/android/promotion_hint_aggregator.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockPromotionHintAggregator
    : public testing::NiceMock<PromotionHintAggregator> {
 public:
  MockPromotionHintAggregator();
  ~MockPromotionHintAggregator();

  MOCK_METHOD1(NotifyPromotionHint, void(const Hint& hint));
  MOCK_METHOD0(IsSafeToPromote, bool());

  // Convenience function to change the return of IsSafeToPromote.
  void SetIsSafeToPromote(bool is_safe);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MOCK_PROMOTION_HINT_AGGREGATOR_H_
