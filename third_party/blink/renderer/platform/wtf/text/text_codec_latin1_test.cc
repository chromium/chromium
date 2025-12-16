// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec_latin1.h"

#include <array>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// crbug.com/468458388
TEST(TextCodecLatin1Test, DecodeNonAsciiAfterWord) {
  // 0x92 in windows-1252 corresponds to U+2019 (RIGHT SINGLE QUOTATION MARK).
  constexpr std::array<uint8_t, 11> kInputBytes = {42, 42, 42,   42, 42, 42,
                                                   42, 42, 0x92, 42, 42};
  const String expected_string(u"********\u2019**");

  // Create a TextCodecLatin1 instance.
  auto codec = NewTextCodec(TextEncoding("windows-1252"));
  bool saw_error = false;
  String result =
      codec->Decode(base::span(kInputBytes), FlushBehavior::kDoNotFlush,
                    /* stop_on_error */ false, saw_error);

  EXPECT_FALSE(saw_error);
  EXPECT_EQ(expected_string, result);
  EXPECT_FALSE(result.Is8Bit());
}

}  // namespace blink
