// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/font_utils.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkFourByteTag.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace skia {

#if !BUILDFLAG(IS_IOS)
TEST(FontUtilsTest, InstantiateTypefaceFromSerializedStream) {
  base::FilePath root_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_dir));
  base::FilePath font_path = root_dir.AppendASCII("third_party")
                                 .AppendASCII("test_fonts")
                                 .AppendASCII("test_fonts")
                                 .AppendASCII("Ahem.ttf");
  sk_sp<SkData> font_data =
      SkData::MakeFromFileName(font_path.AsUTF8Unsafe().c_str());
  ASSERT_TRUE(font_data);

  sk_sp<SkTypeface> orig_typeface =
      DefaultFontMgr()->makeFromData(std::move(font_data));
  ASSERT_TRUE(orig_typeface);

  sk_sp<SkData> data =
      orig_typeface->serialize(SkTypeface::SerializeBehavior::kDoIncludeData);
  ASSERT_TRUE(data);

  SkMemoryStream stream(data->data(), data->size());
  sk_sp<SkTypeface> typeface = MakeTypefaceWithFontations(&stream);
  ASSERT_TRUE(typeface);

  SkString orig_family_name;
  orig_typeface->getFamilyName(&orig_family_name);

  SkString family_name;
  typeface->getFamilyName(&family_name);
  EXPECT_FALSE(family_name.isEmpty());
  EXPECT_EQ(family_name, orig_family_name);

  EXPECT_EQ(typeface->countGlyphs(), orig_typeface->countGlyphs());
  EXPECT_GT(typeface->countGlyphs(), 0);

  EXPECT_EQ(GetTypefaceFactoryIdForTesting(typeface.get()),
            SkSetFourByteTag('f', 'n', 't', 'a'));
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace skia
