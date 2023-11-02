// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_palette.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FontPaletteTest, HashingAndComparison) {
  scoped_refptr<FontPalette> a = FontPalette::Create();

  scoped_refptr<FontPalette> b =
      FontPalette::Create(FontPalette::kLightPalette);
  EXPECT_NE(a->GetHash(), b->GetHash());
  EXPECT_NE(a, b);

  b = FontPalette::Create(FontPalette::kDarkPalette);
  EXPECT_NE(a->GetHash(), b->GetHash());
  EXPECT_NE(a, b);

  b = FontPalette::Create(AtomicString("SomePaletteReference"));
  EXPECT_NE(a->GetHash(), b->GetHash());
  EXPECT_NE(a, b);
}

}  // namespace blink
