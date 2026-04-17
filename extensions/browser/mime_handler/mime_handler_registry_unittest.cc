// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_registry.h"

#include "base/containers/span.h"
#include "base/json/values_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/version_info/channel.h"
#include "extensions/browser/extension_pref_names.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kViewerUrl[] = "viewer.html";

class MimeHandlerRegistryTest : public ExtensionsTest {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(extensions_features::kApiMimeHandler);
    ExtensionsTest::SetUp();
    ASSERT_TRUE(registry());
  }

  // Builds an extension with dict-format mime_types_handler.
  scoped_refptr<const Extension> CreateMimeHandlerExtension(
      const std::string& name,
      const std::string& mime_type,
      const std::string& handler_url) {
    std::string json = base::StringPrintf(
        R"("mime_types_handler": {"%s": {"handler_url": "%s"}})",
        mime_type.c_str(), handler_url.c_str());
    return ExtensionBuilder(name).SetManifestVersion(3).AddJSON(json).Build();
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
        .SetManifestVersion(3)
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

  MimeHandlerRegistry* registry() {
    return MimeHandlerRegistry::Get(browser_context());
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

  EXPECT_EQ(ext->id(), registry()->GetHandlerForMimeType(kPdfMimeType));
}

TEST_F(MimeHandlerRegistryTest, UnregisterRemovesMapping) {
  auto ext =
      CreateMimeHandlerExtension("PDF Handler", kPdfMimeType, kViewerUrl);
  LoadExtension(ext.get());
  UnloadExtension(ext.get());

  EXPECT_TRUE(registry()->GetHandlerForMimeType(kPdfMimeType).empty());
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

  EXPECT_EQ(ext_b->id(), registry()->GetHandlerForMimeType(kPdfMimeType));

  // Unload ext_b; ext_c (next-newest install) becomes active.
  UnloadExtension(ext_b.get());
  EXPECT_EQ(ext_c->id(), registry()->GetHandlerForMimeType(kPdfMimeType));

  // Unload ext_c; ext_a (oldest install) becomes active.
  UnloadExtension(ext_c.get());
  EXPECT_EQ(ext_a->id(), registry()->GetHandlerForMimeType(kPdfMimeType));
}

TEST_F(MimeHandlerRegistryTest, NoHandlerReturnsEmpty) {
  EXPECT_TRUE(registry()->GetHandlerForMimeType(kPdfMimeType).empty());
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
  EXPECT_EQ(public_ext->id(), registry()->GetHandlerForMimeType(kPdfMimeType));
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
  EXPECT_EQ(qoffice->id(), registry()->GetHandlerForMimeType(kPdfMimeType));
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

  const auto& all = registry()->GetHandlersByMimeType();
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
  // Exercise all three precedence rules in one ordered span:
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

  base::span<const ExtensionId> candidates =
      registry()->GetHandlersForMimeType(kPdfMimeType);
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
  // extensions bypass `GetPublicAllowedMIMETypeList()`, so msword is
  // accepted.
  auto allowlisted =
      ExtensionBuilder("AllowlistedMulti")
          .SetManifestVersion(3)
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
  EXPECT_EQ(public_pdf->id(), registry()->GetHandlerForMimeType(kPdfMimeType));
  // msword: only the allowlisted extension registers for it.
  EXPECT_EQ(allowlisted->id(), registry()->GetHandlerForMimeType(kDocMimeType));

  base::span<const ExtensionId> pdf_candidates =
      registry()->GetHandlersForMimeType(kPdfMimeType);
  ASSERT_EQ(pdf_candidates.size(), 2u);
  EXPECT_EQ(pdf_candidates[0], public_pdf->id());
  EXPECT_EQ(pdf_candidates[1], allowlisted->id());

  base::span<const ExtensionId> doc_candidates =
      registry()->GetHandlersForMimeType(kDocMimeType);
  ASSERT_EQ(doc_candidates.size(), 1u);
  EXPECT_EQ(doc_candidates[0], allowlisted->id());
}

TEST_F(MimeHandlerRegistryTest, AllowlistedExtensionRegisteredWhenUsingDict) {
  auto ext = ExtensionBuilder("Allowlisted PDF")
                 .SetManifestVersion(3)
                 .SetID(extension_misc::kPdfExtensionId)
                 .AddJSON(R"("mime_types_handler": {"application/pdf": )"
                          R"({"handler_url": "viewer.html"}})")
                 .Build();
  LoadExtension(ext.get());
  EXPECT_EQ(ext->id(), registry()->GetHandlerForMimeType(kPdfMimeType));
}

TEST_F(MimeHandlerRegistryTest, FlagDisabledNoRegistration) {
  // Override the feature flag to disabled (the fixture enables it in
  // SetUp, so we need a new ScopedFeatureList that takes precedence).
  base::test::ScopedFeatureList disable_flag;
  disable_flag.InitAndDisableFeature(extensions_features::kApiMimeHandler);

  // Create and load extension with flag disabled -- the manifest parser
  // produces no handler data (graceful degradation).
  auto ext = CreateMimeHandlerExtension("Disabled", kPdfMimeType, kViewerUrl);
  LoadExtension(ext.get());

  EXPECT_TRUE(registry()->GetHandlerForMimeType(kPdfMimeType).empty());
}

}  // namespace
}  // namespace extensions
