// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/isolated_xr_device/xr_sandbox_hook_linux.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(XrSandboxHookLinuxTest, ParseManifest) {
  std::optional<XrRuntimeManifest> manifest = ParseXrRuntimeManifest(R"({
    "file_format_version": "1.0.0",
    "runtime": {
      "name": "Monado",
      "library_path": "/usr/lib/libopenxr_monado.so"
    }
  })");
  ASSERT_TRUE(manifest);
  EXPECT_EQ("/usr/lib/libopenxr_monado.so", manifest->library_path);
  EXPECT_EQ(XrRuntimeId::kOther, manifest->runtime_id);
}

TEST(XrSandboxHookLinuxTest, ParseSteamVrManifest) {
  std::optional<XrRuntimeManifest> manifest = ParseXrRuntimeManifest(R"({
    "file_format_version": "1.0.0",
    "runtime": {
      "VALVE_runtime_is_steamvr": true,
      "library_path": "bin/linux64/vrclient.so",
      "name": "SteamVR/OpenXR"
    }
  })");
  ASSERT_TRUE(manifest);
  EXPECT_EQ("bin/linux64/vrclient.so", manifest->library_path);
  EXPECT_EQ(XrRuntimeId::kSteamVr, manifest->runtime_id);
}

TEST(XrSandboxHookLinuxTest, SteamVrMarkerMustBeTrue) {
  std::optional<XrRuntimeManifest> manifest = ParseXrRuntimeManifest(R"({
    "runtime": {
      "VALVE_runtime_is_steamvr": false,
      "library_path": "libfoo.so"
    }
  })");
  ASSERT_TRUE(manifest);
  EXPECT_EQ(XrRuntimeId::kOther, manifest->runtime_id);

  manifest = ParseXrRuntimeManifest(R"({
    "runtime": {
      "VALVE_runtime_is_steamvr": "true",
      "library_path": "libfoo.so"
    }
  })");
  ASSERT_TRUE(manifest);
  EXPECT_EQ(XrRuntimeId::kOther, manifest->runtime_id);
}

TEST(XrSandboxHookLinuxTest, RejectsManifestWithoutLibrary) {
  EXPECT_FALSE(ParseXrRuntimeManifest("not json"));
  EXPECT_FALSE(ParseXrRuntimeManifest(R"({"file_format_version": "1.0.0"})"));
  EXPECT_FALSE(ParseXrRuntimeManifest(R"({"runtime": {"name": "x"}})"));
  EXPECT_FALSE(ParseXrRuntimeManifest(R"({"runtime": {"library_path": ""}})"));
}

}  // namespace vr
