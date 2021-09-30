// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_reporter_android.h"

#include "content/browser/attribution_reporting/conversion_manager.h"
#include "content/browser/attribution_reporting/conversion_test_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/common/url_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

const char kPackageName[] = "org.chromium.chrome.test";
const char kConversionUrl[] = "https://b.com";
const char kInvalidUrl[] = "http://insecure.com";
const char kReportToUrl[] = "https://c.com";
const char kEventId[] = "12345";

class AttributionReporterTest : public ::testing::Test {
 public:
  AttributionReporterTest() = default;

  void SetUp() override {
    url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
  }

  void TearDown() override {}

 protected:
  TestConversionManager test_manager_;

 private:
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

TEST_F(AttributionReporterTest, ValidImpression_Allowed) {
  attribution_reporter_android::ReportAppImpression(
      test_manager_, nullptr, kPackageName, kEventId, kConversionUrl,
      kReportToUrl, 56789);

  EXPECT_EQ(1u, test_manager_.num_impressions());

  EXPECT_EQ(OriginFromAndroidPackageName(kPackageName),
            test_manager_.last_impression_origin());
  EXPECT_EQ(StorableSource::SourceType::kEvent,
            test_manager_.last_impression_source_type());
}

TEST_F(AttributionReporterTest, ValidImpression_Disallowed) {
  ConversionDisallowingContentBrowserClient browser_client;

  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&browser_client);

  attribution_reporter_android::ReportAppImpression(
      test_manager_, nullptr, kPackageName, kEventId, kConversionUrl,
      kReportToUrl, 56789);

  EXPECT_EQ(0u, test_manager_.num_impressions());

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(AttributionReporterTest, InvalidImpression) {
  attribution_reporter_android::ReportAppImpression(
      test_manager_, nullptr, kPackageName, kEventId, kInvalidUrl, kReportToUrl,
      56789);

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

}  // namespace content
