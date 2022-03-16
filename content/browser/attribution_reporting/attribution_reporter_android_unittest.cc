// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_reporter_android.h"

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
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

using testing::AllOf;

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
  MockAttributionManager mock_manager_;

 private:
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

TEST_F(AttributionReporterTest, ValidImpression_Allowed) {
  base::Time time = base::Time::Now() - base::Hours(1);

  EXPECT_CALL(
      mock_manager_,
      HandleSource(
          AllOf(ImpressionOriginIs(OriginFromAndroidPackageName(kPackageName)),
                SourceTypeIs(AttributionSourceType::kEvent),
                ImpressionTimeIs(time))));

  attribution_reporter_android::ReportAppImpression(mock_manager_, kPackageName,
                                                    kEventId, kConversionUrl,
                                                    kReportToUrl, 56789, time);
}

TEST_F(AttributionReporterTest, ImpressionWithoutReportingOrigin_NotAllowed) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  attribution_reporter_android::ReportAppImpression(mock_manager_, kPackageName,
                                                    kEventId, kConversionUrl,
                                                    "", 0, base::Time::Now());
}

TEST_F(AttributionReporterTest, InvalidImpression) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  attribution_reporter_android::ReportAppImpression(
      mock_manager_, kPackageName, kEventId, kInvalidUrl, kReportToUrl, 56789,
      base::Time::Now());
}

}  // namespace
}  // namespace content
