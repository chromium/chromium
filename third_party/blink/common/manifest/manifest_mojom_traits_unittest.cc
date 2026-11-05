// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_mojom_traits.h"

#include <optional>
#include <string>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_invariants.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace blink {
namespace {

Manifest CreateFullManifest() {
  Manifest::Builder builder(GURL("https://example.com/app/index.html"),
                            GURL("https://example.com/app/id"),
                            GURL("https://example.com/"));

  builder.SetManifestUrl(GURL("https://example.com/app/manifest.json"));
  builder.SetDir(mojom::Manifest_TextDirection::kLTR);
  builder.SetName(u"Test App Name");
  builder.SetShortName(u"Test App");
  builder.SetDescription(u"A complete test app description.");
  builder.SetHasCustomId(true);
  builder.SetHasValidSpecifiedStartUrl(true);
  builder.SetDisplay(mojom::DisplayMode::kStandalone);

  std::vector<Manifest::DisplayOverride> overrides;
  overrides.push_back(
      Manifest::DisplayOverride::Create(mojom::DisplayMode::kMinimalUi));
  overrides.push_back(
      Manifest::DisplayOverride::Create(mojom::DisplayMode::kStandalone));
  builder.SetDisplayOverride(std::move(overrides));

  builder.SetOrientation(
      device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY);

  // Icons
  std::vector<Manifest::ImageResource> icons;
  Manifest::ImageResource icon;
  icon.src = GURL("https://example.com/app/icon.png");
  icon.type = u"image/png";
  icon.sizes.emplace_back(192, 192);
  icon.purpose.push_back(mojom::ManifestImageResource_Purpose::ANY);
  icons.push_back(std::move(icon));
  builder.SetIcons(icons);

  // Screenshots
  std::vector<Manifest::Screenshot> screenshots;
  Manifest::Screenshot screenshot;
  screenshot.image.src = GURL("https://example.com/app/screenshot.png");
  screenshot.image.type = u"image/png";
  screenshot.image.sizes.emplace_back(800, 600);
  screenshot.form_factor = mojom::ManifestScreenshot_FormFactor::kWide;
  screenshot.label = u"Desktop Screenshot";
  screenshots.push_back(std::move(screenshot));
  builder.SetScreenshots(std::move(screenshots));

  // Shortcuts
  std::vector<Manifest::ShortcutItem> shortcuts;
  Manifest::ShortcutItem shortcut;
  shortcut.name = u"App Shortcut";
  shortcut.url = GURL("https://example.com/app/shortcut");
  shortcuts.push_back(std::move(shortcut));
  builder.SetShortcuts(std::move(shortcuts));

  // Share target
  Manifest::ShareTarget share_target;
  share_target.action = GURL("https://example.com/app/share");
  share_target.method = mojom::ManifestShareTarget_Method::kPost;
  share_target.enctype = mojom::ManifestShareTarget_Enctype::kMultipartFormData;
  share_target.params.title = u"title";
  share_target.params.text = u"text";
  builder.SetShareTarget(std::move(share_target));

  // File handlers
  std::vector<Manifest::FileHandler> file_handlers;
  Manifest::FileHandler file_handler;
  file_handler.action = GURL("https://example.com/app/open-file");
  file_handler.name = u"Text files";
  file_handler.accept[u"text/plain"].push_back(u".txt");
  file_handler.launch_type = Manifest::FileHandler::LaunchType::kSingleClient;
  file_handlers.push_back(std::move(file_handler));
  builder.SetFileHandlers(std::move(file_handlers));

  // Protocol handlers
  std::vector<Manifest::ProtocolHandler> protocol_handlers;
  Manifest::ProtocolHandler protocol_handler;
  protocol_handler.protocol = u"web+custom";
  protocol_handler.url = GURL("https://example.com/app/handle?proto=%s");
  protocol_handlers.push_back(std::move(protocol_handler));
  builder.SetProtocolHandlers(std::move(protocol_handlers));

  // Scope extensions
  std::vector<Manifest::ScopeExtension> scope_extensions;
  Manifest::ScopeExtension scope_extension;
  scope_extension.origin = url::Origin::Create(GURL("https://example-sub.com"));
  scope_extension.has_origin_wildcard = false;
  scope_extensions.push_back(std::move(scope_extension));
  builder.SetScopeExtensions(std::move(scope_extensions));

  // Lock screen
  Manifest::LockScreen lock_screen;
  lock_screen.start_url = GURL("https://example.com/app/lock");
  builder.SetLockScreen(std::move(lock_screen));

  // Note taking
  Manifest::NoteTaking note_taking;
  note_taking.new_note_url = GURL("https://example.com/app/new-note");
  builder.SetNoteTaking(std::move(note_taking));

  // Related applications
  std::vector<Manifest::RelatedApplication> related_applications;
  Manifest::RelatedApplication app;
  app.platform = u"play";
  app.id = u"com.example.app";
  app.url =
      GURL("https://play.google.com/store/apps/details?id=com.example.app");
  related_applications.push_back(std::move(app));
  builder.SetRelatedApplications(std::move(related_applications));
  builder.SetPreferRelatedApplications(true);

  // Migrate from
  std::vector<Manifest::MigrateFrom> migrate_from;
  Manifest::MigrateFrom from;
  from.id = GURL("https://example.com/old/id");
  from.install_url = GURL("https://example.com/old/install");
  migrate_from.push_back(std::move(from));
  builder.SetMigrateFrom(std::move(migrate_from));

  // Migrate to
  Manifest::MigrateTo migrate_to;
  migrate_to.id = GURL("https://example.com/new/id");
  migrate_to.install_url = GURL("https://example.com/new/install");
  builder.SetMigrateTo(std::move(migrate_to));

  builder.SetThemeColor(0xFF00FF00);
  builder.SetBackgroundColor(0xFFFFFFFF);
  builder.SetGcmSenderId(u"1234567890");

  builder.SetUpdateManifestUrl(
      GURL("https://example.com/app/update_manifest.json"));

  Manifest::LaunchHandler launch_handler(
      Manifest::LaunchHandler::ClientMode::kNavigateExisting);
  builder.SetLaunchHandler(launch_handler);

  // Translations
  base::flat_map<std::u16string, Manifest::TranslationItem> translations;
  Manifest::TranslationItem item;
  item.name = "Nom de l'application";
  item.short_name = "App";
  item.description = "Description en francais";
  translations[u"fr"] = std::move(item);
  builder.SetTranslations(std::move(translations));

  builder.SetDarkThemeColor(0xFF003300);
  builder.SetDarkBackgroundColor(0xFF111111);

  // Tab strip
  Manifest::TabStrip tab_strip;
  Manifest::HomeTabParams home_tab;
  home_tab.icons.push_back(icons[0]);
  tab_strip.home_tab = std::move(home_tab);
  Manifest::NewTabButtonParams new_tab_button;
  new_tab_button.url = GURL("https://example.com/app/new-tab");
  tab_strip.new_tab_button = std::move(new_tab_button);
  builder.SetTabStrip(std::move(tab_strip));

  builder.SetVersion(u"1.2.3");

  // Localized name
  absl::flat_hash_map<base::i18n::LanguageTag,
                      Manifest::ManifestLocalizedTextObject>
      name_localized;
  Manifest::ManifestLocalizedTextObject en_name;
  en_name.value = u"App (EN)";
  en_name.lang = u"en";
  en_name.dir = mojom::Manifest_TextDirection::kLTR;
  name_localized.emplace(base::i18n::GetKnownLanguageTag("en"),
                         std::move(en_name));
  builder.SetNameLocalized(std::move(name_localized));

  // Localized short name
  absl::flat_hash_map<base::i18n::LanguageTag,
                      Manifest::ManifestLocalizedTextObject>
      short_name_localized;
  Manifest::ManifestLocalizedTextObject en_short_name;
  en_short_name.value = u"App";
  en_short_name.lang = u"en";
  en_short_name.dir = mojom::Manifest_TextDirection::kLTR;
  short_name_localized.emplace(base::i18n::GetKnownLanguageTag("en"),
                               std::move(en_short_name));
  builder.SetShortNameLocalized(std::move(short_name_localized));

  // Localized description
  absl::flat_hash_map<base::i18n::LanguageTag,
                      Manifest::ManifestLocalizedTextObject>
      description_localized;
  Manifest::ManifestLocalizedTextObject en_description;
  en_description.value = u"A complete test app description.";
  en_description.lang = u"en";
  en_description.dir = mojom::Manifest_TextDirection::kLTR;
  description_localized.emplace(base::i18n::GetKnownLanguageTag("en"),
                                std::move(en_description));
  builder.SetDescriptionLocalized(std::move(description_localized));

  // Localized icons
  absl::flat_hash_map<base::i18n::LanguageTag,
                      std::vector<Manifest::ImageResource>>
      icons_localized;
  std::vector<Manifest::ImageResource> en_icons;
  en_icons.push_back(icons[0]);
  icons_localized.emplace(base::i18n::GetKnownLanguageTag("en"),
                          std::move(en_icons));
  builder.SetIconsLocalized(std::move(icons_localized));

  return std::move(builder).BuildChecked();
}

Manifest CreateEmptyValidManifest() {
  return Manifest::Builder(GURL("https://example.com/app/index.html"),
                           GURL("https://example.com/app/id"),
                           GURL("https://example.com/"))
      .BuildChecked();
}

}  // namespace

class ManifestMojomTraitsTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

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

TEST_F(ManifestMojomTraitsTest, FullManifestRoundTrip) {
  Manifest original = CreateFullManifest();
  Manifest round_tripped = CreateEmptyValidManifest();
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Manifest>(
      original, round_tripped));
  EXPECT_EQ(original, round_tripped);
}

TEST_F(ManifestMojomTraitsTest, InvalidManifestFailsToDeserialize) {
  base::HistogramTester histogram_tester;

  mojo::Message message;
  mojo::internal::MessageDispatchContext dispatch_context(&message);

  mojom::ManifestPtr bad_manifest = mojom::Manifest::New();
  // start_url is empty, violating the start_url validity invariant.
  mojo::Message serialized = mojom::Manifest::SerializeAsMessage(&bad_manifest);

  Manifest out = CreateEmptyValidManifest();
  EXPECT_FALSE(
      mojom::Manifest::DeserializeFromMessage(std::move(serialized), &out));

  histogram_tester.ExpectBucketCount(
      "WebApp.Manifest.InvariantViolation",
      static_cast<int>(ManifestInvariantError::kStartUrlInvalid), 1);

  ASSERT_EQ(1u, dispatch_context.error_trace().size());
  EXPECT_EQ(static_cast<int32_t>(ManifestInvariantError::kStartUrlInvalid),
            dispatch_context.error_trace()[0].custom_code());
}

TEST_F(ManifestMojomTraitsTest, CrossOriginIdFailsToDeserialize) {
  base::HistogramTester histogram_tester;

  mojo::Message message;
  mojo::internal::MessageDispatchContext dispatch_context(&message);

  mojom::ManifestPtr bad_manifest = mojom::Manifest::New();
  bad_manifest->start_url = GURL("https://example.com/start");
  bad_manifest->id = GURL("https://evil.com/id");
  bad_manifest->scope = GURL("https://example.com/");
  mojo::Message serialized = mojom::Manifest::SerializeAsMessage(&bad_manifest);

  Manifest out = CreateEmptyValidManifest();
  EXPECT_FALSE(
      mojom::Manifest::DeserializeFromMessage(std::move(serialized), &out));

  histogram_tester.ExpectBucketCount(
      "WebApp.Manifest.InvariantViolation",
      static_cast<int>(ManifestInvariantError::kIdCrossOrigin), 1);

  ASSERT_EQ(1u, dispatch_context.error_trace().size());
  EXPECT_EQ(static_cast<int32_t>(ManifestInvariantError::kIdCrossOrigin),
            dispatch_context.error_trace()[0].custom_code());
}

}  // namespace blink
