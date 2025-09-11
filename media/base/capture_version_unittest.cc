// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/capture_version.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(CaptureVersionTest, ToString) {
  EXPECT_EQ(media::CaptureVersion().ToString(), "{.sub_capture = 0}");
  EXPECT_EQ(media::CaptureVersion(22).ToString(), "{.sub_capture = 22}");
}

// TODO(crbug.com/394794490): Once `source` is added, also add tests verifying
// that `source` is the major element and `sub_capture` the minor element
// with respect to operator<=>.

}  // namespace
