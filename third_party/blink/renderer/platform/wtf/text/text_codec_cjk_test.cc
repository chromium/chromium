// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec_cjk.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace {

TEST(TextCodecCJK, IsSupported) {
  EXPECT_TRUE(TextCodecCJK::IsSupported("EUC-JP"));
  EXPECT_TRUE(TextCodecCJK::IsSupported("Shift_JIS"));
  EXPECT_TRUE(TextCodecCJK::IsSupported("EUC-KR"));
  EXPECT_TRUE(TextCodecCJK::IsSupported("ISO-2022-JP"));
  EXPECT_TRUE(TextCodecCJK::IsSupported("GBK"));
  EXPECT_TRUE(TextCodecCJK::IsSupported("gb18030"));
  EXPECT_FALSE(TextCodecCJK::IsSupported("non-exist-encoding"));
}

}  // namespace
}  // namespace WTF
