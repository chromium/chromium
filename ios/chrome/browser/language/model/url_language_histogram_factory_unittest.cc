// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/model/url_language_histogram_factory.h"

#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using testing::IsNull;
using testing::Not;

class UrlLanguageHistogramFactoryTest : public PlatformTest {
 public:
  UrlLanguageHistogramFactoryTest() {
    TestProfileIOS::Builder profile_builder;
    profile_ = std::move(profile_builder).Build();
  }

  ~UrlLanguageHistogramFactoryTest() override { profile_.reset(); }

  ProfileIOS* profile() { return profile_.get(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(UrlLanguageHistogramFactoryTest, NotCreatedInIncognito) {
  EXPECT_THAT(UrlLanguageHistogramFactory::GetForProfile(profile()),
              Not(IsNull()));

  ProfileIOS* otr_profile = profile()->GetOffTheRecordProfile();
  language::UrlLanguageHistogram* language_histogram =
      UrlLanguageHistogramFactory::GetForProfile(otr_profile);
  EXPECT_THAT(language_histogram, IsNull());
}
