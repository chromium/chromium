// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/common/child_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(NoRendererCrashesAssertionSuspensions, SingleProcess) {
  NoRendererCrashesAssertion::Suspensions suspensions;
  EXPECT_FALSE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));

  suspensions.AddSuspension(123);
  EXPECT_TRUE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));

  suspensions.RemoveSuspension(123);
  EXPECT_FALSE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));
}

TEST(NoRendererCrashesAssertionSuspensions, AllProcesses) {
  NoRendererCrashesAssertion::Suspensions suspensions;
  EXPECT_FALSE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));

  suspensions.AddSuspension(ChildProcessHost::kInvalidUniqueID);
  EXPECT_TRUE(suspensions.IsSuspended(123));
  EXPECT_TRUE(suspensions.IsSuspended(456));

  suspensions.RemoveSuspension(ChildProcessHost::kInvalidUniqueID);
  EXPECT_FALSE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));
}

TEST(NoRendererCrashesAssertionSuspensions, SingleProcessNesting) {
  NoRendererCrashesAssertion::Suspensions suspensions;
  EXPECT_FALSE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));

  suspensions.AddSuspension(123);
  EXPECT_TRUE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));

  suspensions.AddSuspension(123);
  EXPECT_TRUE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));

  suspensions.RemoveSuspension(123);
  EXPECT_TRUE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));

  suspensions.RemoveSuspension(123);
  EXPECT_FALSE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));
}

TEST(NoRendererCrashesAssertionSuspensions, AllProcessesNesting) {
  NoRendererCrashesAssertion::Suspensions suspensions;
  EXPECT_FALSE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));

  suspensions.AddSuspension(ChildProcessHost::kInvalidUniqueID);
  EXPECT_TRUE(suspensions.IsSuspended(123));
  EXPECT_TRUE(suspensions.IsSuspended(456));

  suspensions.AddSuspension(ChildProcessHost::kInvalidUniqueID);
  EXPECT_TRUE(suspensions.IsSuspended(123));
  EXPECT_TRUE(suspensions.IsSuspended(456));

  suspensions.RemoveSuspension(ChildProcessHost::kInvalidUniqueID);
  EXPECT_TRUE(suspensions.IsSuspended(123));
  EXPECT_TRUE(suspensions.IsSuspended(456));

  suspensions.RemoveSuspension(ChildProcessHost::kInvalidUniqueID);
  EXPECT_FALSE(suspensions.IsSuspended(123));
  EXPECT_FALSE(suspensions.IsSuspended(456));
}

}  // namespace content
