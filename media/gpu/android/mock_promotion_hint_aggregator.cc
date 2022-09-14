// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/mock_promotion_hint_aggregator.h"

using testing::_;
using testing::Return;

namespace media {

MockPromotionHintAggregator::MockPromotionHintAggregator() {
  SetIsSafeToPromote(false);
}

MockPromotionHintAggregator::~MockPromotionHintAggregator() {}

void MockPromotionHintAggregator::SetIsSafeToPromote(bool is_safe) {
  ON_CALL(*this, IsSafeToPromote()).WillByDefault(Return(is_safe));
}

}  // namespace media
