// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_mojom_traits.h"

#include <optional>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace blink {

class ManifestMojomTraitsTest : public testing::Test {};

// Tests the StructTraits path for ShortcutItem with localized name field.
// This exercises the absl::flat_hash_map<icu::Locale, ...> conversion.
TEST_F(ManifestMojomTraitsTest, ShortcutItemLocalizedRoundTrip) {
  Manifest::ShortcutItem original;
  original.name = u"Shortcut";
  original.url = GURL("https://example.com/shortcut");

  original.name_localized.emplace();
  Manifest::ManifestLocalizedTextObject en_name;
  en_name.value = u"English Shortcut";
  en_name.lang = u"en";
  en_name.dir = mojom::Manifest_TextDirection::kLTR;
  original.name_localized->emplace(icu::Locale::getEnglish(), en_name);

  Manifest::ManifestLocalizedTextObject zh_hans_cn_name;
  zh_hans_cn_name.value = u"简体中文快捷方式";
  zh_hans_cn_name.lang = u"zh-Hans-CN";
  original.name_localized->emplace(icu::Locale("zh", "Hans", "CN"),
                                   zh_hans_cn_name);

  Manifest::ShortcutItem round_tripped;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestShortcutItem>(
      original, round_tripped));

  ASSERT_TRUE(round_tripped.name_localized.has_value());
  EXPECT_EQ(2u, round_tripped.name_localized->size());

  auto en_it = round_tripped.name_localized->find(icu::Locale::getEnglish());
  ASSERT_NE(en_it, round_tripped.name_localized->end());
  EXPECT_EQ(u"English Shortcut", en_it->second.value);
  EXPECT_EQ(u"en", en_it->second.lang);
  EXPECT_EQ(mojom::Manifest_TextDirection::kLTR, en_it->second.dir);

  auto zh_it =
      round_tripped.name_localized->find(icu::Locale("zh", "Hans", "CN"));
  ASSERT_NE(zh_it, round_tripped.name_localized->end());
  EXPECT_EQ(u"简体中文快捷方式", zh_it->second.value);
  EXPECT_EQ(u"zh-Hans-CN", zh_it->second.lang);
}

// Tests the StructTraits path for ShortcutItem with localized icons field.
TEST_F(ManifestMojomTraitsTest, ShortcutItemIconsLocalizedRoundTrip) {
  Manifest::ShortcutItem original;
  original.name = u"Shortcut";
  original.url = GURL("https://example.com/shortcut");

  original.icons_localized.emplace();

  std::vector<Manifest::ImageResource> en_icons_vec;
  Manifest::ImageResource en_icon;
  en_icon.src = GURL("https://example.com/icon-en.png");
  en_icon.type = u"image/png";
  en_icon.sizes.emplace_back(192, 192);
  en_icon.purpose.emplace_back(mojom::ManifestImageResource_Purpose::ANY);
  en_icons_vec.emplace_back(std::move(en_icon));
  original.icons_localized->emplace(icu::Locale::getEnglish(),
                                    std::move(en_icons_vec));

  std::vector<Manifest::ImageResource> fr_icons_vec;
  Manifest::ImageResource fr_icon;
  fr_icon.src = GURL("https://example.com/icon-fr.png");
  fr_icon.type = u"image/png";
  fr_icon.sizes.emplace_back(192, 192);
  fr_icon.purpose.emplace_back(mojom::ManifestImageResource_Purpose::ANY);
  fr_icons_vec.emplace_back(std::move(fr_icon));
  original.icons_localized->emplace(icu::Locale::getFrench(),
                                    std::move(fr_icons_vec));

  Manifest::ShortcutItem round_tripped;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestShortcutItem>(
      original, round_tripped));

  ASSERT_TRUE(round_tripped.icons_localized.has_value());
  EXPECT_EQ(2u, round_tripped.icons_localized->size());

  auto en_it = round_tripped.icons_localized->find(icu::Locale::getEnglish());
  ASSERT_NE(en_it, round_tripped.icons_localized->end());
  ASSERT_EQ(1u, en_it->second.size());
  EXPECT_EQ(GURL("https://example.com/icon-en.png"), en_it->second[0].src);

  auto fr_it = round_tripped.icons_localized->find(icu::Locale::getFrench());
  ASSERT_NE(fr_it, round_tripped.icons_localized->end());
  ASSERT_EQ(1u, fr_it->second.size());
  EXPECT_EQ(GURL("https://example.com/icon-fr.png"), fr_it->second[0].src);
}

}  // namespace blink
