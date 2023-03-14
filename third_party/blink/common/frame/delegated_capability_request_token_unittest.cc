// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/delegated_capability_request_token.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using DelegatedCapabilityRequestTokenTest = testing::Test;

// A test of basic functionality.
TEST_F(DelegatedCapabilityRequestTokenTest, Basic) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // By default, the object is not active.
  DelegatedCapabilityRequestToken request_token;
  EXPECT_FALSE(request_token.IsActive());

  // Activation works as expected.
  request_token.Activate();
  EXPECT_TRUE(request_token.IsActive());

  // Test the activation state immediately before expiration.
  const base::TimeDelta kEpsilon = base::Milliseconds(10);
  task_environment.FastForwardBy(
      DelegatedCapabilityRequestToken::kActivationLifespan - kEpsilon);
  EXPECT_TRUE(request_token.IsActive());

  // Test the activation state immediately after expiration.
  task_environment.FastForwardBy(2 * kEpsilon);
  EXPECT_FALSE(request_token.IsActive());

  // Repeated activation works as expected.
  request_token.Activate();
  EXPECT_TRUE(request_token.IsActive());
  task_environment.FastForwardBy(
      DelegatedCapabilityRequestToken::kActivationLifespan + kEpsilon);
  EXPECT_FALSE(request_token.IsActive());
}

}  // namespace blink
