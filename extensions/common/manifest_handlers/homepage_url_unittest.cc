// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/manifest_url_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

using HomepageURLManifestTest = ManifestTest;

TEST_F(HomepageURLManifestTest, ParseHomepageURLs) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("homepage_url_valid.json"));

  Testcase testcases[] = {
    Testcase("homepage_url_empty.json",
             errors::kInvalidHomepageURL),
    Testcase("homepage_url_invalid.json",
             errors::kInvalidHomepageURL),
    Testcase("homepage_url_bad_schema.json",
             errors::kInvalidHomepageURL)
  };
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(HomepageURLManifestTest, GetHomepageURL) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("homepage_url_valid.json"));
  EXPECT_EQ(GURL("http://foo.com#bar"),
            ManifestURL::GetHomepageURL(extension.get()));

  // The Google Gallery URL ends with the id, which depends on the path, which
  // can be different in testing, so we just check the part before id.
  extension = LoadAndExpectSuccess("homepage_url_google_hosted.json");
  EXPECT_TRUE(
      base::StartsWith(ManifestURL::GetHomepageURL(extension.get()).spec(),
                       extension_urls::GetWebstoreItemDetailURLPrefix(),
                       base::CompareCase::INSENSITIVE_ASCII));

  extension = LoadAndExpectSuccess("homepage_url_externally_hosted.json");
  EXPECT_EQ(GURL(), ManifestURL::GetHomepageURL(extension.get()));
}

}  // namespace extensions
