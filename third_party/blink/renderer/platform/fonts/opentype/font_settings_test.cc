// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

template <typename T, typename U>
scoped_refptr<T> MakeSettings(std::initializer_list<U> items) {
  scoped_refptr<T> settings = T::Create();
  for (auto item = items.begin(); item != items.end(); ++item) {
    settings->Append(*item);
  }
  return settings;
}

}  // namespace

TEST(FontSettingsTest, HashTest) {
  scoped_refptr<FontVariationSettings> one_axis_a =
      MakeSettings<FontVariationSettings, FontVariationAxis>(
          {FontVariationAxis{AtomicString("a   "), 0}});
  scoped_refptr<FontVariationSettings> one_axis_b =
      MakeSettings<FontVariationSettings, FontVariationAxis>(
          {FontVariationAxis{AtomicString("b   "), 0}});
  scoped_refptr<FontVariationSettings> two_axes =
      MakeSettings<FontVariationSettings, FontVariationAxis>(
          {FontVariationAxis{AtomicString("a   "), 0},
           FontVariationAxis{AtomicString("b   "), 0}});
  scoped_refptr<FontVariationSettings> two_axes_different_value =
      MakeSettings<FontVariationSettings, FontVariationAxis>(
          {FontVariationAxis{AtomicString("a   "), 0},
           FontVariationAxis{AtomicString("b   "), 1}});

  scoped_refptr<FontVariationSettings> empty_variation_settings =
      FontVariationSettings::Create();

  CHECK_NE(one_axis_a->GetHash(), one_axis_b->GetHash());
  CHECK_NE(one_axis_a->GetHash(), two_axes->GetHash());
  CHECK_NE(one_axis_a->GetHash(), two_axes_different_value->GetHash());
  CHECK_NE(empty_variation_settings->GetHash(), one_axis_a->GetHash());
  CHECK_EQ(empty_variation_settings->GetHash(), 0u);
}

TEST(FontSettingsTest, ToString) {
  {
    scoped_refptr<FontVariationSettings> settings =
        MakeSettings<FontVariationSettings, FontVariationAxis>(
            {FontVariationAxis{AtomicString("aaaa"), 42},
             FontVariationAxis{AtomicString("bbbb"), 8118}});
    EXPECT_EQ("aaaa=42,bbbb=8118", settings->ToString());
  }
  {
    scoped_refptr<FontFeatureSettings> settings =
        MakeSettings<FontFeatureSettings, FontFeature>(
            {FontFeature{AtomicString("aaaa"), 42},
             FontFeature{AtomicString("bbbb"), 8118}});
    EXPECT_EQ("aaaa=42,bbbb=8118", settings->ToString());
  }
}
TEST(FontSettingsTest, FindTest) {
  {
    scoped_refptr<FontVariationSettings> settings =
        MakeSettings<FontVariationSettings, FontVariationAxis>(
            {FontVariationAxis{AtomicString("abcd"), 42},
             FontVariationAxis{AtomicString("efgh"), 8118}});
    FontVariationAxis found_axis(0, 0);
    ASSERT_FALSE(settings->FindPair('aaaa', &found_axis));
    ASSERT_FALSE(settings->FindPair('bbbb', &found_axis));
    ASSERT_EQ(found_axis.Value(), 0);
    ASSERT_TRUE(settings->FindPair('abcd', &found_axis));
    ASSERT_EQ(found_axis.TagString(), AtomicString("abcd"));
    ASSERT_EQ(found_axis.Value(), 42);
    ASSERT_TRUE(settings->FindPair('efgh', &found_axis));
    ASSERT_EQ(found_axis.TagString(), AtomicString("efgh"));
    ASSERT_EQ(found_axis.Value(), 8118);
  }
}

TEST(FontSettingsTest, FindTestEmpty) {
  scoped_refptr<FontVariationSettings> settings =
      MakeSettings<FontVariationSettings, FontVariationAxis>({});
  FontVariationAxis found_axis(0, 0);
  ASSERT_FALSE(settings->FindPair('aaaa', &found_axis));
}

}  // namespace blink
