// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/mime_types_handler.h"

#include <algorithm>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;

constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kTextPlainMimeType[] = "text/plain";

using MimeTypesHandlerNotAllowedTest = ManifestTest;

class MimeTypesHandlerTest : public ManifestTest {
 protected:
  ExtensionId GetTestExtensionID() const override {
    // Extension ID must correspond to a hashed extension ID in the allowlist
    // for "mime_types" and "mime_types_handler" in _manifest_features.json.
    return extension_misc::kMimeHandlerPrivateTestExtensionId;
  }
};

}  // namespace

TEST_F(MimeTypesHandlerNotAllowedTest, UnableToLoadLegacy) {
  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types": ["text/plain", "application/octet-stream"],
    "mime_types_handler": "index.html"
  })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension);

  EXPECT_FALSE(MimeTypesHandler::Get(*extension));
}

TEST_F(MimeTypesHandlerNotAllowedTest, DictFormatWarnsOnDisallowedMimeType) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "text/plain": {
        "handler_url": "viewer.html"
      },
      "application/pdf": {
         "handler_url": "pdf_viewer.html",
         "can_embed": true
      }
    }
  })";
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      ManifestData::FromJSON(kManifest),
      "mime_types_handler: ignoring unsupported MIME type 'text/plain'.");
  ASSERT_TRUE(extension);

  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  ASSERT_TRUE(handler);
  EXPECT_FALSE(handler->GetHandlerUrl(kTextPlainMimeType).is_valid());
  EXPECT_TRUE(handler->GetHandlerUrl(kPdfMimeType).is_valid());
  // One warning for the skipped entry.
  ASSERT_EQ(1u, extension->install_warnings().size());
  EXPECT_THAT(extension->install_warnings()[0].message,
              HasSubstr("text/plain"));
}

TEST_F(MimeTypesHandlerTest, LoadLegacy) {
  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types": ["text/plain", "application/octet-stream"],
    "mime_types_handler": "index.html"
  })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension);

  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  ASSERT_TRUE(handler);
  EXPECT_THAT(handler->GetSupportedMimeTypes(),
              ElementsAre("application/octet-stream", "text/plain"));

  EXPECT_FALSE(handler->GetHandlerUrl("text/html").is_valid());

  // Legacy format populates per-type configs with the shared handler URL.
  EXPECT_EQ(extension->GetResourceURL("index.html"),
            handler->GetHandlerUrl(kTextPlainMimeType));
  EXPECT_EQ(extension->GetResourceURL("index.html"),
            handler->GetHandlerUrl("application/octet-stream"));
  EXPECT_FALSE(handler->CanEmbedMimeType(kTextPlainMimeType));
  EXPECT_TRUE(handler->HasPlugin());
}

TEST(MimeTypesHandlerUnitTest, PublicAllowedMIMETypeList) {
  const auto& list = MimeTypesHandler::GetPublicAllowedMIMETypeList();
  EXPECT_FALSE(list.empty());
  EXPECT_TRUE(std::ranges::contains(list, kPdfMimeType));
  // Inline subresource types must NOT be in the list.
  EXPECT_FALSE(std::ranges::contains(list, "image/png"));
}

TEST_F(MimeTypesHandlerTest, DictFormatParsing) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "application/pdf": {
        "handler_url": "pdf_viewer.html",
        "can_embed": true
      }
    }
  })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension);

  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  ASSERT_TRUE(handler);

  EXPECT_EQ(extension->GetResourceURL("pdf_viewer.html"),
            handler->GetHandlerUrl(kPdfMimeType));
  EXPECT_TRUE(handler->CanEmbedMimeType(kPdfMimeType));

  // Unknown type returns empty GURL.
  EXPECT_FALSE(handler->GetHandlerUrl(kTextPlainMimeType).is_valid());
  EXPECT_TRUE(handler->GetHandlerUrl(kTextPlainMimeType).is_empty());
  EXPECT_FALSE(handler->CanEmbedMimeType(kTextPlainMimeType));
}

TEST_F(MimeTypesHandlerTest, DictFormatDisabledFlag) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "application/pdf": {
        "handler_url": "pdf_viewer.html",
        "can_embed": true
      }
    }
  })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension);

  // Handler should be null because dict format requires the flag.
  EXPECT_FALSE(MimeTypesHandler::Get(*extension));
}

TEST_F(MimeTypesHandlerTest, DictFormatWarnsOnEmptyMimeType) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "": {
        "handler_url": "viewer.html"
      }
    }
  })";
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      ManifestData::FromJSON(kManifest),
      "mime_types_handler: ignoring entry with empty MIME type key.");
  ASSERT_TRUE(extension);

  EXPECT_FALSE(MimeTypesHandler::Get(*extension));
}

TEST_F(MimeTypesHandlerTest, DictFormatWarnsOnAbsoluteHandlerUrl) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "application/pdf": {
        "handler_url": "https://evil.com/viewer.html"
      }
    }
  })";
  scoped_refptr<Extension> extension =
      LoadAndExpectWarning(ManifestData::FromJSON(kManifest),
                           "mime_types_handler: ignoring entry for "
                           "'application/pdf': invalid handler_url.");
  ASSERT_TRUE(extension);

  EXPECT_FALSE(MimeTypesHandler::Get(*extension));
}

TEST_F(MimeTypesHandlerTest, DictFormatWarnsOnJavascriptHandlerUrl) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"json({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "application/pdf": {
        "handler_url": "javascript:alert(1)"
      }
    }
  })json";
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      ManifestData::FromJSON(kManifest),
      "mime_types_handler: ignoring entry for 'application/pdf': invalid "
      "handler_url.");
  ASSERT_TRUE(extension);

  EXPECT_FALSE(MimeTypesHandler::Get(*extension));
}

TEST_F(MimeTypesHandlerTest, DictFormatRejectsNonDictConfig) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "application/pdf": "viewer.html"
    }
  })";
  LoadAndExpectError(ManifestData::FromJSON(kManifest),
                     manifest_errors::kInvalidMimeTypesHandler);
}

TEST_F(MimeTypesHandlerTest, DictFormatRejectsMissingHandlerUrl) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "application/pdf": {
        "can_embed": true
      }
    }
  })";
  LoadAndExpectError(ManifestData::FromJSON(kManifest),
                     manifest_errors::kInvalidMimeTypesHandler);
}

TEST_F(MimeTypesHandlerTest, DictFormatWarnsOnEmptyHandlerUrl) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "application/pdf": {
        "handler_url": ""
      }
    }
  })";
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      ManifestData::FromJSON(kManifest),
      "mime_types_handler: ignoring entry for 'application/pdf': empty "
      "handler_url.");
  ASSERT_TRUE(extension);

  EXPECT_FALSE(MimeTypesHandler::Get(*extension));
}

TEST_F(MimeTypesHandlerTest, DictFormatWarnsOnDataHandlerUrl) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(extensions_features::kApiMimeHandler);

  static constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "manifest_version": 3,
    "version": "0.1",
    "mime_types_handler": {
      "application/pdf": {
        "handler_url": "data:text/html,<h1>test</h1>"
      }
    }
  })";
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      ManifestData::FromJSON(kManifest),
      "mime_types_handler: ignoring entry for 'application/pdf': invalid "
      "handler_url.");
  ASSERT_TRUE(extension);

  EXPECT_FALSE(MimeTypesHandler::Get(*extension));
}

}  // namespace extensions
