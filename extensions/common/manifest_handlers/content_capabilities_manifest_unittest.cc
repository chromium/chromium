// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/content_capabilities_handler.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/extensions_api_permissions.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

class ContentCapabilitiesManifestTest : public ManifestTest {
  std::string GetTestExtensionID() const override {
    return std::string("apdfllckaahabafndbhieahigkjlhalf");
  }
};

TEST_F(ContentCapabilitiesManifestTest, AllowSubdomainWildcards) {
  constexpr char kSubdomainWildcard[] =
      R"("content_capabilities": {
           "matches": [
             "https://*.example.com/"
           ],
           "permissions": [
             "clipboardRead",
             "clipboardWrite",
             "unlimitedStorage"
           ]
         })";
  base::Value manifest = ExtensionBuilder("subdomain wildcard")
                             .AddJSON(kSubdomainWildcard)
                             .BuildManifest();
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData(std::move(manifest).TakeDict()));
  const ContentCapabilitiesInfo& info =
      ContentCapabilitiesInfo::Get(extension.get());
  // Make sure the wildcard subdomain is included in the pattern set.
  EXPECT_TRUE(info.url_patterns.MatchesURL(GURL("https://example.com/")));
  EXPECT_TRUE(info.url_patterns.MatchesURL(GURL("https://bar.example.com/")));
  EXPECT_TRUE(
      info.url_patterns.MatchesURL(GURL("https://foo.bar.example.com/")));
  EXPECT_FALSE(info.url_patterns.MatchesURL(GURL("https://chromium.org/")));
}

TEST_F(ContentCapabilitiesManifestTest, RejectedAllHosts) {
  constexpr char kAllHosts[] =
      R"("content_capabilities": {
           "matches": [
             "https://*.com/",
             "https://example.com/"
           ],
           "permissions": [
             "clipboardRead",
             "clipboardWrite",
             "unlimitedStorage"
           ]
         })";
  base::Value manifest =
      ExtensionBuilder("all hosts").AddJSON(kAllHosts).BuildManifest();
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      ManifestData(std::move(manifest).TakeDict()),
      manifest_errors::kInvalidContentCapabilitiesMatchOrigin);
  const ContentCapabilitiesInfo& info = ContentCapabilitiesInfo::Get(
      extension.get());
  // Make sure the wildcard is not included in the pattern set.
  EXPECT_FALSE(info.url_patterns.MatchesURL(GURL("https://nonspecific.com/")));
  EXPECT_TRUE(info.url_patterns.MatchesURL(GURL("https://example.com/")));
}

TEST_F(ContentCapabilitiesManifestTest, RejectedETLDWildcard) {
  constexpr char kEtldWildcard[] =
      R"("content_capabilities": {
           "matches": [
             "https://*.co.uk/",
             "https://*.appspot.com/",
             "<all_urls>",
             "https://example.com/"
           ],
           "permissions": [
             "clipboardRead",
             "clipboardWrite",
             "unlimitedStorage"
           ]
         })";

  // Note: We use LoadExtension() here (instead of
  // LoadExtensionAndExpectWarning()) because we expect multiple warnings, and
  // LoadExtensionAndExpectWarning() only expects one. We manually verify the
  // warnings.
  base::Value manifest =
      ExtensionBuilder("etld wildcard").AddJSON(kEtldWildcard).BuildManifest();
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension(ManifestData(std::move(manifest).TakeDict()), &error);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(error.empty());
  // 3 bad patterns: *.co.uk, *.appspot.com, <all_urls>.
  size_t kNumExpectedWarnings = 3;
  ASSERT_EQ(kNumExpectedWarnings, extension->install_warnings().size());
  for (size_t i = 0; i < kNumExpectedWarnings; ++i) {
    EXPECT_EQ(manifest_errors::kInvalidContentCapabilitiesMatchOrigin,
              extension->install_warnings()[i].message);
  }

  const ContentCapabilitiesInfo& info =
      ContentCapabilitiesInfo::Get(extension.get());
  // Make sure the wildcard is not included in the pattern set.
  EXPECT_FALSE(info.url_patterns.MatchesURL(GURL("https://example.co.uk/")));
  EXPECT_FALSE(
      info.url_patterns.MatchesURL(GURL("https://example.appspot.com/")));
  EXPECT_TRUE(info.url_patterns.MatchesURL(GURL("https://example.com/")));
}

TEST_F(ContentCapabilitiesManifestTest, InvalidPermission) {
  constexpr char kInvalidPermission[] =
      R"("content_capabilities": {
           "matches": [
             "https://valid.example.com/"
           ],
           "permissions": [
             "clipboardRead",
             "clipboardWrite",
             "unlimitedStorage",
             "usb"
           ]
         })";

  base::Value manifest = ExtensionBuilder("invalid permission")
                             .AddJSON(kInvalidPermission)
                             .BuildManifest();
  scoped_refptr<Extension> extension(LoadAndExpectWarning(
      ManifestData(std::move(manifest).TakeDict()),
      manifest_errors::kInvalidContentCapabilitiesPermission));
  const ContentCapabilitiesInfo& info = ContentCapabilitiesInfo::Get(
      extension.get());
  // Make sure the invalid permission is not included in the permission set.
  EXPECT_EQ(3u, info.permissions.size());
  EXPECT_EQ(1u, info.permissions.count(APIPermissionID::kClipboardRead));
  EXPECT_EQ(1u, info.permissions.count(APIPermissionID::kClipboardWrite));
  EXPECT_EQ(1u, info.permissions.count(APIPermissionID::kUnlimitedStorage));
  EXPECT_EQ(0u, info.permissions.count(APIPermissionID::kUsb));
}

TEST_F(ContentCapabilitiesManifestTest, RejectNonHttpsUrlPatterns) {
  constexpr char kNonHttpsMatches[] =
      R"("content_capabilities": {
           "matches": [
             "http://valid.example.com/"
           ],
           "permissions": [
             "clipboardRead",
             "clipboardWrite",
             "unlimitedStorage"
           ]
         })";
  base::Value manifest = ExtensionBuilder("non https matches")
                             .AddJSON(kNonHttpsMatches)
                             .BuildManifest();
  LoadAndExpectError(ManifestData(std::move(manifest).TakeDict()),
                     manifest_errors::kInvalidContentCapabilitiesMatch);
}

TEST_F(ContentCapabilitiesManifestTest, Valid) {
  constexpr char kValid[] =
      R"("content_capabilities": {
           "matches": [
             "https://valid.example.com/"
           ],
           "permissions": [
             "clipboardRead",
             "clipboardWrite",
             "unlimitedStorage"
           ]
         })";
  base::Value manifest =
      ExtensionBuilder("valid").AddJSON(kValid).BuildManifest();
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData(std::move(manifest).TakeDict()));
  const ContentCapabilitiesInfo& info = ContentCapabilitiesInfo::Get(
      extension.get());
  EXPECT_EQ(1u, info.url_patterns.size());
  EXPECT_FALSE(info.url_patterns.MatchesURL(GURL("http://valid.example.com/")));
  EXPECT_FALSE(info.url_patterns.MatchesURL(GURL("https://foo.example.com/")));
  EXPECT_FALSE(info.url_patterns.MatchesURL(GURL("https://example.com/")));
  EXPECT_TRUE(info.url_patterns.MatchesURL(GURL("https://valid.example.com/")));
  EXPECT_EQ(3u, info.permissions.size());
  EXPECT_EQ(1u, info.permissions.count(APIPermissionID::kClipboardRead));
  EXPECT_EQ(1u, info.permissions.count(APIPermissionID::kClipboardWrite));
  EXPECT_EQ(1u, info.permissions.count(APIPermissionID::kUnlimitedStorage));
  EXPECT_EQ(0u, info.permissions.count(APIPermissionID::kUsb));
}

}  // namespace extensions
