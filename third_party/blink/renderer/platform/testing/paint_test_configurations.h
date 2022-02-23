// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_

#include <gtest/gtest.h>
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

enum { kUnderInvalidationChecking = 1 << 0 };

class PaintTestConfigurations
    : public testing::WithParamInterface<unsigned>,
      private ScopedPaintUnderInvalidationCheckingForTest {
 public:
  PaintTestConfigurations()
      : ScopedPaintUnderInvalidationCheckingForTest(
            GetParam() & kUnderInvalidationChecking) {}
  ~PaintTestConfigurations() {
    // Must destruct all objects before toggling back feature flags.
    std::unique_ptr<base::test::TaskEnvironment> task_environment;
    if (!base::ThreadPoolInstance::Get()) {
      // Create a TaskEnvironment for the garbage collection below.
      task_environment = std::make_unique<base::test::TaskEnvironment>();
    }
    WebHeap::CollectAllGarbageForTesting();
  }
};

// For now this has only one configuration, but can be extended in the future
// to include more configurations.
#define INSTANTIATE_PAINT_TEST_SUITE_P(test_class) \
  INSTANTIATE_TEST_SUITE_P(All, test_class, ::testing::Values(0))

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
