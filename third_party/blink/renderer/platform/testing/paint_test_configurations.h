// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_

#include <gtest/gtest.h>
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

enum {
  kCompositeAfterPaint = 1 << 0,
  kUnderInvalidationChecking = 1 << 1,
  kFastBorderRadius = 1 << 2,
  kDoNotCompositeTrivial3D = 1 << 3,
};

class PaintTestConfigurations
    : public testing::WithParamInterface<unsigned>,
      private ScopedCompositeAfterPaintForTest,
      private ScopedPaintUnderInvalidationCheckingForTest,
      private ScopedFastBorderRadiusForTest {
 public:
  PaintTestConfigurations()
      : ScopedCompositeAfterPaintForTest(GetParam() & kCompositeAfterPaint),
        ScopedPaintUnderInvalidationCheckingForTest(GetParam() &
                                                    kUnderInvalidationChecking),
        ScopedFastBorderRadiusForTest(GetParam() & kFastBorderRadius) {}
  ~PaintTestConfigurations() {
    // Must destruct all objects before toggling back feature flags.
    WebHeap::CollectAllGarbageForTesting();
  }
};

#define INSTANTIATE_PAINT_TEST_SUITE_P(test_class) \
  INSTANTIATE_TEST_SUITE_P(All, test_class,        \
                           ::testing::Values(0, kCompositeAfterPaint))

#define INSTANTIATE_CAP_TEST_SUITE_P(test_class) \
  INSTANTIATE_TEST_SUITE_P(All, test_class,      \
                           ::testing::Values(kCompositeAfterPaint))

#define INSTANTIATE_LAYER_LIST_TEST_SUITE_P(test_class) \
  INSTANTIATE_TEST_SUITE_P(                             \
      All, test_class,                                  \
      ::testing::Values(0, kCompositeAfterPaint, kFastBorderRadius))

#define INSTANTIATE_SCROLL_HIT_TEST_SUITE_P(test_class) \
  INSTANTIATE_TEST_SUITE_P(All, test_class,             \
                           ::testing::Values(0, kCompositeAfterPaint))

#define INSTANTIATE_DO_NOT_COMPOSITE_TRIVIAL_3D_P(test_class)              \
  INSTANTIATE_TEST_SUITE_P(                                                \
      All, test_class,                                                     \
      ::testing::Values(0, kCompositeAfterPaint, kDoNotCompositeTrivial3D, \
                        kCompositeAfterPaint | kDoNotCompositeTrivial3D))

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
