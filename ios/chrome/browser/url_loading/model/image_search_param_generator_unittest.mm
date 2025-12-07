// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"

#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

class ImageSearchParamGeneratorTest : public PlatformTest {
 public:
  ImageSearchParamGeneratorTest() {}

 protected:
  void SetUp() override {
    // Set up a TestProfileIOS instance.
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_profile_builder).Build();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(ImageSearchParamGeneratorTest, TestNilImage) {
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
  web::NavigationManager::WebLoadParams load_params =
      ImageSearchParamGenerator::LoadParamsForImageData(nil, GURL(),
                                                        template_url_service);
  ASSERT_EQ(load_params.url,
            GURL("https://www.google.com/searchbyimage/upload"));
}
