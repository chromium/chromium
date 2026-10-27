// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_initializer.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

#if defined(V8_USE_EXTERNAL_STARTUP_DATA) && BUILDFLAG(IS_WIN)

class V8InitializerTest : public testing::Test {
 protected:
  void SetUp() override { V8Initializer::ResetV8FilePathForTesting(); }
  void TearDown() override { V8Initializer::ResetV8FilePathForTesting(); }
};

TEST_F(V8InitializerTest, GetV8FilePathPrefersModuleDirWhenPresent) {
  base::ScopedPathOverride assets_override(base::DIR_ASSETS);
  base::ScopedPathOverride module_override(base::DIR_MODULE);

  base::FilePath module_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &module_dir));
  const base::FilePath module_snapshot =
      module_dir.AppendASCII("snapshot_blob.bin");
  ASSERT_TRUE(base::WriteFile(module_snapshot, "test"));

  EXPECT_EQ(V8Initializer::GetV8FilePathForTesting("snapshot_blob.bin"),
            module_snapshot);
}

TEST_F(V8InitializerTest,
       GetV8FilePathFallsBackToAssetsDirWhenMissingInModuleDir) {
  base::ScopedPathOverride assets_override(base::DIR_ASSETS);
  base::ScopedPathOverride module_override(base::DIR_MODULE);

  base::FilePath assets_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &assets_dir));

  EXPECT_EQ(V8Initializer::GetV8FilePathForTesting("snapshot_blob.bin"),
            assets_dir.AppendASCII("snapshot_blob.bin"));
}

TEST_F(V8InitializerTest, GetV8FilePathCachesDirectorySelectionUntilReset) {
  base::ScopedPathOverride assets_override(base::DIR_ASSETS);
  base::ScopedPathOverride module_override(base::DIR_MODULE);

  base::FilePath assets_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &assets_dir));
  base::FilePath module_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &module_dir));

  // First call without a snapshot in DIR_MODULE caches the DIR_ASSETS choice.
  EXPECT_EQ(V8Initializer::GetV8FilePathForTesting("snapshot_blob.bin"),
            assets_dir.AppendASCII("snapshot_blob.bin"));

  // Writing a snapshot into DIR_MODULE afterwards does not change the cached
  // directory until ResetV8FilePathForTesting() is called.
  const base::FilePath module_snapshot =
      module_dir.AppendASCII("snapshot_blob.bin");
  ASSERT_TRUE(base::WriteFile(module_snapshot, "test"));
  EXPECT_EQ(V8Initializer::GetV8FilePathForTesting("snapshot_blob.bin"),
            assets_dir.AppendASCII("snapshot_blob.bin"));

  V8Initializer::ResetV8FilePathForTesting();
  EXPECT_EQ(V8Initializer::GetV8FilePathForTesting("snapshot_blob.bin"),
            module_snapshot);
}

#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA) && BUILDFLAG(IS_WIN)

}  // namespace gin
