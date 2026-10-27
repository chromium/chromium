// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_mojom_traits.h"

#include <optional>
#include <string>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace blink {

class ManifestMojomTraitsTest : public testing::Test {};

// Tests the StructTraits path for ShortcutItem with localized name field.
// This exercises the absl::flat_hash_map<base::i18n::LanguageTag, ...>
// conversion.
TEST_F(ManifestMojomTraitsTest, ShortcutItemLocalizedRoundTrip) {
  Manifest::ShortcutItem original;
  original.name = u"Shortcut";
  original.url = GURL("https://example.com/shortcut");

  original.name_localized.emplace();
  Manifest::ManifestLocalizedTextObject en_name;
  en_name.value = u"English Shortcut";
  en_name.lang = u"en";
  en_name.dir = mojom::Manifest_TextDirection::kLTR;
  original.name_localized->emplace(base::i18n::GetKnownLanguageTag("en"),
                                   en_name);

  Manifest::ManifestLocalizedTextObject zh_hans_cn_name;
  zh_hans_cn_name.value = u"简体中文快捷方式";
  zh_hans_cn_name.lang = u"zh-Hans-CN";
  original.name_localized->emplace(
      base::i18n::GetKnownLanguageTag("zh-Hans-CN"), zh_hans_cn_name);

  Manifest::ShortcutItem round_tripped;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestShortcutItem>(
      original, round_tripped));

  ASSERT_TRUE(round_tripped.name_localized.has_value());
  EXPECT_EQ(2u, round_tripped.name_localized->size());

  auto en_it =
      round_tripped.name_localized->find(base::i18n::GetKnownLanguageTag("en"));
  ASSERT_NE(en_it, round_tripped.name_localized->end());
  EXPECT_EQ(u"English Shortcut", en_it->second.value);
  EXPECT_EQ(u"en", en_it->second.lang);
  EXPECT_EQ(mojom::Manifest_TextDirection::kLTR, en_it->second.dir);

  auto zh_it = round_tripped.name_localized->find(
      base::i18n::GetKnownLanguageTag("zh-Hans-CN"));
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
  original.icons_localized->emplace(base::i18n::GetKnownLanguageTag("en"),
                                    std::move(en_icons_vec));

  std::vector<Manifest::ImageResource> fr_icons_vec;
  Manifest::ImageResource fr_icon;
  fr_icon.src = GURL("https://example.com/icon-fr.png");
  fr_icon.type = u"image/png";
  fr_icon.sizes.emplace_back(192, 192);
  fr_icon.purpose.emplace_back(mojom::ManifestImageResource_Purpose::ANY);
  fr_icons_vec.emplace_back(std::move(fr_icon));
  original.icons_localized->emplace(base::i18n::GetKnownLanguageTag("fr"),
                                    std::move(fr_icons_vec));

  Manifest::ShortcutItem round_tripped;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestShortcutItem>(
      original, round_tripped));

  ASSERT_TRUE(round_tripped.icons_localized.has_value());
  EXPECT_EQ(2u, round_tripped.icons_localized->size());

  auto en_it = round_tripped.icons_localized->find(
      base::i18n::GetKnownLanguageTag("en"));
  ASSERT_NE(en_it, round_tripped.icons_localized->end());
  ASSERT_EQ(1u, en_it->second.size());
  EXPECT_EQ(GURL("https://example.com/icon-en.png"), en_it->second[0].src);

  auto fr_it = round_tripped.icons_localized->find(
      base::i18n::GetKnownLanguageTag("fr"));
  ASSERT_NE(fr_it, round_tripped.icons_localized->end());
  ASSERT_EQ(1u, fr_it->second.size());
  EXPECT_EQ(GURL("https://example.com/icon-fr.png"), fr_it->second[0].src);
}

TEST_F(ManifestMojomTraitsTest, ImageResourceFtpSchemeSucceeds) {
  Manifest::ImageResource original;
  original.src = GURL("ftp://example.com/icon.png");
  original.type = u"image/png";
  original.purpose.emplace_back(mojom::ManifestImageResource_Purpose::ANY);

  Manifest::ImageResource round_tripped;
  // Should succeed to deserialize because traits only check validity, not
  // scheme.
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestImageResource>(
      original, round_tripped));
  EXPECT_EQ(original.src, round_tripped.src);
}

TEST_F(ManifestMojomTraitsTest, ImageResourceValidSchemeSucceeds) {
  Manifest::ImageResource original;
  original.src = GURL("https://example.com/icon.png");
  original.type = u"image/png";
  original.purpose.emplace_back(mojom::ManifestImageResource_Purpose::ANY);

  Manifest::ImageResource round_tripped;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestImageResource>(
      original, round_tripped));
  EXPECT_EQ(original.src, round_tripped.src);
}

TEST_F(ManifestMojomTraitsTest, ImageResourceEmptySrcFails) {
  Manifest::ImageResource original;
  original.src = GURL();  // Empty URL
  original.type = u"image/png";
  original.purpose.emplace_back(mojom::ManifestImageResource_Purpose::ANY);

  Manifest::ImageResource round_tripped;
  // Should fail to deserialize because of empty URL.
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::ManifestImageResource>(
          original, round_tripped));
}

TEST_F(ManifestMojomTraitsTest, NestedStructTraitsRoundTrip) {
  // Screenshot
  {
    Manifest::Screenshot original;
    original.image.src = GURL("https://example.com/screenshot.png");
    original.image.type = u"image/png";
    original.image.sizes.emplace_back(1280, 720);
    original.form_factor = mojom::ManifestScreenshot_FormFactor::kWide;
    original.label = u"Wide screenshot";
    Manifest::Screenshot round_tripped;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestScreenshot>(
        original, round_tripped));
    EXPECT_EQ(original, round_tripped);
  }

  // FileHandler
  {
    Manifest::FileHandler original;
    original.action = GURL("https://example.com/open");
    original.name = u"Text files";
    original.accept[u"text/plain"].push_back(u".txt");
    original.accept[u"text/plain"].push_back(u".text");
    original.launch_type = Manifest::FileHandler::LaunchType::kMultipleClients;
    Manifest::FileHandler round_tripped;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestFileHandler>(
        original, round_tripped));
    EXPECT_EQ(original, round_tripped);
  }

  // ProtocolHandler
  {
    Manifest::ProtocolHandler original;
    original.protocol = u"web+custom";
    original.url = GURL("https://example.com/handle?p=%s");
    Manifest::ProtocolHandler round_tripped;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::ManifestProtocolHandler>(
            original, round_tripped));
    EXPECT_EQ(original, round_tripped);
  }

  // ScopeExtension
  {
    Manifest::ScopeExtension original;
    original.origin = url::Origin::Create(GURL("https://sub.example.com"));
    original.has_origin_wildcard = true;
    Manifest::ScopeExtension round_tripped;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::ManifestScopeExtension>(
            original, round_tripped));
    EXPECT_EQ(original, round_tripped);
  }

  // LockScreen
  {
    Manifest::LockScreen original;
    original.start_url = GURL("https://example.com/lock");
    Manifest::LockScreen round_tripped;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestLockScreen>(
        original, round_tripped));
    EXPECT_EQ(original, round_tripped);
  }

  // NoteTaking
  {
    Manifest::NoteTaking original;
    original.new_note_url = GURL("https://example.com/new-note");
    Manifest::NoteTaking round_tripped;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestNoteTaking>(
        original, round_tripped));
    EXPECT_EQ(original, round_tripped);
  }

  // MigrateFrom
  {
    Manifest::MigrateFrom original;
    original.id = GURL("https://example.com/old/id");
    original.install_url = GURL("https://example.com/old/install");
    original.behavior = mojom::ManifestMigrationBehavior::kForce;
    Manifest::MigrateFrom round_tripped;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestMigrateFrom>(
        original, round_tripped));
    EXPECT_EQ(original, round_tripped);
  }

  // MigrateTo
  {
    Manifest::MigrateTo original;
    original.id = GURL("https://example.com/new/id");
    original.install_url = GURL("https://example.com/new/install");
    Manifest::MigrateTo round_tripped;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ManifestMigrateTo>(
        original, round_tripped));
    EXPECT_EQ(original, round_tripped);
  }
}

}  // namespace blink
