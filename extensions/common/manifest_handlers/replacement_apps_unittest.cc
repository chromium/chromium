// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "components/version_info/version_info.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/replacement_apps.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class ReplacementAppsManifestTest : public ManifestTest {
 public:
  ReplacementAppsManifestTest() : channel_(version_info::Channel::UNKNOWN) {}

 protected:
  ManifestData CreateManifest(const char* replacement_web_app,
                              const char* replacement_android_app) {
    if (replacement_web_app != nullptr && replacement_android_app != nullptr) {
      // both replacement apps are specified
      constexpr char kManifest[] =
          R"({
             "name": "test",
             "version": "1",
             "manifest_version": 2,
             "app": {
               "background": {
                 "scripts": ["background.js"]
               }
             },
             "replacement_web_app": %s,
             "replacement_android_app": %s
           })";
      base::Value manifest = base::test::ParseJson(base::StringPrintf(
          kManifest, replacement_web_app, replacement_android_app));
      return ManifestData(std::move(manifest), "test");
    } else if (replacement_web_app != nullptr) {
      // only web replacement app specified
      constexpr char kManifest[] =
          R"({
             "name": "test",
             "version": "1",
             "manifest_version": 2,
             "replacement_web_app": %s
           })";
      base::Value manifest = base::test::ParseJson(
          base::StringPrintf(kManifest, replacement_web_app));
      return ManifestData(std::move(manifest), "test");
    } else if (replacement_android_app != nullptr) {
      // only Android replacement app specified
      constexpr char kManifest[] =
          R"({
            "name": "test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "background": {
                "scripts": ["background.js"]
              }
            },
            "replacement_android_app": %s
            })";
      base::Value manifest = base::test::ParseJson(
          base::StringPrintf(kManifest, replacement_android_app));
      return ManifestData(std::move(manifest), "test");
    }

    base::Value manifest = base::test::ParseJson(
        R"({
             "name": "test",
             "version": "1",
             "manifest_version": 2
           })");
    return ManifestData(std::move(manifest), "test");
  }

 private:
  ScopedCurrentChannel channel_;
};

}  // namespace

TEST_F(ReplacementAppsManifestTest, InvalidAndroidAppType) {
  LoadAndExpectError(CreateManifest(nullptr, "123"),
                     manifest_errors::kInvalidReplacementAndroidApp);
  LoadAndExpectError(CreateManifest(nullptr, "true"),
                     manifest_errors::kInvalidReplacementAndroidApp);
  LoadAndExpectError(CreateManifest(nullptr, "{}"),
                     manifest_errors::kInvalidReplacementAndroidApp);
  LoadAndExpectError(CreateManifest(nullptr, R"({"foo": false})"),
                     manifest_errors::kInvalidReplacementAndroidApp);
  LoadAndExpectError(CreateManifest(nullptr, R"(["com.company.app"])"),
                     manifest_errors::kInvalidReplacementAndroidApp);
}

TEST_F(ReplacementAppsManifestTest, InvalidWebAppType) {
  LoadAndExpectError(CreateManifest("32", nullptr),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest("true", nullptr),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest(R"("not_a_valid_url")", nullptr),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest("{}", nullptr),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest(R"({"foo": false})", nullptr),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest(R"("http://not_secure.com")", nullptr),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest(R"(["https://secure.com"])", nullptr),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(
      CreateManifest(R"(["https://www.google.com", "not_a_valid_url"])",
                     nullptr),
      manifest_errors::kInvalidReplacementWebApp);
}

TEST_F(ReplacementAppsManifestTest, VerifyParse) {
  scoped_refptr<Extension> good = LoadAndExpectSuccess(
      CreateManifest(R"("https://www.google.com")", R"("com.company.app")"));
  EXPECT_TRUE(ReplacementAppsInfo::HasReplacementWebApp(good.get()));
  EXPECT_EQ(ReplacementAppsInfo::GetReplacementWebApp(good.get()),
            GURL("https://www.google.com"));
  EXPECT_TRUE(ReplacementAppsInfo::HasReplacementAndroidApp(good.get()));
  EXPECT_EQ(ReplacementAppsInfo::GetReplacementAndroidApp(good.get()),
            "com.company.app");
}

}  // namespace extensions
