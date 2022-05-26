// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/language/language_model_manager_factory.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"
#import "ios/chrome/browser/translate/language_detection_model_service_factory.h"
#import "ios/chrome/browser/translate/translate_model_service_factory.h"
#import "ios/chrome/browser/translate/translate_ranker_factory.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ChromeIOSTranslateClientTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {translate::kTFLiteLanguageDetectionEnabled}, {});
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());

    browser_state_ = builder.Build();

    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state_.SetBrowserState(browser_state_.get());
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        &web_state_, /*url_language_histogram=*/nullptr);
    ChromeIOSTranslateClient::CreateForWebState(&web_state_);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::FakeWebState web_state_;
};

TEST_F(ChromeIOSTranslateClientTest, TranslateUICreated) {
  ChromeIOSTranslateClient* translate_client =
      ChromeIOSTranslateClient::FromWebState(&web_state_);
  translate_client->ShowTranslateUI(translate::TRANSLATE_STEP_AFTER_TRANSLATE,
                                    "en", "en",
                                    translate::TranslateErrors::NONE,
                                    /*triggered_from_menu=*/false);
  EXPECT_EQ(1U, InfoBarManagerImpl::FromWebState(&web_state_)->infobar_count());
}
