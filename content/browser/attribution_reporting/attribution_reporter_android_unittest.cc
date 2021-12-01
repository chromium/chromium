// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_reporter_android.h"

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/common/url_utils.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

namespace {

using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::IsNull;
using testing::Pointee;
using testing::Property;
using testing::Return;

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

 protected:
  TestAttributionManager test_manager_;

 private:
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

TEST_F(AttributionReporterTest, ValidImpression_Allowed) {
  base::Time time = base::Time::Now() - base::Hours(1);
  attribution_reporter_android::ReportAppImpression(
      test_manager_, nullptr, kPackageName, kEventId, kConversionUrl,
      kReportToUrl, 56789, time);

  EXPECT_THAT(
      test_manager_.handled_sources(),
      ElementsAre(AllOf(Property(&StorableSource::impression_origin,
                                 OriginFromAndroidPackageName(kPackageName)),
                        Property(&StorableSource::source_type,
                                 StorableSource::SourceType::kEvent),
                        Property(&StorableSource::impression_time, time))));
}

TEST_F(AttributionReporterTest, ValidImpression_Allowed_NoOptionals) {
  attribution_reporter_android::ReportAppImpression(
      test_manager_, nullptr, kPackageName, kEventId, kConversionUrl, "", 0,
      base::Time::Now());

  EXPECT_THAT(
      test_manager_.handled_sources(),
      ElementsAre(AllOf(Property(&StorableSource::impression_origin,
                                 OriginFromAndroidPackageName(kPackageName)),
                        Property(&StorableSource::source_type,
                                 StorableSource::SourceType::kEvent))));
}

TEST_F(AttributionReporterTest, ValidImpression_Disallowed) {
  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(
      browser_client,
      IsConversionMeasurementOperationAllowed(
          _, ContentBrowserClient::ConversionMeasurementOperation::kImpression,
          Pointee(_), IsNull(), Pointee(_)))
      .WillOnce(Return(false));
  ScopedContentBrowserClientSetting setting(&browser_client);

  attribution_reporter_android::ReportAppImpression(
      test_manager_, nullptr, kPackageName, kEventId, kConversionUrl,
      kReportToUrl, 56789, base::Time::Now());

  EXPECT_THAT(test_manager_.handled_sources(), IsEmpty());
}

TEST_F(AttributionReporterTest, InvalidImpression) {
  attribution_reporter_android::ReportAppImpression(
      test_manager_, nullptr, kPackageName, kEventId, kInvalidUrl, kReportToUrl,
      56789, base::Time::Now());

  EXPECT_THAT(test_manager_.handled_sources(), IsEmpty());
}

}  // namespace
}  // namespace content
