// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
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
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("content_capabilities_subdomain_wildcard.json"));
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
  scoped_refptr<Extension> extension(LoadAndExpectWarning(
      "content_capabilities_all_hosts.json",
      manifest_errors::kInvalidContentCapabilitiesMatchOrigin));
  const ContentCapabilitiesInfo& info = ContentCapabilitiesInfo::Get(
      extension.get());
  // Make sure the wildcard is not included in the pattern set.
  EXPECT_FALSE(info.url_patterns.MatchesURL(GURL("https://nonspecific.com/")));
  EXPECT_TRUE(info.url_patterns.MatchesURL(GURL("https://example.com/")));
}

TEST_F(ContentCapabilitiesManifestTest, RejectedETLDWildcard) {
  // Note: We use LoadExtension() here (instead of
  // LoadExtensionAndExpectWarning()) because we expect multiple warnings, and
  // LoadExtensionAndExpectWarning() only expects one. We manually verify the
  // warnings.
  std::string error;
  scoped_refptr<Extension> extension(LoadExtension(
      ManifestData("content_capabilities_etld_wildcard.json"), &error));
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
  scoped_refptr<Extension> extension(LoadAndExpectWarning(
      "content_capabilities_invalid_permission.json",
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

TEST_F(ContentCapabilitiesManifestTest, InvalidValue) {
  LoadAndExpectError("content_capabilities_invalid_value.json",
                     "expected dictionary, got list");
}

TEST_F(ContentCapabilitiesManifestTest, RejectNonHttpsUrlPatterns) {
  LoadAndExpectError("content_capabilities_non_https_matches.json",
                     manifest_errors::kInvalidContentCapabilitiesMatch);
}

TEST_F(ContentCapabilitiesManifestTest, Valid) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("content_capabilities_valid.json"));
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
