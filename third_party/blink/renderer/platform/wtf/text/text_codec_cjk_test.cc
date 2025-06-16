// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec_cjk.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(TextCodecCjk, IsSupported) {
  EXPECT_TRUE(TextCodecCjk::IsSupported("EUC-JP"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("Shift_JIS"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("EUC-KR"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("ISO-2022-JP"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("GBK"));
  EXPECT_TRUE(TextCodecCjk::IsSupported("gb18030"));
  EXPECT_FALSE(TextCodecCjk::IsSupported("non-exist-encoding"));
}

}  // namespace
}  // namespace blink
