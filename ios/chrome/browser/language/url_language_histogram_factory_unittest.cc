// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/url_language_histogram_factory.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using testing::IsNull;
using testing::Not;

class UrlLanguageHistogramFactoryTest : public PlatformTest {
 public:
  UrlLanguageHistogramFactoryTest() {
    TestChromeBrowserState::Builder browser_state_builder;
    chrome_browser_state_ = browser_state_builder.Build();
  }

  ~UrlLanguageHistogramFactoryTest() override { chrome_browser_state_.reset(); }

  ios::ChromeBrowserState* chrome_browser_state() {
    return chrome_browser_state_.get();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(UrlLanguageHistogramFactoryTest, NotCreatedInIncognito) {
  EXPECT_THAT(
      UrlLanguageHistogramFactory::GetForBrowserState(chrome_browser_state()),
      Not(IsNull()));

  ios::ChromeBrowserState* otr_browser_state =
      chrome_browser_state()->GetOffTheRecordChromeBrowserState();
  language::UrlLanguageHistogram* language_histogram =
      UrlLanguageHistogramFactory::GetForBrowserState(otr_browser_state);
  EXPECT_THAT(language_histogram, IsNull());
}
