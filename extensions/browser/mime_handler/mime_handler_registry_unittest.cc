// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_registry.h"

#include <vector>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/version_info/channel.h"
#include "extensions/browser/extension_pref_names.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kViewerUrl[] = "viewer.html";

// Expected handler registration for `channel`: the ApiMimeHandler feature
// state (default or explicit override) with the parser's dev-channel
// fallback. Computed at runtime so a change to the flag's default state
// moves these expectations together with the parser gate. Must be called
// while the ScopedFeatureList under test is active.
bool ExpectHandlerRegistered(version_info::Channel channel) {
  return base::FeatureList::IsEnabled(extensions_features::kApiMimeHandler) ||
         channel <= version_info::Channel::DEV;
}

class MimeHandlerRegistryTest : public ExtensionsTest {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(extensions_features::kApiMimeHandler);
    ExtensionsTest::SetUp();
    ASSERT_TRUE(registry());
  }

  // Builds an extension with dict-format mime_types_handler. `extra_entry`
  // adds keys to the MIME type's entry next to "handler_url".
  scoped_refptr<const Extension> CreateMimeHandlerExtension(
      const std::string& name,
      const std::string& mime_type,
      const std::string& handler_url,
      base::DictValue extra_entry = base::DictValue()) {
    extra_entry.Set("handler_url", handler_url);
    return ExtensionBuilder(name)
        .SetManifestKey(
            "mime_types_handler",
            base::DictValue().Set(mime_type, std::move(extra_entry)))
        .Build();
  }

  // `extra_entry` adds keys to the PDF entry next to "handler_url".
  scoped_refptr<const Extension> CreateSplitModePdfHandlerExtension(
      base::DictValue extra_entry = base::DictValue()) {
    extra_entry.Set("handler_url", kViewerUrl);
    return ExtensionBuilder("Split PDF Handler")
        .SetManifestKey("incognito", "split")
        .SetManifestKey(
            "mime_types_handler",
            base::DictValue().Set(kPdfMimeType, std::move(extra_entry)))
        .Build();
  }

  // Builds an allowlisted dict-format mime handler whose extension ID is
  // `id_from_allowlist` (must be one of `kMIMETypeHandlersAllowlist`).
  scoped_refptr<const Extension> CreateAllowlistedHandler(
      const ExtensionId& id_from_allowlist,
      const std::string& mime_type) {
    std::string json = base::StringPrintf(
        R"("mime_types_handler": {"%s": {"handler_url": "viewer.html"}})",
        mime_type.c_str());
    return ExtensionBuilder(id_from_allowlist)
        .SetID(id_from_allowlist)
        .AddJSON(json)
        .Build();
  }

  // Overrides the persisted first-install time for `ext` so tests can
  // control the sort key used by the registry.
  void SetFirstInstallTime(const Extension* ext, base::Time t) {
    ExtensionPrefs::Get(browser_context())
        ->UpdateExtensionPref(ext->id(), kPrefFirstInstallTime,
                              base::TimeToValue(t));
  }

  void LoadExtension(const Extension* extension) {
    ExtensionRegistry::Get(browser_context())
        ->AddEnabled(base::WrapRefCounted(extension));
    ExtensionRegistry::Get(browser_context())->TriggerOnLoaded(extension);
  }

  void UnloadExtension(const Extension* extension) {
    ExtensionRegistry::Get(browser_context())->RemoveEnabled(extension->id());
    ExtensionRegistry::Get(browser_context())
        ->TriggerOnUnloaded(extension, UnloadedExtensionReason::UNINSTALL);
  }

  // Installs `extension` as an update of an already installed version.
  void UpdateExtension(const Extension* extension) {
    ExtensionRegistry* extension_registry =
        ExtensionRegistry::Get(browser_context());
    extension_registry->AddEnabled(base::WrapRefCounted(extension));
    extension_registry->TriggerOnWillBeInstalled(
        extension, /*is_update=*/true, /*old_name=*/extension->name());
  }

  void SetStoredOptions(const Extension* extension, const char* options_json) {
    ExtensionPrefs::Get(browser_context())
        ->UpdateExtensionPref(extension->id(), "mime_handler_options",
                              base::test::ParseJson(options_json));
  }

  const base::DictValue* GetStoredOptions(const Extension* extension) {
    return ExtensionPrefs::Get(browser_context())
        ->ReadPrefAsDict(extension->id(), "mime_handler_options");
  }

  MimeHandlerRegistry* registry() {
    return MimeHandlerRegistry::Get(browser_context());
  }

  // Returns the highest-precedence candidate for `mime_type`, or an empty
  // id when the registry holds none.
  ExtensionId GetHandlerForMimeType(const std::string& mime_type) {
    const std::vector<ExtensionId> handlers =
        registry()->GetHandlersForMimeType(mime_type, browser_context());
    return handlers.empty() ? ExtensionId() : handlers.front();
  }

 private:
  // Channel "trunk" is represented as UNKNOWN in the feature system.
  // Required because the dict-format mime_types_handler manifest key
  // is gated to channel "trunk".
  ScopedCurrentChannel channel_{version_info::Channel::UNKNOWN};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MimeHandlerRegistryTest, RegisterAndLookup) {
  auto ext =
      CreateMimeHandlerExtension("PDF Handler", kPdfMimeType, kViewerUrl);
  LoadExtension(ext.get());

  EXPECT_EQ(ext->id(), GetHandlerForMimeType(kPdfMimeType));
}

TEST_F(MimeHandlerRegistryTest, UnregisterRemovesMapping) {
  auto ext =
      CreateMimeHandlerExtension("PDF Handler", kPdfMimeType, kViewerUrl);
  LoadExtension(ext.get());
  UnloadExtension(ext.get());

  EXPECT_TRUE(GetHandlerForMimeType(kPdfMimeType).empty());
}

TEST_F(MimeHandlerRegistryTest, MostRecentlyInstalledWins) {
  auto ext_a = CreateMimeHandlerExtension("A", kPdfMimeType, "a.html");
  auto ext_b = CreateMimeHandlerExtension("B", kPdfMimeType, "b.html");
  auto ext_c = CreateMimeHandlerExtension("C", kPdfMimeType, "c.html");

  const base::Time t0 = base::Time::Now();
  SetFirstInstallTime(ext_a.get(), t0);
  SetFirstInstallTime(ext_b.get(), t0 + base::Hours(2));
  SetFirstInstallTime(ext_c.get(), t0 + base::Hours(1));

  // Load in a different order than install order. ext_b (newest install)
  // is loaded second, proving neither first-loaded nor last-loaded wins.
  LoadExtension(ext_a.get());
  LoadExtension(ext_b.get());
  LoadExtension(ext_c.get());

  EXPECT_EQ(ext_b->id(), GetHandlerForMimeType(kPdfMimeType));

  // Unload ext_b; ext_c (next-newest install) becomes active.
  UnloadExtension(ext_b.get());
  EXPECT_EQ(ext_c->id(), GetHandlerForMimeType(kPdfMimeType));

  // Unload ext_c; ext_a (oldest install) becomes active.
  UnloadExtension(ext_c.get());
  EXPECT_EQ(ext_a->id(), GetHandlerForMimeType(kPdfMimeType));
}

TEST_F(MimeHandlerRegistryTest, NoHandlerReturnsEmpty) {
  EXPECT_TRUE(GetHandlerForMimeType(kPdfMimeType).empty());
}

TEST_F(MimeHandlerRegistryTest, PublicHandlerSupersedesAllowlisted) {
  // Allowlisted PDF handler, installed AFTER the public handler.
  auto allowlisted =
      CreateAllowlistedHandler(extension_misc::kPdfExtensionId, kPdfMimeType);
  auto public_ext =
      CreateMimeHandlerExtension("PublicPdf", kPdfMimeType, kViewerUrl);

  const base::Time t0 = base::Time::Now();
  SetFirstInstallTime(public_ext.get(), t0);
  SetFirstInstallTime(allowlisted.get(), t0 + base::Hours(1));

  LoadExtension(public_ext.get());
  LoadExtension(allowlisted.get());

  // Public handlers supersede allowlisted ones, regardless of install time.
  EXPECT_EQ(public_ext->id(), GetHandlerForMimeType(kPdfMimeType));
}

TEST_F(MimeHandlerRegistryTest, AllowlistArrayIndexBreaksTies) {
  // PDF is at array index 0; QuickOffice is at a higher index.
  // Higher array index wins, even when installed EARLIER than PDF.
  auto pdf =
      CreateAllowlistedHandler(extension_misc::kPdfExtensionId, kPdfMimeType);
  auto qoffice = CreateAllowlistedHandler(
      extension_misc::kQuickOfficeExtensionId, kPdfMimeType);

  const base::Time t0 = base::Time::Now();
  SetFirstInstallTime(qoffice.get(), t0);
  SetFirstInstallTime(pdf.get(), t0 + base::Hours(1));

  LoadExtension(qoffice.get());
  LoadExtension(pdf.get());

  // Expected: qoffice wins by array index, even though pdf is newer.
  EXPECT_EQ(qoffice->id(), GetHandlerForMimeType(kPdfMimeType));
}

TEST_F(MimeHandlerRegistryTest, GetHandlersByMimeTypeReturnsAllTypes) {
  // Two distinct MIME types. QuickOffice is allowlisted, so it is free to
  // register any MIME type (including a non-public one).
  constexpr char kDocMimeType[] = "application/msword";

  auto pdf_handler =
      CreateMimeHandlerExtension("PdfHandler", kPdfMimeType, "a.html");
  auto doc_handler = CreateAllowlistedHandler(
      extension_misc::kQuickOfficeExtensionId, kDocMimeType);

  LoadExtension(pdf_handler.get());
  LoadExtension(doc_handler.get());

  auto all = registry()->GetHandlersByMimeType(browser_context());
  ASSERT_EQ(all.size(), 2u);

  auto pdf_it = all.find(kPdfMimeType);
  ASSERT_NE(pdf_it, all.end());
  ASSERT_EQ(pdf_it->second.size(), 1u);
  EXPECT_EQ(pdf_it->second.front(), pdf_handler->id());

  auto doc_it = all.find(kDocMimeType);
  ASSERT_NE(doc_it, all.end());
  ASSERT_EQ(doc_it->second.size(), 1u);
  EXPECT_EQ(doc_it->second.front(), doc_handler->id());
}

TEST_F(MimeHandlerRegistryTest, GetHandlersForMimeTypeIsOrdered) {
  // Exercise all three precedence rules in one ordered list:
  //   * public_new and public_old are public handlers; public beats
  //     allowlisted, and newest-install-time wins among public.
  //   * qoffice and pdf are both allowlisted; qoffice has a higher
  //     index in `kMIMETypeHandlersAllowlist` and must beat pdf.
  auto public_new =
      CreateMimeHandlerExtension("PublicNew", kPdfMimeType, "b.html");
  auto public_old =
      CreateMimeHandlerExtension("PublicOld", kPdfMimeType, "a.html");
  auto qoffice = CreateAllowlistedHandler(
      extension_misc::kQuickOfficeExtensionId, kPdfMimeType);
  auto pdf =
      CreateAllowlistedHandler(extension_misc::kPdfExtensionId, kPdfMimeType);

  const base::Time t0 = base::Time::Now();
  SetFirstInstallTime(public_old.get(), t0);
  SetFirstInstallTime(public_new.get(), t0 + base::Hours(1));
  // Install times for allowlisted handlers do not affect precedence;
  // set deterministic values anyway.
  SetFirstInstallTime(qoffice.get(), t0 + base::Hours(2));
  SetFirstInstallTime(pdf.get(), t0 + base::Hours(3));

  LoadExtension(qoffice.get());
  LoadExtension(pdf.get());
  LoadExtension(public_old.get());
  LoadExtension(public_new.get());

  const std::vector<ExtensionId> candidates =
      registry()->GetHandlersForMimeType(kPdfMimeType, browser_context());
  ASSERT_EQ(candidates.size(), 4u);
  EXPECT_EQ(candidates[0], public_new->id());
  EXPECT_EQ(candidates[1], public_old->id());
  EXPECT_EQ(candidates[2], qoffice->id());
  EXPECT_EQ(candidates[3], pdf->id());
}

TEST_F(MimeHandlerRegistryTest, MultipleMimeTypesWithOverlap) {
  constexpr char kDocMimeType[] = "application/msword";

  // Allowlisted QuickOffice extension registers for BOTH application/pdf
  // and application/msword via a dict-format manifest. Allowlisted
  // extensions may claim unsupported MIME types, so msword is accepted.
  auto allowlisted =
      ExtensionBuilder("AllowlistedMulti")
          .SetID(extension_misc::kQuickOfficeExtensionId)
          .AddJSON(R"("mime_types_handler": {)"
                   R"("application/pdf": {"handler_url": "q.html"},)"
                   R"("application/msword": {"handler_url": "q.html"}})")
          .Build();
  auto public_pdf =
      CreateMimeHandlerExtension("PublicPdf", kPdfMimeType, kViewerUrl);

  LoadExtension(allowlisted.get());
  LoadExtension(public_pdf.get());

  // PDF: public beats allowlisted.
  EXPECT_EQ(public_pdf->id(), GetHandlerForMimeType(kPdfMimeType));
  // msword: only the allowlisted extension registers for it.
  EXPECT_EQ(allowlisted->id(), GetHandlerForMimeType(kDocMimeType));

  const std::vector<ExtensionId> pdf_candidates =
      registry()->GetHandlersForMimeType(kPdfMimeType, browser_context());
  ASSERT_EQ(pdf_candidates.size(), 2u);
  EXPECT_EQ(pdf_candidates[0], public_pdf->id());
  EXPECT_EQ(pdf_candidates[1], allowlisted->id());

  const std::vector<ExtensionId> doc_candidates =
      registry()->GetHandlersForMimeType(kDocMimeType, browser_context());
  ASSERT_EQ(doc_candidates.size(), 1u);
  EXPECT_EQ(doc_candidates[0], allowlisted->id());
}

TEST_F(MimeHandlerRegistryTest, AllowlistedExtensionRegisteredWhenUsingDict) {
  auto ext = ExtensionBuilder("Allowlisted PDF")
                 .SetID(extension_misc::kPdfExtensionId)
                 .AddJSON(R"("mime_types_handler": {"application/pdf": )"
                          R"({"handler_url": "viewer.html"}})")
                 .Build();
  LoadExtension(ext.get());
  EXPECT_EQ(ext->id(), GetHandlerForMimeType(kPdfMimeType));
}

TEST_F(MimeHandlerRegistryTest, FlagDisabledRegistrationByChannel) {
  base::test::ScopedFeatureList disable_flag;
  disable_flag.InitAndDisableFeature(extensions_features::kApiMimeHandler);

  for (auto channel :
       {version_info::Channel::UNKNOWN, version_info::Channel::CANARY,
        version_info::Channel::DEV, version_info::Channel::BETA,
        version_info::Channel::STABLE}) {
    SCOPED_TRACE(version_info::GetChannelString(channel));
    ScopedCurrentChannel scoped_channel(channel);
    auto ext = CreateMimeHandlerExtension(
        std::string(version_info::GetChannelString(channel)), kPdfMimeType,
        kViewerUrl);
    LoadExtension(ext.get());
    EXPECT_TRUE(GetHandlerForMimeType(kPdfMimeType).empty());
    UnloadExtension(ext.get());
  }
}

TEST_F(MimeHandlerRegistryTest, FlagDefaultRegistrationByChannel) {
  // Without an explicit override, availability follows the flag's
  // default state with the dev-channel fallback.
  for (auto channel :
       {version_info::Channel::UNKNOWN, version_info::Channel::CANARY,
        version_info::Channel::DEV, version_info::Channel::BETA,
        version_info::Channel::STABLE}) {
    SCOPED_TRACE(testing::Message()
                 << "channel=" << version_info::GetChannelString(channel));
    ScopedCurrentChannel scoped_channel(channel);
    base::test::ScopedFeatureList no_override;
    no_override.InitWithEmptyFeatureAndFieldTrialLists();

    auto ext = CreateMimeHandlerExtension(
        std::string(version_info::GetChannelString(channel)), kPdfMimeType,
        kViewerUrl);
    LoadExtension(ext.get());

    if (ExpectHandlerRegistered(channel)) {
      EXPECT_EQ(ext->id(), GetHandlerForMimeType(kPdfMimeType));
    } else {
      EXPECT_TRUE(GetHandlerForMimeType(kPdfMimeType).empty());
    }

    UnloadExtension(ext.get());
  }
}

TEST_F(MimeHandlerRegistryTest, FlagEnabledRegistrationByChannel) {
  base::test::ScopedFeatureList enable_flag;
  enable_flag.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  for (auto channel :
       {version_info::Channel::UNKNOWN, version_info::Channel::CANARY,
        version_info::Channel::DEV, version_info::Channel::BETA,
        version_info::Channel::STABLE}) {
    SCOPED_TRACE(version_info::GetChannelString(channel));
    ScopedCurrentChannel scoped_channel(channel);
    auto ext = CreateMimeHandlerExtension(
        std::string(version_info::GetChannelString(channel)), kPdfMimeType,
        kViewerUrl);
    LoadExtension(ext.get());
    EXPECT_EQ(ext->id(), GetHandlerForMimeType(kPdfMimeType));
    UnloadExtension(ext.get());
  }
}

TEST_F(MimeHandlerRegistryTest, EnabledByDefaultUntilDisabled) {
  auto ext =
      CreateMimeHandlerExtension("PDF Handler", kPdfMimeType, kViewerUrl);
  LoadExtension(ext.get());

  // Default: enabled, present in every accessor.
  EXPECT_TRUE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                               browser_context()));
  EXPECT_EQ(ext->id(), GetHandlerForMimeType(kPdfMimeType));
  EXPECT_EQ(registry()
                ->GetHandlersForMimeType(kPdfMimeType, browser_context())
                .size(),
            1u);
  EXPECT_EQ(
      registry()->GetHandlersByMimeType(browser_context()).count(kPdfMimeType),
      1u);

  // After disabling: gone from every accessor.
  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, false,
                                    browser_context());
  EXPECT_FALSE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                                browser_context()));
  EXPECT_TRUE(GetHandlerForMimeType(kPdfMimeType).empty());
  EXPECT_TRUE(registry()
                  ->GetHandlersForMimeType(kPdfMimeType, browser_context())
                  .empty());
  EXPECT_EQ(
      registry()->GetHandlersByMimeType(browser_context()).count(kPdfMimeType),
      0u);

  // Re-enable: comes back.
  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, true,
                                    browser_context());
  EXPECT_EQ(ext->id(), GetHandlerForMimeType(kPdfMimeType));
}

TEST_F(MimeHandlerRegistryTest, ManifestEnabledFalseStartsDisabled) {
  auto ext =
      CreateMimeHandlerExtension("PDF Handler", kPdfMimeType, kViewerUrl,
                                 base::DictValue().Set("enabled", false));
  LoadExtension(ext.get());

  EXPECT_TRUE(GetHandlerForMimeType(kPdfMimeType).empty());
  EXPECT_FALSE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                                browser_context()));

  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, true,
                                    browser_context());
  EXPECT_EQ(ext->id(), GetHandlerForMimeType(kPdfMimeType));
  EXPECT_TRUE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                               browser_context()));

  // The PDF handler should stay enabled after the extension is loaded again,
  // because the stored choice wins over the manifest.
  UnloadExtension(ext.get());
  LoadExtension(ext.get());
  EXPECT_EQ(ext->id(), GetHandlerForMimeType(kPdfMimeType));
  EXPECT_TRUE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                               browser_context()));
}

TEST_F(MimeHandlerRegistryTest, DisableRollsBackToPreviouslyInstalledHandler) {
  // Precedence for public handlers is "newest GetFirstInstallTime
  // wins". Disabling the newer handler must roll the active handler
  // back to the previously-installed one; re-enabling must restore
  // the precedence order.
  auto ext_old = CreateMimeHandlerExtension("Older", kPdfMimeType, "a.html");
  auto ext_new = CreateMimeHandlerExtension("Newer", kPdfMimeType, "b.html");
  const base::Time t0 = base::Time::Now();
  SetFirstInstallTime(ext_old.get(), t0);
  SetFirstInstallTime(ext_new.get(), t0 + base::Hours(1));
  LoadExtension(ext_old.get());
  LoadExtension(ext_new.get());
  ASSERT_EQ(ext_new->id(), GetHandlerForMimeType(kPdfMimeType));

  registry()->SetEnabledForMimeType(ext_new->id(), kPdfMimeType, false,
                                    browser_context());
  EXPECT_EQ(ext_old->id(), GetHandlerForMimeType(kPdfMimeType));

  // Re-enable: ext_new wins again (newer install time beats ext_old).
  registry()->SetEnabledForMimeType(ext_new->id(), kPdfMimeType, true,
                                    browser_context());
  EXPECT_EQ(ext_new->id(), GetHandlerForMimeType(kPdfMimeType));
}

// MIME handler choices saved by an older Chrome version should still apply
// after the upgrade.
TEST_F(MimeHandlerRegistryTest, MigratesMimeHandlerEnabledToOptions) {
  const ExtensionId malformed_id =
      CreateMimeHandlerExtension("Malformed", kPdfMimeType, kViewerUrl)->id();
  const ExtensionId migrated_id =
      CreateMimeHandlerExtension("Migrated", kPdfMimeType, kViewerUrl)->id();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  prefs->UpdateExtensionPref(
      malformed_id, "mime_handler_enabled",
      base::test::ParseJson(R"({"application/pdf": "not a bool"})"));
  prefs->UpdateExtensionPref(migrated_id, "mime_handler_enabled",
                             base::test::ParseJson(R"({"application/pdf": false,
                                "application/vnd.ms-excel": true})"));

  // The legacy prefs were written after the fixture created its registry, so
  // create a new registry to migrate them, as on the next browser start.
  MimeHandlerRegistry restarted_registry(browser_context());

  EXPECT_FALSE(prefs->ReadPrefAsDict(malformed_id, "mime_handler_options"));
  // A '.' in a MIME type is not expected to split it into nested keys.
  EXPECT_THAT(prefs->ReadPrefAsDict(migrated_id, "mime_handler_options"),
              testing::Pointee(base::test::IsJson(
                  R"({"application/pdf": {"enabled": false},
                      "application/vnd.ms-excel": {"enabled": true}})")));
  for (const ExtensionId& id : {malformed_id, migrated_id}) {
    EXPECT_FALSE(prefs->ReadPrefAsDict(id, "mime_handler_enabled"));
  }
}

TEST_F(MimeHandlerRegistryTest, ChoicesForSeveralMimeTypesSurviveReload) {
  // A '.' in a MIME type is not expected to split the stored choice into
  // nested keys.
  constexpr char kDottedMimeType[] = "application/vnd.ms-excel";
  auto ext =
      ExtensionBuilder("AllowlistedMulti")
          .SetID(extension_misc::kQuickOfficeExtensionId)
          .AddJSON(R"("mime_types_handler": {)"
                   R"("application/pdf": {"handler_url": "q.html"},)"
                   R"("application/vnd.ms-excel": {"handler_url": "q.html"}})")
          .Build();
  LoadExtension(ext.get());

  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, false,
                                    browser_context());
  registry()->SetEnabledForMimeType(ext->id(), kDottedMimeType, false,
                                    browser_context());
  UnloadExtension(ext.get());
  LoadExtension(ext.get());

  EXPECT_TRUE(GetHandlerForMimeType(kPdfMimeType).empty());
  EXPECT_TRUE(GetHandlerForMimeType(kDottedMimeType).empty());
}

// Stored options of a MIME type the new version no longer declares must not
// come back if a later version declares the type again.
TEST_F(MimeHandlerRegistryTest, UpdateForgetsOptionsOfUndeclaredMimeTypes) {
  struct {
    const char* name;
    scoped_refptr<const Extension> updated_extension;
    const char* stored_options_json;
    const char* expected_options_json;
  } kTestCases[] = {
      // New manifest defaults must not reset what the extension stored.
      {.name = "Type still declared with new manifest defaults",
       .updated_extension = CreateMimeHandlerExtension(
           "Kept", kPdfMimeType, kViewerUrl,
           base::DictValue().Set("enabled", true).Set("can_embed", true)),
       .stored_options_json =
           R"({"application/pdf": {"enabled": false, "canEmbed": false}})",
       .expected_options_json =
           R"({"application/pdf": {"enabled": false, "canEmbed": false}})"},
      {.name = "Type dropped",
       .updated_extension = ExtensionBuilder("Dropped").Build(),
       .stored_options_json =
           R"({"application/pdf": {"enabled": false, "canEmbed": true}})",
       .expected_options_json = "{}"},
      {.name = "Dotted type dropped",
       .updated_extension = CreateAllowlistedHandler(
           extension_misc::kMimeHandlerPrivateTestExtensionId, kPdfMimeType),
       .stored_options_json = R"({"application/pdf": {"enabled": false},
           "application/vnd.ms-excel": {"enabled": false}})",
       .expected_options_json = R"({"application/pdf": {"enabled": false}})"},
      {.name = "Legacy list",
       .updated_extension =
           ExtensionBuilder("Legacy")
               .SetID(extension_misc::kPdfExtensionId)
               .AddJSON(R"("mime_types": ["application/pdf"],)"
                        R"( "mime_types_handler": "index.html")")
               .Build(),
       .stored_options_json = R"({"application/pdf": {"enabled": false},
           "application/msword": {"enabled": false}})",
       .expected_options_json = R"({"application/pdf": {"enabled": false}})"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);
    const Extension* extension = test_case.updated_extension.get();
    SetStoredOptions(extension, test_case.stored_options_json);

    UpdateExtension(extension);

    const base::DictValue* options = GetStoredOptions(extension);
    ASSERT_TRUE(options);
    EXPECT_THAT(*options, base::test::IsJson(test_case.expected_options_json));
  }
}

// For a split mode extension, options set from incognito should change only
// incognito. They should not be saved in the prefs. For a spanning mode
// extension, they should change the regular profile too and be saved.
TEST_F(MimeHandlerRegistryTest,
       IncognitoChangesReachRegularOnlyInSpanningMode) {
  struct {
    const char* name;
    scoped_refptr<const Extension> extension;
    bool expected_enabled_in_regular;
    bool expected_can_embed_in_regular;
  } kTestCases[] = {
      {.name = "Split mode",
       .extension = CreateSplitModePdfHandlerExtension(),
       .expected_enabled_in_regular = true,
       .expected_can_embed_in_regular = false},
      {.name = "Spanning mode",
       .extension =
           CreateMimeHandlerExtension("PDF Handler", kPdfMimeType, kViewerUrl),
       .expected_enabled_in_regular = false,
       .expected_can_embed_in_regular = true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);
    const Extension* ext = test_case.extension.get();
    LoadExtension(ext);
    ASSERT_TRUE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                                 incognito_context()));
    ASSERT_FALSE(registry()->CanEmbedForMimeType(ext->id(), kPdfMimeType,
                                                 incognito_context()));

    registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, false,
                                      incognito_context());
    registry()->SetCanEmbedForMimeType(ext->id(), kPdfMimeType, true,
                                       incognito_context());

    EXPECT_FALSE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                                  incognito_context()));
    EXPECT_TRUE(registry()->CanEmbedForMimeType(ext->id(), kPdfMimeType,
                                                incognito_context()));
    EXPECT_TRUE(registry()
                    ->GetHandlersForMimeType(kPdfMimeType, incognito_context())
                    .empty());
    EXPECT_EQ(registry()
                  ->GetHandlersByMimeType(incognito_context())
                  .count(kPdfMimeType),
              0u);

    EXPECT_EQ(test_case.expected_enabled_in_regular,
              registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                               browser_context()));
    EXPECT_EQ(test_case.expected_can_embed_in_regular,
              registry()->CanEmbedForMimeType(ext->id(), kPdfMimeType,
                                              browser_context()));

    UnloadExtension(ext);
  }
}

// Incognito should use the options set from the regular profile until
// incognito sets its own. An option set from incognito should replace only that
// specific option.
TEST_F(MimeHandlerRegistryTest, IncognitoInheritsRegularOptions) {
  // The manifest values are the opposite of the options set below, so each
  // check shows whether the regular profile or the manifest decided.
  auto ext = CreateSplitModePdfHandlerExtension(
      base::DictValue().Set("enabled", false));
  LoadExtension(ext.get());
  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, true,
                                    browser_context());
  registry()->SetCanEmbedForMimeType(ext->id(), kPdfMimeType, true,
                                     browser_context());

  {
    // Nothing is set from incognito yet.
    EXPECT_THAT(
        registry()->GetHandlersForMimeType(kPdfMimeType, incognito_context()),
        testing::ElementsAre(ext->id()));
    EXPECT_TRUE(registry()->CanEmbedForMimeType(ext->id(), kPdfMimeType,
                                                incognito_context()));
  }

  {
    // After incognito disables the handler, canEmbed in incognito is expected
    // to still come from the regular profile.
    registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, false,
                                      incognito_context());
    EXPECT_TRUE(registry()
                    ->GetHandlersForMimeType(kPdfMimeType, incognito_context())
                    .empty());
    EXPECT_TRUE(registry()->CanEmbedForMimeType(ext->id(), kPdfMimeType,
                                                incognito_context()));
  }
}

// Incognito sets only `enabled`. Later changes in the regular profile should
// show in incognito for canEmbed, but not for `enabled`.
TEST_F(MimeHandlerRegistryTest, IncognitoFollowsLaterRegularChanges) {
  auto ext = CreateSplitModePdfHandlerExtension();
  LoadExtension(ext.get());
  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, false,
                                    incognito_context());
  ASSERT_FALSE(registry()->CanEmbedForMimeType(ext->id(), kPdfMimeType,
                                               incognito_context()));

  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, true,
                                    browser_context());
  registry()->SetCanEmbedForMimeType(ext->id(), kPdfMimeType, true,
                                     browser_context());

  EXPECT_FALSE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                                incognito_context()));
  EXPECT_TRUE(registry()->CanEmbedForMimeType(ext->id(), kPdfMimeType,
                                              incognito_context()));
}

// An update that still handles PDFs should keep the options set from
// incognito.
TEST_F(MimeHandlerRegistryTest, UpdateKeepsIncognitoChanges) {
  auto ext = CreateSplitModePdfHandlerExtension();
  LoadExtension(ext.get());
  ASSERT_TRUE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                               incognito_context()));
  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, false,
                                    incognito_context());

  UnloadExtension(ext.get());
  UpdateExtension(ext.get());
  ExtensionRegistry::Get(browser_context())->TriggerOnLoaded(ext.get());

  EXPECT_FALSE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                                incognito_context()));
}

// An update that stops handling PDFs should delete the PDF options set from
// incognito. A later version that handles PDFs again should start from the
// manifest.
TEST_F(MimeHandlerRegistryTest, UpdateForgetsIncognitoChangesOfDroppedType) {
  auto declares_pdf = CreateSplitModePdfHandlerExtension();
  LoadExtension(declares_pdf.get());
  registry()->SetEnabledForMimeType(declares_pdf->id(), kPdfMimeType, false,
                                    incognito_context());
  ASSERT_FALSE(registry()->IsEnabledForMimeType(
      declares_pdf->id(), kPdfMimeType, incognito_context()));

  UpdateExtension(
      ExtensionBuilder("Dropped").SetID(declares_pdf->id()).Build().get());
  UpdateExtension(declares_pdf.get());

  EXPECT_TRUE(registry()->IsEnabledForMimeType(declares_pdf->id(), kPdfMimeType,
                                               incognito_context()));
}

// Uninstalling the extension should delete the options set from incognito.
TEST_F(MimeHandlerRegistryTest, UninstallForgetsIncognitoChanges) {
  auto ext = CreateSplitModePdfHandlerExtension();
  LoadExtension(ext.get());
  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, false,
                                    incognito_context());
  ASSERT_FALSE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                                incognito_context()));

  UnloadExtension(ext.get());
  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUninstalled(ext.get(), UNINSTALL_REASON_FOR_TESTING);
  LoadExtension(ext.get());

  EXPECT_TRUE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                               incognito_context()));
}

// Ending the incognito session should delete the options set from incognito.
TEST_F(MimeHandlerRegistryTest, IncognitoSessionEndForgetsIncognitoChanges) {
  auto ext = CreateSplitModePdfHandlerExtension();
  LoadExtension(ext.get());
  registry()->SetEnabledForMimeType(ext->id(), kPdfMimeType, false,
                                    incognito_context());
  ASSERT_FALSE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                                incognito_context()));

  // The test browser client has no lifetime tracker, so call the observer
  // method directly.
  registry()->OnRelatedOffTheRecordBrowserContextDestroyed(
      *incognito_context());

  EXPECT_TRUE(registry()->IsEnabledForMimeType(ext->id(), kPdfMimeType,
                                               incognito_context()));
}

}  // namespace
}  // namespace extensions
