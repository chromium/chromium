// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/version_name_info.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

using VersionNameManifestTest = ManifestTest;

TEST_F(VersionNameManifestTest, Empty) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for empty version name",
         "manifest_version": 3,
         "version": "1",
         "version_name": ""
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_EQ("", VersionNameInfo::GetVersionName(*extension.get()));
  EXPECT_EQ("1", extension.get()->GetVersionForDisplay());
}

TEST_F(VersionNameManifestTest, Omitted) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for omitted version name",
         "manifest_version": 3,
         "version": "1"
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_EQ("", VersionNameInfo::GetVersionName(*extension.get()));
  EXPECT_EQ("1", extension.get()->GetVersionForDisplay());
}

TEST_F(VersionNameManifestTest, Invalid) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for invalid version name",
         "manifest_version": 3,
         "version": "1",
         "version_name": 42
       })";
  LoadAndExpectError(ManifestData::FromJSON(kManifest),
                     manifest_errors::kInvalidVersionName);
}

TEST_F(VersionNameManifestTest, Valid) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for version name",
         "manifest_version": 3,
         "version": "1.1",
         "version_name": "1.2-beta"
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_EQ("1.2-beta", VersionNameInfo::GetVersionName(*extension.get()));
  EXPECT_EQ("1.2-beta", extension.get()->GetVersionForDisplay());
}

}  // namespace extensions
