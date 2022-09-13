// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "extensions/common/api/oauth2.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::api::oauth2::OAuth2Info;
using extensions::mojom::ManifestLocation;

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

using manifest_handler_helpers::TokenizeDictionaryPath;

// Produces extension ID = "mdbihdcgjmagbcapkhhkjbbdlkflmbfo".
const char kExtensionKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCV9PlZjcTIXfnlB3HXo50OlM/CnIq0y7jm"
    "KfPVyStaWsmFB7NaVnqUXoGb9swBDfVnZ6BrupwnxL76TWEJPo+KQMJ6uz0PPdJWi2jQfZiG"
    "iheDiKH5Gv+dVd67qf7ly8QWW0o8qmFpqBZQpksm1hOGbfsupv9W4c42tMEIicDMLQIDAQAB";
const char kAutoApproveNotAllowedWarning[] =
    "'oauth2.auto_approve' is not allowed for specified extension ID.";

std::vector<base::StringPiece> GetOauth2KeyPath(base::StringPiece sub_key) {
  return std::vector<base::StringPiece>(
      {api::oauth2::ManifestKeys::kOauth2, sub_key});
}

}  // namespace

class OAuth2ManifestTest : public ManifestTest {
 protected:
  enum AutoApproveValue {
    AUTO_APPROVE_NOT_SET,
    AUTO_APPROVE_FALSE,
    AUTO_APPROVE_TRUE,
    AUTO_APPROVE_INVALID
  };

  enum ClientIdValue {
    CLIENT_ID_DEFAULT,
    CLIENT_ID_NOT_SET,
    CLIENT_ID_EMPTY
  };

  base::Value CreateManifest(AutoApproveValue auto_approve,
                             bool extension_id_allowlisted,
                             ClientIdValue client_id) {
    base::Value manifest = base::test::ParseJson(R"({
          "name": "test",
          "version": "0.1",
          "manifest_version": 2,
          "oauth2": {
            "scopes": [ "scope1" ],
          },
        })");
    EXPECT_TRUE(manifest.is_dict());
    switch (auto_approve) {
      case AUTO_APPROVE_NOT_SET:
        break;
      case AUTO_APPROVE_FALSE:
        manifest.SetPath(GetOauth2KeyPath(OAuth2Info::kAutoApprove),
                         base::Value(false));
        break;
      case AUTO_APPROVE_TRUE:
        manifest.SetPath(GetOauth2KeyPath(OAuth2Info::kAutoApprove),
                         base::Value(true));
        break;
      case AUTO_APPROVE_INVALID:
        manifest.SetPath(GetOauth2KeyPath(OAuth2Info::kAutoApprove),
                         base::Value("incorrect value"));
        break;
    }
    switch (client_id) {
      case CLIENT_ID_DEFAULT:
        manifest.SetPath(GetOauth2KeyPath(OAuth2Info::kClientId),
                         base::Value("client1"));
        break;
      case CLIENT_ID_NOT_SET:
        break;
      case CLIENT_ID_EMPTY:
        manifest.SetPath(GetOauth2KeyPath(OAuth2Info::kClientId),
                         base::Value(""));
    }
    if (extension_id_allowlisted) {
      manifest.SetPath(TokenizeDictionaryPath(keys::kKey),
                       base::Value(kExtensionKey));
    }
    return manifest;
  }
};

TEST_F(OAuth2ManifestTest, OAuth2SectionParsing) {
  base::Value base_manifest(base::Value::Type::DICTIONARY);

  base_manifest.SetPath(TokenizeDictionaryPath(keys::kName),
                        base::Value("test"));
  base_manifest.SetPath(TokenizeDictionaryPath(keys::kVersion),
                        base::Value("0.1"));
  base_manifest.SetPath(TokenizeDictionaryPath(keys::kManifestVersion),
                        base::Value(2));
  base_manifest.SetPath(GetOauth2KeyPath(OAuth2Info::kClientId),
                        base::Value("client1"));
  base::Value scopes(base::Value::Type::LIST);
  scopes.Append(base::Value("scope1"));
  scopes.Append(base::Value("scope2"));
  base_manifest.SetPath(GetOauth2KeyPath(OAuth2Info::kScopes),
                        std::move(scopes));

  // OAuth2 section should be parsed for an extension.
  {
    base::Value ext_manifest(base::Value::Type::DICTIONARY);
    // Lack of "app" section representa an extension. So the base manifest
    // itself represents an extension.
    ext_manifest.MergeDictionary(&base_manifest);
    ext_manifest.SetPath(TokenizeDictionaryPath(keys::kKey),
                         base::Value(kExtensionKey));
    ext_manifest.SetPath(GetOauth2KeyPath(OAuth2Info::kAutoApprove),
                         base::Value(true));

    ManifestData manifest(std::move(ext_manifest), "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());

    const auto& info = OAuth2ManifestHandler::GetOAuth2Info(*extension);
    ASSERT_TRUE(info.client_id);
    EXPECT_EQ("client1", *info.client_id);
    EXPECT_THAT(info.scopes, ::testing::ElementsAre("scope1", "scope2"));
    EXPECT_TRUE(info.auto_approve);
    EXPECT_TRUE(*info.auto_approve);
  }

  // OAuth2 section should be parsed for a packaged app.
  {
    base::Value app_manifest(base::Value::Type::DICTIONARY);
    app_manifest.SetPath(TokenizeDictionaryPath(keys::kLaunchLocalPath),
                         base::Value("launch.html"));
    app_manifest.MergeDictionary(&base_manifest);

    ManifestData manifest(std::move(app_manifest), "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());

    const auto& info = OAuth2ManifestHandler::GetOAuth2Info(*extension);
    ASSERT_TRUE(info.client_id);
    EXPECT_EQ("client1", *info.client_id);
    EXPECT_THAT(info.scopes, ::testing::ElementsAre("scope1", "scope2"));
    EXPECT_FALSE(info.auto_approve);
  }

  // OAuth2 section should NOT be parsed for a hosted app.
  {
    base::Value app_manifest(base::Value::Type::DICTIONARY);
    app_manifest.SetPath(TokenizeDictionaryPath(keys::kLaunchWebURL),
                         base::Value("http://www.google.com"));
    app_manifest.MergeDictionary(&base_manifest);

    ManifestData manifest(std::move(app_manifest), "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_EQ(1U, extension->install_warnings().size());
    const extensions::InstallWarning& warning =
        extension->install_warnings()[0];
    EXPECT_EQ("'oauth2' is only allowed for extensions, legacy packaged apps, "
                  "and packaged apps, but this is a hosted app.",
              warning.message);

    const auto& info = OAuth2ManifestHandler::GetOAuth2Info(*extension);
    EXPECT_FALSE(info.client_id);
    EXPECT_TRUE(info.scopes.empty());
    EXPECT_FALSE(info.auto_approve);
  }
}

TEST_F(OAuth2ManifestTest, AutoApproveNotSetExtensionNotOnAllowlist) {
  base::Value ext_manifest =
      CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_DEFAULT);
  ManifestData manifest(std::move(ext_manifest), "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_FALSE(OAuth2ManifestHandler::GetOAuth2Info(*extension).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveFalseExtensionNotOnAllowlist) {
  base::Value ext_manifest =
      CreateManifest(AUTO_APPROVE_FALSE, false, CLIENT_ID_DEFAULT);
  ManifestData manifest(std::move(ext_manifest), "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_EQ(1U, extension->install_warnings().size());
  const extensions::InstallWarning& warning =
      extension->install_warnings()[0];
  EXPECT_EQ(kAutoApproveNotAllowedWarning, warning.message);
  EXPECT_FALSE(OAuth2ManifestHandler::GetOAuth2Info(*extension).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveTrueExtensionNotOnAllowlist) {
  base::Value ext_manifest =
      CreateManifest(AUTO_APPROVE_TRUE, false, CLIENT_ID_DEFAULT);
  ManifestData manifest(std::move(ext_manifest), "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_EQ(1U, extension->install_warnings().size());
  const extensions::InstallWarning& warning =
      extension->install_warnings()[0];
  EXPECT_EQ(kAutoApproveNotAllowedWarning, warning.message);
  EXPECT_FALSE(OAuth2ManifestHandler::GetOAuth2Info(*extension).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveInvalidExtensionNotOnAllowlist) {
  base::Value ext_manifest =
      CreateManifest(AUTO_APPROVE_INVALID, false, CLIENT_ID_DEFAULT);
  ManifestData manifest(std::move(ext_manifest), "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_EQ(1U, extension->install_warnings().size());
  const extensions::InstallWarning& warning =
      extension->install_warnings()[0];
  EXPECT_EQ(kAutoApproveNotAllowedWarning, warning.message);
  EXPECT_FALSE(OAuth2ManifestHandler::GetOAuth2Info(*extension).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveNotSetExtensionOnAllowlist) {
  base::Value ext_manifest =
      CreateManifest(AUTO_APPROVE_NOT_SET, true, CLIENT_ID_DEFAULT);
  ManifestData manifest(std::move(ext_manifest), "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_FALSE(OAuth2ManifestHandler::GetOAuth2Info(*extension).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveFalseExtensionOnAllowlist) {
  base::Value ext_manifest =
      CreateManifest(AUTO_APPROVE_FALSE, true, CLIENT_ID_DEFAULT);
  ManifestData manifest(std::move(ext_manifest), "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_TRUE(extension->install_warnings().empty());
  ASSERT_TRUE(OAuth2ManifestHandler::GetOAuth2Info(*extension).auto_approve);
  EXPECT_FALSE(*OAuth2ManifestHandler::GetOAuth2Info(*extension).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveTrueExtensionOnAllowlist) {
  base::Value ext_manifest =
      CreateManifest(AUTO_APPROVE_TRUE, true, CLIENT_ID_DEFAULT);
  ManifestData manifest(std::move(ext_manifest), "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_TRUE(OAuth2ManifestHandler::GetOAuth2Info(*extension).auto_approve);
  EXPECT_TRUE(*OAuth2ManifestHandler::GetOAuth2Info(*extension).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveInvalidExtensionOnAllowlist) {
  base::Value ext_manifest =
      CreateManifest(AUTO_APPROVE_INVALID, true, CLIENT_ID_DEFAULT);
  ManifestData manifest(std::move(ext_manifest), "test");
  std::string error;
  scoped_refptr<extensions::Extension> extension =
      LoadExtension(manifest, &error);
  EXPECT_EQ(
      "Error at key 'oauth2.auto_approve'. Type is invalid. Expected boolean, "
      "found string.",
      error);
}

TEST_F(OAuth2ManifestTest, InvalidClientId) {
  {
    base::Value ext_manifest =
        CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_NOT_SET);
    ManifestData manifest(std::move(ext_manifest), "test");
    std::string error;
    LoadAndExpectError(manifest, errors::kInvalidOAuth2ClientId);
  }

  {
    base::Value ext_manifest =
        CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_EMPTY);
    ManifestData manifest(std::move(ext_manifest), "test");
    std::string error;
    LoadAndExpectError(manifest, errors::kInvalidOAuth2ClientId);
  }
}

TEST_F(OAuth2ManifestTest, ComponentInvalidClientId) {
  // Component Apps without auto_approve must include a client ID.
  {
    base::Value ext_manifest =
        CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_NOT_SET);
    ManifestData manifest(std::move(ext_manifest), "test");
    std::string error;
    LoadAndExpectError(manifest, errors::kInvalidOAuth2ClientId,
                       ManifestLocation::kComponent);
  }

  {
    base::Value ext_manifest =
        CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_EMPTY);
    ManifestData manifest(std::move(ext_manifest), "test");
    std::string error;
    LoadAndExpectError(manifest, errors::kInvalidOAuth2ClientId,
                       ManifestLocation::kComponent);
  }
}

TEST_F(OAuth2ManifestTest, ComponentWithChromeClientId) {
  {
    base::Value ext_manifest =
        CreateManifest(AUTO_APPROVE_TRUE, true, CLIENT_ID_NOT_SET);
    ManifestData manifest(std::move(ext_manifest), "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest, ManifestLocation::kComponent);
    EXPECT_FALSE(OAuth2ManifestHandler::GetOAuth2Info(*extension).client_id);
  }

  {
    base::Value ext_manifest =
        CreateManifest(AUTO_APPROVE_TRUE, true, CLIENT_ID_EMPTY);
    ManifestData manifest(std::move(ext_manifest), "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest, ManifestLocation::kComponent);
    ASSERT_TRUE(OAuth2ManifestHandler::GetOAuth2Info(*extension).client_id);
    EXPECT_TRUE(
        OAuth2ManifestHandler::GetOAuth2Info(*extension).client_id->empty());
  }
}

TEST_F(OAuth2ManifestTest, ComponentWithStandardClientId) {
  base::Value ext_manifest =
      CreateManifest(AUTO_APPROVE_TRUE, true, CLIENT_ID_DEFAULT);
  ManifestData manifest(std::move(ext_manifest), "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest, ManifestLocation::kComponent);
  ASSERT_TRUE(OAuth2ManifestHandler::GetOAuth2Info(*extension).client_id);
  EXPECT_EQ("client1",
            *OAuth2ManifestHandler::GetOAuth2Info(*extension).client_id);
}

}  // namespace extensions
