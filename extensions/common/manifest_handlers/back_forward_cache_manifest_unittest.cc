// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/back_forward_cache_info.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kManifestTemplate[] =
    R"({
         "name": "Test Extension",
         "version": "1.0",
         "manifest_version": 3,
         "back_forward_cache": %s
       })";

}  // namespace

class BackForwardCacheManifestTest : public ManifestTest {
 public:
  BackForwardCacheManifestTest() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      extensions_features::kBackForwardCacheForComponentExtensionFrames};
};

TEST_F(BackForwardCacheManifestTest, FeatureFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      extensions_features::kBackForwardCacheForComponentExtensionFrames);
  std::string manifest =
      base::StringPrintf(kManifestTemplate, R"({"supported": true})");
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      ManifestData::FromJSON(manifest), mojom::ManifestLocation::kComponent));
  EXPECT_FALSE(BackForwardCacheInfo::IsSupported(*extension));
}

TEST_F(BackForwardCacheManifestTest, NotSpecified) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(ManifestData::FromJSON(R"({
        "name": "Test Extension",
        "version": "1.0",
        "manifest_version": 3
      })"),
                           mojom::ManifestLocation::kComponent));
  EXPECT_FALSE(BackForwardCacheInfo::IsSupported(*extension));
}

TEST_F(BackForwardCacheManifestTest, NonComponentExtensionIgnored) {
  std::string manifest =
      base::StringPrintf(kManifestTemplate, R"({"supported": true})");
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      ManifestData::FromJSON(manifest), mojom::ManifestLocation::kInternal));
  // Restricted to component extensions via _manifest_features.json.
  EXPECT_FALSE(BackForwardCacheInfo::IsSupported(*extension));
}

TEST_F(BackForwardCacheManifestTest, DictionaryTrue) {
  std::string manifest =
      base::StringPrintf(kManifestTemplate, R"({"supported": true})");
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      ManifestData::FromJSON(manifest), mojom::ManifestLocation::kComponent));
  EXPECT_TRUE(BackForwardCacheInfo::IsSupported(*extension));
}

TEST_F(BackForwardCacheManifestTest, DictionaryFalse) {
  std::string manifest =
      base::StringPrintf(kManifestTemplate, R"({"supported": false})");
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      ManifestData::FromJSON(manifest), mojom::ManifestLocation::kComponent));
  EXPECT_FALSE(BackForwardCacheInfo::IsSupported(*extension));
}

TEST_F(BackForwardCacheManifestTest, BooleanDisallowed) {
  std::string manifest = base::StringPrintf(kManifestTemplate, "true");
  LoadAndExpectError(ManifestData::FromJSON(manifest),
                     manifest_errors::kInvalidBackForwardCache,
                     mojom::ManifestLocation::kComponent);
}

TEST_F(BackForwardCacheManifestTest, InvalidStringValue) {
  std::string manifest = base::StringPrintf(kManifestTemplate, "\"invalid\"");
  LoadAndExpectError(ManifestData::FromJSON(manifest),
                     manifest_errors::kInvalidBackForwardCache,
                     mojom::ManifestLocation::kComponent);
}

TEST_F(BackForwardCacheManifestTest, InvalidDictionaryValue) {
  std::string manifest =
      base::StringPrintf(kManifestTemplate, R"({"supported": "not_a_bool"})");
  LoadAndExpectError(ManifestData::FromJSON(manifest),
                     manifest_errors::kInvalidBackForwardCache,
                     mojom::ManifestLocation::kComponent);
}

}  // namespace extensions
