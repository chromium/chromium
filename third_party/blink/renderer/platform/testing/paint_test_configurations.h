// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_

#include <gtest/gtest.h>
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "cc/base/features.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/native_theme/native_theme_features.h"

namespace blink {

enum {
  kUnderInvalidationChecking = 1 << 0,
  kScrollUnification = 1 << 1,
  kSolidColorLayers = 1 << 2,
  kCompositeScrollAfterPaint = 1 << 3,
  kUsedColorSchemeRootScrollbars = 1 << 4,
  kFluentScrollbar = 1 << 5,
  kSparseObjectPaintProperties = 1 << 6,
};

class PaintTestConfigurations
    : public testing::WithParamInterface<unsigned>,
      private ScopedPaintUnderInvalidationCheckingForTest,
      private ScopedSolidColorLayersForTest,
      private ScopedCompositeScrollAfterPaintForTest,
      private ScopedUsedColorSchemeRootScrollbarsForTest,
      private ScopedSparseObjectPaintPropertiesForTest {
 public:
  PaintTestConfigurations()
      : ScopedPaintUnderInvalidationCheckingForTest(GetParam() &
                                                    kUnderInvalidationChecking),
        ScopedSolidColorLayersForTest(GetParam() & kSolidColorLayers),
        ScopedCompositeScrollAfterPaintForTest(GetParam() &
                                               kCompositeScrollAfterPaint),
        ScopedUsedColorSchemeRootScrollbarsForTest(
            GetParam() & kUsedColorSchemeRootScrollbars),
        ScopedSparseObjectPaintPropertiesForTest(GetParam() &
                                                 kSparseObjectPaintProperties) {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};
    if (GetParam() & kScrollUnification) {
      enabled_features.push_back(::features::kScrollUnification);
    } else {
      disabled_features.push_back(::features::kScrollUnification);
    }
    if (GetParam() & kFluentScrollbar) {
      enabled_features.push_back(::features::kFluentScrollbar);
    } else {
      disabled_features.push_back(::features::kFluentScrollbar);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~PaintTestConfigurations() override {
    // Must destruct all objects before toggling back feature flags.
    std::unique_ptr<base::test::TaskEnvironment> task_environment;
    if (!base::ThreadPoolInstance::Get()) {
      // Create a TaskEnvironment for the garbage collection below.
      task_environment = std::make_unique<base::test::TaskEnvironment>();
    }
    feature_list_.Reset();
    WebHeap::CollectAllGarbageForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Note: If a new test fails with kCompositeScrollAfterPaint, please add the
// following at the beginning of the test to skip it temporarily:
//  if (RuntimeEnabledFeatures::CompositeScrollAfterPaintEnabled()) {
//    // TODO(crbug.com/1414885): Fix this test.
//    return;
//  }
#define INSTANTIATE_PAINT_TEST_SUITE_P(test_class)                       \
  INSTANTIATE_TEST_SUITE_P(                                              \
      All, test_class,                                                   \
      ::testing::Values(0, kScrollUnification, kSolidColorLayers,        \
                        kCompositeScrollAfterPaint,                      \
                        kCompositeScrollAfterPaint | kScrollUnification, \
                        kUsedColorSchemeRootScrollbars, kFluentScrollbar))

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
