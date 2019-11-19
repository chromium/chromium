// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/ime_text_span.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/mojom/ime_types.mojom-blink.h"

namespace blink {
namespace {

ImeTextSpan CreateImeTextSpan(unsigned start_offset, unsigned end_offset) {
  return ImeTextSpan(ImeTextSpan::Type::kComposition, start_offset, end_offset,
                     Color::kTransparent,
                     ui::mojom::ImeTextSpanThickness::kNone,
                     Color::kTransparent);
}

TEST(ImeTextSpanTest, OneChar) {
  ImeTextSpan ime_text_span = CreateImeTextSpan(0, 1);
  EXPECT_EQ(0u, ime_text_span.StartOffset());
  EXPECT_EQ(1u, ime_text_span.EndOffset());
}

TEST(ImeTextSpanTest, MultiChar) {
  ImeTextSpan ime_text_span = CreateImeTextSpan(0, 5);
  EXPECT_EQ(0u, ime_text_span.StartOffset());
  EXPECT_EQ(5u, ime_text_span.EndOffset());
}

TEST(ImeTextSpanTest, ZeroLength) {
  ImeTextSpan ime_text_span = CreateImeTextSpan(0, 0);
  EXPECT_EQ(0u, ime_text_span.StartOffset());
  EXPECT_EQ(1u, ime_text_span.EndOffset());
}

TEST(ImeTextSpanTest, ZeroLengthNonZeroStart) {
  ImeTextSpan ime_text_span = CreateImeTextSpan(3, 3);
  EXPECT_EQ(3u, ime_text_span.StartOffset());
  EXPECT_EQ(4u, ime_text_span.EndOffset());
}

TEST(ImeTextSpanTest, EndBeforeStart) {
  ImeTextSpan ime_text_span = CreateImeTextSpan(1, 0);
  EXPECT_EQ(1u, ime_text_span.StartOffset());
  EXPECT_EQ(2u, ime_text_span.EndOffset());
}

TEST(ImeTextSpanTest, LastChar) {
  ImeTextSpan ime_text_span =
      CreateImeTextSpan(std::numeric_limits<unsigned>::max() - 1,
                        std::numeric_limits<unsigned>::max());
  EXPECT_EQ(std::numeric_limits<unsigned>::max() - 1,
            ime_text_span.StartOffset());
  EXPECT_EQ(std::numeric_limits<unsigned>::max(), ime_text_span.EndOffset());
}

TEST(ImeTextSpanTest, LastCharEndBeforeStart) {
  ImeTextSpan ime_text_span =
      CreateImeTextSpan(std::numeric_limits<unsigned>::max(),
                        std::numeric_limits<unsigned>::max() - 1);
  EXPECT_EQ(std::numeric_limits<unsigned>::max() - 1,
            ime_text_span.StartOffset());
  EXPECT_EQ(std::numeric_limits<unsigned>::max(), ime_text_span.EndOffset());
}

TEST(ImeTextSpanTest, LastCharEndBeforeStartZeroEnd) {
  ImeTextSpan ime_text_span =
      CreateImeTextSpan(std::numeric_limits<unsigned>::max(), 0);
  EXPECT_EQ(std::numeric_limits<unsigned>::max() - 1,
            ime_text_span.StartOffset());
  EXPECT_EQ(std::numeric_limits<unsigned>::max(), ime_text_span.EndOffset());
}

}  // namespace
}  // namespace blink
