// Copyright 2019 The Chromium Authors
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
  ManifestData CreateManifest(const char* replacement_web_app) {
    constexpr char kManifest[] =
        R"({
             "name": "test",
             "version": "1",
             "manifest_version": 2,
             "replacement_web_app": %s
           })";
    base::Value manifest = base::test::ParseJson(
        base::StringPrintf(kManifest, replacement_web_app));
    return ManifestData(std::move(manifest).TakeDict());
  }

 private:
  ScopedCurrentChannel channel_;
};

}  // namespace

TEST_F(ReplacementAppsManifestTest, InvalidWebAppType) {
  LoadAndExpectError(CreateManifest("32"),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest("true"),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest(R"("not_a_valid_url")"),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest("{}"),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest(R"({"foo": false})"),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest(R"("http://not_secure.com")"),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(CreateManifest(R"(["https://secure.com"])"),
                     manifest_errors::kInvalidReplacementWebApp);
  LoadAndExpectError(
      CreateManifest(R"(["https://www.google.com", "not_a_valid_url"])"),
      manifest_errors::kInvalidReplacementWebApp);
}

TEST_F(ReplacementAppsManifestTest, VerifyParse) {
  scoped_refptr<Extension> good =
      LoadAndExpectSuccess(CreateManifest(R"("https://www.google.com")"));
  EXPECT_TRUE(ReplacementAppsInfo::HasReplacementWebApp(good.get()));
  EXPECT_EQ(ReplacementAppsInfo::GetReplacementWebApp(good.get()),
            GURL("https://www.google.com"));
}

}  // namespace extensions
