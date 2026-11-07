// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/coreml_initializer.h"

#import <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_path_override.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn {
namespace {

// Differs from BaseBundleID() + ".helper", as in channel-customized builds.
constexpr char kGpuHelperBundleId[] = "org.chromium.webnn.test.helper";

TEST(CoreMLInitializerTest, CreatesCacheDirUnderGpuHelperBundleId) {
  base::ScopedPathOverride cache_dir_override(base::DIR_CACHE);
  base::FilePath cache_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_CACHE, &cache_dir));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath helper_app = temp_dir.GetPath().Append("Test Helper.app");
  base::FilePath contents_dir = helper_app.Append("Contents");
  ASSERT_TRUE(base::CreateDirectory(contents_dir));
  NSDictionary* info_plist = @{@"CFBundleIdentifier" : @(kGpuHelperBundleId)};
  ASSERT_TRUE([info_plist
      writeToURL:base::apple::FilePathToNSURL(contents_dir.Append("Info.plist"))
           error:nil]);

  SetupCacheDirForTesting(helper_app);

  EXPECT_TRUE(
      base::DirectoryExists(cache_dir.Append(kGpuHelperBundleId)
                                .Append("com.apple.e5rt.e5bundlecache")));
  EXPECT_FALSE(base::PathExists(cache_dir.Append(
      base::StrCat({base::apple::BaseBundleID(), ".helper"}))));
}

TEST(CoreMLInitializerTest, NoCacheDirWithoutGpuHelperBundleId) {
  base::ScopedPathOverride cache_dir_override(base::DIR_CACHE);
  base::FilePath cache_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_CACHE, &cache_dir));
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  SetupCacheDirForTesting(base::FilePath());
  SetupCacheDirForTesting(temp_dir.GetPath().Append("Missing.app"));

  EXPECT_TRUE(base::IsDirectoryEmpty(cache_dir));
}

}  // namespace
}  // namespace webnn
