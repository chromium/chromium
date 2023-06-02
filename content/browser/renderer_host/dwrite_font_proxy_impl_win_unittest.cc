// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"

#include <dwrite.h>
#include <dwrite_2.h>

#include <memory>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/icu/source/common/unicode/umachine.h"

namespace content {

namespace {

// Base test class that sets up the Mojo connection to DWriteFontProxy so that
// tests can call its Mojo methods.
class DWriteFontProxyImplUnitTest : public testing::Test {
 public:
  DWriteFontProxyImplUnitTest()
      : receiver_(&impl_, dwrite_font_proxy_.BindNewPipeAndPassReceiver()) {}

  blink::mojom::DWriteFontProxy& dwrite_font_proxy() {
    return *dwrite_font_proxy_;
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<blink::mojom::DWriteFontProxy> dwrite_font_proxy_;
  DWriteFontProxyImpl impl_;
  mojo::Receiver<blink::mojom::DWriteFontProxy> receiver_;
};

TEST_F(DWriteFontProxyImplUnitTest, GetFamilyCount) {
  UINT32 family_count = 0;
  dwrite_font_proxy().GetFamilyCount(&family_count);
  EXPECT_NE(0u, family_count);  // Assume there's some fonts on the test system.
}

TEST_F(DWriteFontProxyImplUnitTest, FindFamily) {
  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(u"Arial", &arial_index);
  EXPECT_NE(UINT_MAX, arial_index);

  UINT32 times_index = 0;
  dwrite_font_proxy().FindFamily(u"Times New Roman", &times_index);
  EXPECT_NE(UINT_MAX, times_index);
  EXPECT_NE(arial_index, times_index);

  UINT32 unknown_index = 0;
  dwrite_font_proxy().FindFamily(u"Not a font family", &unknown_index);
  EXPECT_EQ(UINT_MAX, unknown_index);
}

TEST_F(DWriteFontProxyImplUnitTest, GetFamilyNames) {
  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(u"Arial", &arial_index);

  std::vector<blink::mojom::DWriteStringPairPtr> names;
  dwrite_font_proxy().GetFamilyNames(arial_index, &names);

  EXPECT_LT(0u, names.size());
  for (const auto& pair : names) {
    EXPECT_FALSE(pair->first.empty());
    EXPECT_FALSE(pair->second.empty());
  }
}

TEST_F(DWriteFontProxyImplUnitTest, GetFamilyNamesIndexOutOfBounds) {
  std::vector<blink::mojom::DWriteStringPairPtr> names;
  UINT32 invalid_index = 1000000;
  dwrite_font_proxy().GetFamilyNames(invalid_index, &names);

  EXPECT_TRUE(names.empty());
}

TEST_F(DWriteFontProxyImplUnitTest, GetFontFileHandles) {
  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(u"Arial", &arial_index);

  std::vector<base::File> handles;
  dwrite_font_proxy().GetFontFileHandles(arial_index, &handles);

  EXPECT_LT(0u, handles.size());
  for (auto& file : handles) {
    EXPECT_TRUE(file.IsValid());
    EXPECT_LT(0, file.GetLength());  // Check the file exists
  }
}

TEST_F(DWriteFontProxyImplUnitTest, GetFontFileHandlesIndexOutOfBounds) {
  std::vector<base::File> handles;
  UINT32 invalid_index = 1000000;
  dwrite_font_proxy().GetFontFileHandles(invalid_index, &handles);

  EXPECT_EQ(0u, handles.size());
}

TEST_F(DWriteFontProxyImplUnitTest, MapCharacter) {
  blink::mojom::MapCharactersResultPtr result;
  dwrite_font_proxy().MapCharacters(
      u"abc",
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL),
      std::u16string(), DWRITE_READING_DIRECTION_LEFT_TO_RIGHT,
      std::u16string(), &result);

  EXPECT_NE(UINT32_MAX, result->family_index);
  EXPECT_NE(std::u16string(), result->family_name);
  EXPECT_EQ(3u, result->mapped_length);
  EXPECT_NE(0.0, result->scale);
  EXPECT_NE(0, result->font_style->font_weight);
  EXPECT_EQ(DWRITE_FONT_STYLE_NORMAL, result->font_style->font_slant);
  EXPECT_NE(0, result->font_style->font_stretch);
}

TEST_F(DWriteFontProxyImplUnitTest, MapCharacterInvalidCharacter) {
  blink::mojom::MapCharactersResultPtr result;
  dwrite_font_proxy().MapCharacters(
      u"\ufffe\uffffabc",
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL),
      u"en-us", DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, std::u16string(),
      &result);

  EXPECT_EQ(UINT32_MAX, result->family_index);
  EXPECT_EQ(std::u16string(), result->family_name);
  EXPECT_EQ(2u, result->mapped_length);
}

TEST_F(DWriteFontProxyImplUnitTest, MapCharacterInvalidAfterValid) {
  blink::mojom::MapCharactersResultPtr result;
  dwrite_font_proxy().MapCharacters(
      u"abc\ufffe\uffff",
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL),
      u"en-us", DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, std::u16string(),
      &result);

  EXPECT_NE(UINT32_MAX, result->family_index);
  EXPECT_NE(std::u16string(), result->family_name);
  EXPECT_EQ(3u, result->mapped_length);
  EXPECT_NE(0.0, result->scale);
  EXPECT_NE(0, result->font_style->font_weight);
  EXPECT_EQ(DWRITE_FONT_STYLE_NORMAL, result->font_style->font_slant);
  EXPECT_NE(0, result->font_style->font_stretch);
}

TEST_F(DWriteFontProxyImplUnitTest, TestCustomFontFiles) {
  // Override windows fonts path to force the custom font file codepath.
  impl_.SetWindowsFontsPathForTesting(u"X:\\NotWindowsFonts");

  UINT32 arial_index = 0;
  dwrite_font_proxy().FindFamily(u"Arial", &arial_index);

  std::vector<base::File> handles;
  dwrite_font_proxy().GetFontFileHandles(arial_index, &handles);

  EXPECT_FALSE(handles.empty());
  for (auto& file : handles) {
    EXPECT_TRUE(file.IsValid());
    EXPECT_LT(0, file.GetLength());  // Check the file exists
  }
}

}  // namespace

}  // namespace content
