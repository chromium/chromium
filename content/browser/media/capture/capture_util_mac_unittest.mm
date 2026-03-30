// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/capture_util_mac.h"

#include <map>
#include <optional>
#include <string>

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::map<pid_t, pid_t> g_parent_pids;
std::map<pid_t, std::optional<std::string>> g_bundle_ids;
std::map<content::DesktopMediaID::Id, pid_t> g_window_owners;

pid_t GetParentPidFake(pid_t pid) {
  auto it = g_parent_pids.find(pid);
  return it != g_parent_pids.end() ? it->second : 0;
}

std::optional<std::string> GetBundleIdForProcessFake(pid_t pid) {
  auto it = g_bundle_ids.find(pid);
  return it != g_bundle_ids.end() ? it->second : std::nullopt;
}

pid_t GetWindowOwnerPidFake(content::DesktopMediaID::Id window_id) {
  auto it = g_window_owners.find(window_id);
  return it != g_window_owners.end() ? it->second : 0;
}

class CaptureUtilMacTest : public testing::Test {
 protected:
  void SetUp() override {
    g_parent_pids.clear();
    g_bundle_ids.clear();
    g_window_owners.clear();

    content::SetGetParentPidForTesting(GetParentPidFake);
    content::SetGetBundleIdForProcessForTesting(GetBundleIdForProcessFake);
    content::SetGetWindowOwnerPidForTesting(GetWindowOwnerPidFake);
  }

  void TearDown() override {
    content::SetGetParentPidForTesting(nullptr);
    content::SetGetBundleIdForProcessForTesting(nullptr);
    content::SetGetWindowOwnerPidForTesting(nullptr);
  }
};

TEST_F(CaptureUtilMacTest, SingleProcess) {
  content::DesktopMediaID::Id kWindowId = 123;
  pid_t kPid = 1000;
  std::string kBundleId = "com.example.App";

  g_window_owners[kWindowId] = kPid;
  g_bundle_ids[kPid] = kBundleId;

  auto result = content::GetMainBundleIdForNativeWindowId(kWindowId);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, kBundleId);
}

TEST_F(CaptureUtilMacTest, HelperProcessMatchedPrefix) {
  content::DesktopMediaID::Id kWindowId = 123;
  pid_t kChildPid = 1001;
  pid_t kParentPid = 1000;
  std::string kParentBundleId = "com.example.App";
  std::string kChildBundleId = "com.example.App.Helper";

  g_window_owners[kWindowId] = kChildPid;
  g_bundle_ids[kChildPid] = kChildBundleId;
  g_bundle_ids[kParentPid] = kParentBundleId;
  g_parent_pids[kChildPid] = kParentPid;

  auto result = content::GetMainBundleIdForNativeWindowId(kWindowId);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, kParentBundleId);
}

TEST_F(CaptureUtilMacTest, PrefixWithoutDot_DoesNotMatch) {
  content::DesktopMediaID::Id kWindowId = 123;
  pid_t kChildPid = 1001;
  pid_t kParentPid = 1000;
  std::string kParentBundleId = "com.example.App";
  std::string kChildBundleId = "com.example.AppHelper";  // No dot

  g_window_owners[kWindowId] = kChildPid;
  g_bundle_ids[kChildPid] = kChildBundleId;
  g_bundle_ids[kParentPid] = kParentBundleId;
  g_parent_pids[kChildPid] = kParentPid;

  auto result = content::GetMainBundleIdForNativeWindowId(kWindowId);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, kChildBundleId);  // Stops at child
}

TEST_F(CaptureUtilMacTest, NonMatchingParent) {
  content::DesktopMediaID::Id kWindowId = 123;
  pid_t kChildPid = 1001;
  pid_t kParentPid = 1000;
  std::string kParentBundleId = "com.apple.Safari";
  std::string kChildBundleId = "com.example.App";

  g_window_owners[kWindowId] = kChildPid;
  g_bundle_ids[kChildPid] = kChildBundleId;
  g_bundle_ids[kParentPid] = kParentBundleId;
  g_parent_pids[kChildPid] = kParentPid;

  auto result = content::GetMainBundleIdForNativeWindowId(kWindowId);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, kChildBundleId);  // Stops at boundary
}

TEST_F(CaptureUtilMacTest, MissingBundleId) {
  content::DesktopMediaID::Id kWindowId = 123;
  pid_t kChildPid = 1001;
  pid_t kParentPid = 1000;

  g_window_owners[kWindowId] = kChildPid;
  g_parent_pids[kChildPid] = kParentPid;
  // kChildPid has NO bundle ID

  auto result = content::GetMainBundleIdForNativeWindowId(kWindowId);
  EXPECT_FALSE(result.has_value());  // Fails because child lacks bundle
}

}  // namespace
