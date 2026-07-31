// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/embedder_isolation_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using Mode = EmbedderIsolationInfo::Mode;

TEST(EmbedderIsolationInfoTest, Privileged) {
  EmbedderIsolationInfo info = EmbedderIsolationInfo::CreateForPrivileged(7);
  EXPECT_EQ(info.mode(), Mode::kPrivileged);
  EXPECT_TRUE(info.is_privileged());
  EXPECT_FALSE(info.is_none());
  EXPECT_FALSE(info.is_pdf());
  EXPECT_FALSE(info.is_unique_instance());
  EXPECT_EQ(info.privileged_feature_id(), 7);
  // The unique-instance accessor is empty for privileged infos.
  EXPECT_EQ(info.instance_id(), std::nullopt);
}

// Same feature id compares equal (so instances of one feature may share a
// process); different feature ids do not (so distinct features never do).
TEST(EmbedderIsolationInfoTest, PrivilegedEqualityGroupsByFeatureId) {
  EmbedderIsolationInfo a = EmbedderIsolationInfo::CreateForPrivileged(1);
  EmbedderIsolationInfo b = EmbedderIsolationInfo::CreateForPrivileged(1);
  EmbedderIsolationInfo c = EmbedderIsolationInfo::CreateForPrivileged(2);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

// Privileged is distinct from every other mode, so privileged content never
// shares a SiteInfo (hence a process) with ordinary or other embedder-
// isolated content.
TEST(EmbedderIsolationInfoTest, PrivilegedDistinctFromOtherModes) {
  EmbedderIsolationInfo privileged =
      EmbedderIsolationInfo::CreateForPrivileged(0);
  EXPECT_NE(privileged, EmbedderIsolationInfo::CreateNone());
  EXPECT_NE(privileged, EmbedderIsolationInfo::CreateForPdf());
  EXPECT_NE(privileged, EmbedderIsolationInfo::CreateForUniqueInstance(0));
}

TEST(EmbedderIsolationInfoTest, PrivilegedDebugString) {
  EXPECT_EQ(EmbedderIsolationInfo::CreateForPrivileged(3).ToDebugString(),
            "privileged(feature_id=3)");
}

}  // namespace
}  // namespace content
