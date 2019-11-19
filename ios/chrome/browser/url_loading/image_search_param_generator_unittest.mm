// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/image_search_param_generator.h"

#include "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ImageSearchParamGeneratorTest : public PlatformTest {
 public:
  ImageSearchParamGeneratorTest() {}

 protected:
  void SetUp() override {
    // Set up a TestChromeBrowserState instance.
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(ImageSearchParamGeneratorTest, TestNilImage) {
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  web::NavigationManager::WebLoadParams load_params =
      ImageSearchParamGenerator::LoadParamsForImageData(nil, GURL(),
                                                        template_url_service);
  ASSERT_EQ(load_params.url,
            GURL("https://www.google.com/searchbyimage/upload"));
}
