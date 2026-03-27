// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/description_info.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

using DescriptionManifestTest = ManifestTest;

TEST_F(DescriptionManifestTest, Empty) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for empty description",
         "manifest_version": 3,
         "version": "1",
         "description": ""
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_EQ("", DescriptionInfo::GetDescription(*extension.get()));
}

TEST_F(DescriptionManifestTest, Omitted) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for omitted description",
         "manifest_version": 3,
         "version": "1"
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_EQ("", DescriptionInfo::GetDescription(*extension.get()));
}

TEST_F(DescriptionManifestTest, Invalid) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for invalid description",
         "manifest_version": 3,
         "version": "1",
         "description": 42
       })";
  LoadAndExpectError(ManifestData::FromJSON(kManifest),
                     manifest_errors::kInvalidDescription);
}

TEST_F(DescriptionManifestTest, Simple) {
  static constexpr char kManifest[] =
      R"({
         "name": "Test for description",
         "manifest_version": 3,
         "version": "1",
         "description": "Simple test for a short extension description."
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_EQ("Simple test for a short extension description.",
            DescriptionInfo::GetDescription(*extension.get()));
}

}  // namespace extensions
