// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest.h"

#include <optional>
#include <string>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/manifest/manifest_invariants.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace blink {

namespace {

Manifest::Builder CreateValidBuilder() {
  GURL start_url("https://example.com/app/index.html");
  GURL id("https://example.com/app/id");
  GURL scope("https://example.com/app/");
  return Manifest::Builder(std::move(start_url), std::move(id),
                           std::move(scope));
}

Manifest CreateFullManifest() {
  Manifest::Builder builder(GURL("https://example.com/app/index.html"),
                            GURL("https://example.com/app/id"),
                            GURL("https://example.com/app/"));

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

  return std::move(builder).BuildChecked();
}

}  // namespace

// Tests covering intrinsic manifest invariants.
class ManifestInvariantsTest : public testing::Test {};

TEST_F(ManifestInvariantsTest, Invariant1_ValidAndNonEmpty) {
  // start_url invalid/empty
  {
    Manifest::Builder b(GURL(), GURL("https://example.com/id"),
                        GURL("https://example.com/"));
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kStartUrlInvalid);
  }
  {
    Manifest::Builder b(GURL("invalid-url"), GURL("https://example.com/id"),
                        GURL("https://example.com/"));
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kStartUrlInvalid);
  }

  // id invalid/empty
  {
    Manifest::Builder b(GURL("https://example.com/start"), GURL(),
                        GURL("https://example.com/"));
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kIdInvalid);
  }

  // scope invalid/empty
  {
    Manifest::Builder b(GURL("https://example.com/start"),
                        GURL("https://example.com/id"), GURL());
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kScopeInvalid);
  }
}

TEST_F(ManifestInvariantsTest, Invariant2_StartUrlOriginNotOpaque) {
  Manifest::Builder b(GURL("data:text/html,Hello"),
                      GURL("data:text/html,Hello"),
                      GURL("data:text/html,Hello"));
  auto result = std::move(b).Build();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ManifestInvariantError::kOriginOpaque);
}

TEST_F(ManifestInvariantsTest, Invariant3_IdAndScopeSameOrigin) {
  // id cross-origin
  {
    Manifest::Builder b(GURL("https://example.com/start"),
                        GURL("https://attacker.com/id"),
                        GURL("https://example.com/"));
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kIdCrossOrigin);
  }

  // scope cross-origin
  {
    Manifest::Builder b(GURL("https://example.com/start"),
                        GURL("https://example.com/id"),
                        GURL("https://attacker.com/"));
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kScopeCrossOrigin);
  }
}

TEST_F(ManifestInvariantsTest, Invariant4_ShareTargetActionSameOrigin) {
  auto b = CreateValidBuilder();
  Manifest::ShareTarget share_target;
  share_target.action = GURL("https://attacker.com/share");
  b.SetShareTarget(std::move(share_target));
  auto result = std::move(b).Build();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            ManifestInvariantError::kShareTargetActionCrossOrigin);

  // Same origin passes
  auto b2 = CreateValidBuilder();
  Manifest::ShareTarget share_target2;
  share_target2.action = GURL("https://example.com/share");
  b2.SetShareTarget(std::move(share_target2));
  EXPECT_TRUE(std::move(b2).Build().has_value());
}

TEST_F(ManifestInvariantsTest, Invariant5_FileHandlers) {
  // Action cross-origin
  {
    auto b = CreateValidBuilder();
    Manifest::FileHandler fh;
    fh.action = GURL("https://attacker.com/open");
    fh.accept[u"text/plain"].push_back(u".txt");
    b.SetFileHandlers({fh});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kFileHandlerActionCrossOrigin);
  }

  // Invalid MIME type
  {
    auto b = CreateValidBuilder();
    Manifest::FileHandler fh;
    fh.action = GURL("https://example.com/open");
    fh.accept[u"invalid-mime"].push_back(u".txt");
    b.SetFileHandlers({fh});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kFileHandlerInvalidMimeType);
  }

  // Empty extension list
  {
    auto b = CreateValidBuilder();
    Manifest::FileHandler fh;
    fh.action = GURL("https://example.com/open");
    fh.accept[u"text/plain"] = {};
    b.SetFileHandlers({fh});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kFileHandlerEmptyExtensionList);
  }

  // Invalid extension (no dot or only dot)
  {
    auto b = CreateValidBuilder();
    Manifest::FileHandler fh;
    fh.action = GURL("https://example.com/open");
    fh.accept[u"text/plain"].push_back(u"txt");
    b.SetFileHandlers({fh});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kFileHandlerInvalidExtension);
  }
  {
    auto b = CreateValidBuilder();
    Manifest::FileHandler fh;
    fh.action = GURL("https://example.com/open");
    fh.accept[u"text/plain"].push_back(u".");
    b.SetFileHandlers({fh});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kFileHandlerInvalidExtension);
  }

  // Extension with control character
  {
    auto b = CreateValidBuilder();
    Manifest::FileHandler fh;
    fh.action = GURL("https://example.com/open");
    std::u16string ext = u".tx\x0001t";
    fh.accept[u"text/plain"].push_back(ext);
    b.SetFileHandlers({fh});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        ManifestInvariantError::kFileHandlerExtensionContainsControlOrFormat);
  }
}

TEST_F(ManifestInvariantsTest, Invariant6_ProtocolHandlers) {
  // Action cross-origin
  {
    auto b = CreateValidBuilder();
    Manifest::ProtocolHandler ph;
    ph.protocol = u"web+custom";
    ph.url = GURL("https://attacker.com/handle?p=%s");
    b.SetProtocolHandlers({ph});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kProtocolHandlerUrlCrossOrigin);
  }

  // Invalid protocol scheme (e.g. http or https)
  {
    auto b = CreateValidBuilder();
    Manifest::ProtocolHandler ph;
    ph.protocol = u"https";
    ph.url = GURL("https://example.com/handle?p=%s");
    b.SetProtocolHandlers({ph});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kProtocolHandlerInvalidScheme);
  }

  // Valid protocol handler
  {
    auto b = CreateValidBuilder();
    Manifest::ProtocolHandler ph;
    ph.protocol = u"web+custom";
    ph.url = GURL("https://example.com/handle?p=%s");
    b.SetProtocolHandlers({ph});
    EXPECT_TRUE(std::move(b).Build().has_value());
  }
}

TEST_F(ManifestInvariantsTest, Invariant7_NoteTaking) {
  // Empty new_note_url passes (boundary condition from parser)
  {
    auto b = CreateValidBuilder();
    Manifest::NoteTaking nt;
    nt.new_note_url = GURL();
    b.SetNoteTaking(nt);
    EXPECT_TRUE(std::move(b).Build().has_value());
  }

  // Cross-origin fails
  {
    auto b = CreateValidBuilder();
    Manifest::NoteTaking nt;
    nt.new_note_url = GURL("https://attacker.com/note");
    b.SetNoteTaking(nt);
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kNoteTakingUrlCrossOrigin);
  }

  // Same-origin passes
  {
    auto b = CreateValidBuilder();
    Manifest::NoteTaking nt;
    nt.new_note_url = GURL("https://example.com/app/note");
    b.SetNoteTaking(nt);
    EXPECT_TRUE(std::move(b).Build().has_value());
  }
}

TEST_F(ManifestInvariantsTest, Invariant8_LockScreen) {
  // Empty start_url passes (boundary condition)
  {
    auto b = CreateValidBuilder();
    Manifest::LockScreen ls;
    ls.start_url = GURL();
    b.SetLockScreen(ls);
    EXPECT_TRUE(std::move(b).Build().has_value());
  }

  // Cross-origin fails
  {
    auto b = CreateValidBuilder();
    Manifest::LockScreen ls;
    ls.start_url = GURL("https://attacker.com/lock");
    b.SetLockScreen(ls);
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kLockScreenStartUrlCrossOrigin);
  }

  // Same-origin passes
  {
    auto b = CreateValidBuilder();
    Manifest::LockScreen ls;
    ls.start_url = GURL("https://example.com/app/lock");
    b.SetLockScreen(ls);
    EXPECT_TRUE(std::move(b).Build().has_value());
  }
}

TEST_F(ManifestInvariantsTest, Invariant9_MigrateFrom) {
  // id invalid
  {
    auto b = CreateValidBuilder();
    Manifest::MigrateFrom mf;
    mf.id = GURL();
    b.SetMigrateFrom({mf});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kMigrateFromIdInvalid);
  }

  // id cross-site
  {
    auto b = CreateValidBuilder();
    Manifest::MigrateFrom mf;
    mf.id = GURL("https://otherdomain.com/id");
    b.SetMigrateFrom({mf});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kMigrateFromIdCrossSite);
  }

  // install_url cross-site
  {
    auto b = CreateValidBuilder();
    Manifest::MigrateFrom mf;
    mf.id = GURL("https://sub.example.com/id");
    mf.install_url = GURL("https://otherdomain.com/install");
    b.SetMigrateFrom({mf});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kMigrateFromInstallUrlCrossSite);
  }

  // install_url cross-origin with id
  {
    auto b = CreateValidBuilder();
    Manifest::MigrateFrom mf;
    mf.id = GURL("https://sub1.example.com/id");
    mf.install_url = GURL("https://sub2.example.com/install");
    b.SetMigrateFrom({mf});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kMigrateFromInstallUrlCrossOriginWithId);
  }

  // Valid migrate_from
  {
    auto b = CreateValidBuilder();
    Manifest::MigrateFrom mf;
    mf.id = GURL("https://sub.example.com/id");
    mf.install_url = GURL("https://sub.example.com/install");
    b.SetMigrateFrom({mf});
    EXPECT_TRUE(std::move(b).Build().has_value());
  }
}

TEST_F(ManifestInvariantsTest, Invariant10_MigrateTo) {
  // id invalid
  {
    auto b = CreateValidBuilder();
    Manifest::MigrateTo mt;
    mt.id = GURL();
    mt.install_url = GURL("https://example.com/install");
    b.SetMigrateTo(mt);
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kMigrateToIdInvalid);
  }

  // id cross-site
  {
    auto b = CreateValidBuilder();
    Manifest::MigrateTo mt;
    mt.id = GURL("https://otherdomain.com/id");
    mt.install_url = GURL("https://otherdomain.com/install");
    b.SetMigrateTo(mt);
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kMigrateToIdCrossSite);
  }

  // install_url cross-origin with id
  {
    auto b = CreateValidBuilder();
    Manifest::MigrateTo mt;
    mt.id = GURL("https://sub1.example.com/id");
    mt.install_url = GURL("https://sub2.example.com/install");
    b.SetMigrateTo(mt);
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kMigrateToInstallUrlCrossOriginWithId);
  }

  // Valid migrate_to
  {
    auto b = CreateValidBuilder();
    Manifest::MigrateTo mt;
    mt.id = GURL("https://sub.example.com/id");
    mt.install_url = GURL("https://sub.example.com/install");
    b.SetMigrateTo(mt);
    EXPECT_TRUE(std::move(b).Build().has_value());
  }
}

TEST_F(ManifestInvariantsTest, Invariant11_Shortcuts) {
  // Shortcut URL invalid
  {
    auto b = CreateValidBuilder();
    Manifest::ShortcutItem item;
    item.name = u"Shortcut";
    item.url = GURL();
    b.SetShortcuts({item});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kShortcutUrlInvalid);
  }

  // Shortcut URL cross-origin
  {
    auto b = CreateValidBuilder();
    Manifest::ShortcutItem item;
    item.name = u"Shortcut";
    item.url = GURL("https://otherdomain.com/app/shortcut");
    b.SetShortcuts({item});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kShortcutUrlOutOfScope);
  }

  // Shortcut URL not in scope
  {
    auto b = CreateValidBuilder();
    Manifest::ShortcutItem item;
    item.name = u"Shortcut";
    item.url = GURL("https://example.com/other/shortcut");
    b.SetShortcuts({item});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kShortcutUrlOutOfScope);
  }

  // Boundary condition: scope "/app", shortcut "/application" follows plain
  // string prefix semantics and passes!
  {
    Manifest::Builder b(GURL("https://example.com/app/index.html"),
                        GURL("https://example.com/app/id"),
                        GURL("https://example.com/app"));
    Manifest::ShortcutItem item;
    item.name = u"Shortcut";
    item.url = GURL("https://example.com/application");
    b.SetShortcuts({item});
    EXPECT_TRUE(std::move(b).Build().has_value());
  }
}

TEST_F(ManifestInvariantsTest, Invariant12_ScopeExtensions) {
  // Opaque origin
  {
    auto b = CreateValidBuilder();
    Manifest::ScopeExtension ext;
    ext.origin = url::Origin();  // default is opaque
    b.SetScopeExtensions({ext});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kScopeExtensionOriginOpaque);
  }

  // Non-https origin
  {
    auto b = CreateValidBuilder();
    Manifest::ScopeExtension ext;
    ext.origin = url::Origin::Create(GURL("http://example.com"));
    b.SetScopeExtensions({ext});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              ManifestInvariantError::kScopeExtensionSchemeNotHttps);
  }

  // Valid https origin passes
  {
    auto b = CreateValidBuilder();
    Manifest::ScopeExtension ext;
    ext.origin = url::Origin::Create(GURL("https://other.com"));
    b.SetScopeExtensions({ext});
    EXPECT_TRUE(std::move(b).Build().has_value());
  }
}

TEST_F(ManifestInvariantsTest, Invariant13_Icons) {
  // Invalid src
  {
    auto b = CreateValidBuilder();
    Manifest::ImageResource icon;
    icon.src = GURL();
    b.SetIcons({icon});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kIconSrcInvalid);
  }

  // Invalid scheme (e.g. ftp:)
  {
    auto b = CreateValidBuilder();
    Manifest::ImageResource icon;
    icon.src = GURL("ftp://example.com/icon.png");
    b.SetIcons({icon});
    auto result = std::move(b).Build();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ManifestInvariantError::kIconSrcInvalidScheme);
  }

  // Valid schemes: http, https, data, document_origin.scheme()
  {
    auto b = CreateValidBuilder();
    Manifest::ImageResource icon1;
    icon1.src = GURL("http://example.com/icon1.png");
    Manifest::ImageResource icon2;
    icon2.src = GURL("https://example.com/icon2.png");
    Manifest::ImageResource icon3;
    icon3.src = GURL("data:image/png;base64,iVBORw0KGgoAAAANSUhEUg==");
    b.SetIcons({icon1, icon2, icon3});
    EXPECT_TRUE(std::move(b).Build().has_value());
  }
}

TEST(ManifestTest, CreateDefault) {
  // Valid schemes
  const char* valid_urls[] = {
      "http://example.com/path/index.html",
      "https://example.com/path/index.html",
      "file:///path/to/file.html",
      "blob:https://example.com/f81d4fae-7dec-11d0-a765-00a0c91e6bf6",
      "filesystem:https://example.com/temporary/path.html",
  };

  for (const char* url_str : valid_urls) {
    GURL doc_url(url_str);
    auto manifest = Manifest::CreateDefault(doc_url);
    ASSERT_TRUE(manifest.has_value()) << "Failed for url: " << url_str;
    EXPECT_EQ(manifest->start_url(), doc_url);
    EXPECT_EQ(manifest->id(), doc_url.GetWithoutRef());
    EXPECT_EQ(manifest->scope(), Manifest::GetDefaultScope(doc_url));
    EXPECT_EQ(manifest->display(), mojom::DisplayMode::kUndefined);
    EXPECT_FALSE(manifest->has_custom_id());
    EXPECT_FALSE(manifest->has_valid_specified_start_url());
  }

  // Invalid, about:, and data: schemes return std::nullopt
  EXPECT_FALSE(Manifest::CreateDefault(GURL("")).has_value());
  EXPECT_FALSE(Manifest::CreateDefault(GURL("about:blank")).has_value());
  EXPECT_FALSE(
      Manifest::CreateDefault(GURL("data:text/html,Hello")).has_value());
  EXPECT_FALSE(Manifest::CreateDefault(GURL("invalid-url")).has_value());
}

TEST(ManifestTest, GetDefaultScope) {
  EXPECT_EQ(GURL("https://example.com/a/b/"),
            Manifest::GetDefaultScope(GURL("https://example.com/a/b/c.html")));
  EXPECT_EQ(GURL("https://example.com/a/b/"),
            Manifest::GetDefaultScope(GURL("https://example.com/a/b/")));
  EXPECT_EQ(GURL("https://example.com/"),
            Manifest::GetDefaultScope(GURL("https://example.com")));
  EXPECT_FALSE(Manifest::GetDefaultScope(GURL()).is_valid());
  EXPECT_FALSE(
      Manifest::GetDefaultScope(GURL("https://example.com:invalid/a/b"))
          .is_valid());
}

TEST(ManifestTest, ToBuilderRoundTrip) {
  Manifest original = CreateFullManifest();
  Manifest round_tripped = original.ToBuilder().BuildChecked();
  EXPECT_EQ(original, round_tripped);
}

TEST(ManifestTest, BridgeRoundTrip) {
  Manifest original = CreateFullManifest();
  mojom::ManifestPtr mojom_manifest = ManifestToMojom(original);
  ASSERT_TRUE(mojom_manifest);

  auto round_tripped = ManifestFromMojom(*mojom_manifest);
  ASSERT_TRUE(round_tripped.has_value());
  EXPECT_EQ(original, *round_tripped);
}

}  // namespace blink
