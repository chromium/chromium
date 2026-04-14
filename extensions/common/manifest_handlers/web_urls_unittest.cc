// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

namespace errors = manifest_errors;

using HostedAppWebURLsManifestTest = ManifestTest;

TEST_F(HostedAppWebURLsManifestTest, EmptyApp) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for empty app object",
         "manifest_version": 3,
         "version": "1",
         "app": {}
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_TRUE(extension->is_legacy_packaged_app());
  EXPECT_TRUE(extension->web_extent().is_empty());
}

TEST_F(HostedAppWebURLsManifestTest, AppWithEmptyURLs) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for empty app.urls list",
         "manifest_version": 3,
         "version": "1",
         "app": {
           "urls": []
         }
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_TRUE(extension->web_extent().is_empty());
}

// TODO(crbug.com/324534603): Loading this manifest should fail.
TEST_F(HostedAppWebURLsManifestTest, InvalidApp) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for invalid app number",
         "manifest_version": 3,
         "version": "1",
         "app": 42
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_TRUE(extension->is_legacy_packaged_app());
  EXPECT_TRUE(extension->web_extent().is_empty());
}

TEST_F(HostedAppWebURLsManifestTest, InvalidAppURL) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for invalid app.urls number",
         "manifest_version": 3,
         "version": "1",
         "app": {
           "urls": 42
         }
       })";
  LoadAndExpectError(ManifestData::FromJSON(kManifest),
                     errors::kInvalidWebURLs);
}

TEST_F(HostedAppWebURLsManifestTest, InvalidAppURLs) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for invalid number element in app.urls list",
         "manifest_version": 3,
         "version": "1",
         "app": {
           "urls": [
             "https://example.com",
             42
           ]
         }
       })";
  LoadAndExpectError(
      ManifestData::FromJSON(kManifest),
      ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidWebURL, /*s1=*/"1",
                                          errors::kExpectString));
}

TEST_F(HostedAppWebURLsManifestTest, InvalidAllURLs) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for invalid '<all_urls>' in app.urls list",
         "manifest_version": 3,
         "version": "1",
         "app": {
           "urls": [
             "https://example.com",
             "http://example.com",
             "<all_urls>"
           ]
         }
       })";
  LoadAndExpectError(
      ManifestData::FromJSON(kManifest),
      ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidWebURL, /*s1=*/"2",
                                          errors::kCannotClaimAllURLsInExtent));
}

TEST_F(HostedAppWebURLsManifestTest, InvalidStarHost) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for invalid '*' host in app.urls list",
         "manifest_version": 3,
         "version": "1",
         "app": {
           "urls": [
             "https://example.com",
             "http://example.com",
             "https://*"
           ]
         }
       })";
  LoadAndExpectError(ManifestData::FromJSON(kManifest),
                     ErrorUtils::FormatErrorMessageUTF16(
                         errors::kInvalidWebURL, /*s1=*/"2",
                         errors::kCannotClaimAllHostsInExtent));
}

TEST_F(HostedAppWebURLsManifestTest, InvalidStarInPath) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for invalid '*' path in app.urls list",
         "manifest_version": 3,
         "version": "1",
         "app": {
           "urls": [
             "https://example.com",
             "http://example.com",
             "https://example.com/*"
           ]
         }
       })";
  LoadAndExpectError(
      ManifestData::FromJSON(kManifest),
      ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidWebURL, /*s1=*/"2",
                                          errors::kNoWildCardsInPaths));
}

}  // namespace extensions
