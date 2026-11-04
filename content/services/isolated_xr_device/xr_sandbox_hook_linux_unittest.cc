// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/isolated_xr_device/xr_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <gnu/libc-version.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
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

class XrSandboxHookLinuxLoaderTest : public testing::Test {
 protected:
  // libc stands in for a runtime the hook already loaded by its real path.
  static base::FilePath LibcPath() {
    Dl_info info;
    if (!dladdr(reinterpret_cast<void*>(&gnu_get_libc_version), &info)) {
      return base::FilePath();
    }
    return base::MakeAbsoluteFilePath(base::FilePath(info.dli_fname));
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    manifest_ = temp_dir_.GetPath().Append("share/openxr/1/manifest.json");
    ASSERT_TRUE(base::CreateDirectory(manifest_.DirName()));
    ASSERT_TRUE(base::WriteFile(manifest_, "{}"));
    libc_ = LibcPath();
    ASSERT_FALSE(libc_.empty());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath manifest_;
  base::FilePath libc_;
};

TEST_F(XrSandboxHookLinuxLoaderTest, AliasReachesLoadedLibrary) {
  base::FilePath lib = temp_dir_.GetPath().Append("lib");
  ASSERT_TRUE(base::CreateSymbolicLink(libc_.DirName(), lib));
  std::string library_path = "../../../lib/" + libc_.BaseName().value();
  EXPECT_TRUE(AliasRuntimeLibraryForLoader(manifest_.value(), library_path));

  // Once the path cannot be opened, as in the sandbox, the loader's dlopen()
  // still finds the library by name.
  ASSERT_TRUE(base::DeleteFile(lib));
  std::string name = base::MakeAbsoluteFilePath(manifest_).DirName().value() +
                     "/" + library_path;
  void* handle = dlopen(name.c_str(), RTLD_NOW);
  EXPECT_TRUE(handle);
  EXPECT_EQ(handle, dlopen(libc_.value().c_str(), RTLD_NOW | RTLD_NOLOAD));
}

TEST_F(XrSandboxHookLinuxLoaderTest, AliasNeverLoadsAnotherFile) {
  base::FilePath copy = temp_dir_.GetPath().Append("lib/libcopy.so");
  ASSERT_TRUE(base::CreateDirectory(copy.DirName()));
  ASSERT_TRUE(base::CopyFile(libc_, copy));
  EXPECT_FALSE(AliasRuntimeLibraryForLoader(manifest_.value(),
                                            "../../../lib/libcopy.so"));
  EXPECT_FALSE(dlopen(copy.value().c_str(), RTLD_NOW | RTLD_NOLOAD));
}

TEST_F(XrSandboxHookLinuxLoaderTest, AliasSkipsAbsoluteAndBareNames) {
  EXPECT_FALSE(AliasRuntimeLibraryForLoader(manifest_.value(), libc_.value()));
  EXPECT_FALSE(AliasRuntimeLibraryForLoader(manifest_.value(),
                                            libc_.BaseName().value()));
}

TEST(XrSandboxHookLinuxTest, RejectsManifestWithoutLibrary) {
  EXPECT_FALSE(ParseXrRuntimeManifest("not json"));
  EXPECT_FALSE(ParseXrRuntimeManifest(R"({"file_format_version": "1.0.0"})"));
  EXPECT_FALSE(ParseXrRuntimeManifest(R"({"runtime": {"name": "x"}})"));
  EXPECT_FALSE(ParseXrRuntimeManifest(R"({"runtime": {"library_path": ""}})"));
}

}  // namespace vr
