// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/media_file_checker.h"

#include <utility>

#include "base/files/file.h"
#include "build/build_config.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static void RunMediaFileChecker(const std::string& filename, bool expectation) {
  base::File file(GetTestDataFilePath(filename),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  MediaFileChecker checker(std::move(file));
  const base::TimeDelta check_time = base::Milliseconds(100);
  bool result = checker.Start(check_time);
  EXPECT_EQ(expectation, result);
}

TEST(MediaFileCheckerTest, InvalidFile) {
  RunMediaFileChecker("ten_byte_file", false);
}

TEST(MediaFileCheckerTest, Video) {
  RunMediaFileChecker("bear.ogv", true);
}

TEST(MediaFileCheckerTest, Audio) {
  RunMediaFileChecker("sfx.ogg", true);
}

TEST(MediaFileCheckerTest, MP3) {
  RunMediaFileChecker("sfx.mp3", true);
}

}  // namespace media
