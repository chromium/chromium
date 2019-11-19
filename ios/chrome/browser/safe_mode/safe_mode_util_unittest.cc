// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mach-o/dyld.h>
#include <stddef.h>
#include <string.h>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "ios/chrome/browser/safe_mode/safe_mode_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using std::string;
using std::vector;

namespace {

typedef PlatformTest SafeModeUtilTest;

TEST_F(SafeModeUtilTest, GetAllImages) {
  vector<string> images = safe_mode_util::GetLoadedImages(nullptr);
  // There should be loaded images.
  EXPECT_GT(images.size(), 0U);

  // The libSystem dylib should always be present.
  bool found_lib_system = false;
  string lib_system_prefix("libSystem");
  for (size_t i = 0; i < images.size(); ++i) {
    string base_name = base::FilePath(images[i]).BaseName().value();
    if (base::StartsWith(base_name, lib_system_prefix,
                         base::CompareCase::SENSITIVE)) {
      found_lib_system = true;
      break;
    }
  }
  EXPECT_TRUE(found_lib_system);
}

// Tests that safe_mode_util::GetLoadedImages filters by path.
TEST_F(SafeModeUtilTest, GetSomeImages) {
  vector<string> all_images = safe_mode_util::GetLoadedImages(nullptr);
  ASSERT_GT(all_images.size(), 0U);

  // Use the path of the first item to test filtering.
  string dir_name = base::FilePath(all_images[0]).DirName().value();

  vector<string> filtered_images =
      safe_mode_util::GetLoadedImages(dir_name.c_str());
  EXPECT_GT(filtered_images.size(), 0U);
  EXPECT_LT(filtered_images.size(), all_images.size());
}

}  // namespace
