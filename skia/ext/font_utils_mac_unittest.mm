// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/font_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFourByteTag.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace skia {

TEST(FontUtilsMacTest, HvglFontInstantiatedWithoutStream) {
  constexpr SkFontTableTag kHvglTag = SkSetFourByteTag('h', 'v', 'g', 'l');

  // PingFang SC is Apple's standard CJK system font containing proprietary hvgl
  // tables.
  sk_sp<SkTypeface> pingfang =
      MakeTypefaceFromName("PingFang SC", SkFontStyle());
  ASSERT_TRUE(pingfang);
  ASSERT_GT(pingfang->getTableSize(kHvglTag), 0u);

  // Serialize without stream data (as done for hvgl fonts in printing
  // metafile).
  sk_sp<SkData> serialized_data =
      pingfang->serialize(SkTypeface::SerializeBehavior::kDontIncludeData);
  ASSERT_TRUE(serialized_data);

  SkMemoryStream stream(serialized_data->data(), serialized_data->size());
  sk_sp<SkTypeface> typeface = MakeTypefaceWithFontations(&stream);
  ASSERT_TRUE(typeface);

  EXPECT_GT(typeface->getTableSize(kHvglTag), 0u);

  SkString family_name;
  typeface->getFamilyName(&family_name);
  EXPECT_TRUE(family_name.contains("PingFang"));
  EXPECT_GT(typeface->countGlyphs(), 0);
}

}  // namespace skia
