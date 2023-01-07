// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/transient_allow_popup.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using TransientAllowPopupTest = testing::Test;

// A test of basic functionality.
TEST_F(TransientAllowPopupTest, Basic) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // By default, the object is not active.
  TransientAllowPopup transient_allow_popup;
  EXPECT_FALSE(transient_allow_popup.IsActive());

  // Activation works as expected.
  transient_allow_popup.Activate();
  EXPECT_TRUE(transient_allow_popup.IsActive());

  // Test the activation state immediately before expiration.
  const base::TimeDelta kEpsilon = base::Milliseconds(10);
  task_environment.FastForwardBy(TransientAllowPopup::kActivationLifespan -
                                 kEpsilon);
  EXPECT_TRUE(transient_allow_popup.IsActive());

  // Test the activation state immediately after expiration.
  task_environment.FastForwardBy(2 * kEpsilon);
  EXPECT_FALSE(transient_allow_popup.IsActive());

  // Repeated activation works as expected.
  transient_allow_popup.Activate();
  EXPECT_TRUE(transient_allow_popup.IsActive());

  // Deactivation works as expected.
  transient_allow_popup.Deactivate();
  EXPECT_FALSE(transient_allow_popup.IsActive());
}

}  // namespace content
