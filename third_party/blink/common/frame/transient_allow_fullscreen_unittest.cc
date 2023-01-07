// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/transient_allow_fullscreen.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using TransientAllowFullscreenTest = testing::Test;

// A test of basic functionality.
TEST_F(TransientAllowFullscreenTest, Basic) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // By default, the object is not active.
  TransientAllowFullscreen transient_allow_fullscreen;
  EXPECT_FALSE(transient_allow_fullscreen.IsActive());

  // Activation works as expected.
  transient_allow_fullscreen.Activate();
  EXPECT_TRUE(transient_allow_fullscreen.IsActive());

  // Test the activation state immediately before expiration.
  const base::TimeDelta kEpsilon = base::Milliseconds(10);
  task_environment.FastForwardBy(TransientAllowFullscreen::kActivationLifespan -
                                 kEpsilon);
  EXPECT_TRUE(transient_allow_fullscreen.IsActive());

  // Test the activation state immediately after expiration.
  task_environment.FastForwardBy(2 * kEpsilon);
  EXPECT_FALSE(transient_allow_fullscreen.IsActive());

  // Repeated activation works as expected.
  transient_allow_fullscreen.Activate();
  EXPECT_TRUE(transient_allow_fullscreen.IsActive());
  task_environment.FastForwardBy(TransientAllowFullscreen::kActivationLifespan +
                                 kEpsilon);
  EXPECT_FALSE(transient_allow_fullscreen.IsActive());
}

}  // namespace blink
