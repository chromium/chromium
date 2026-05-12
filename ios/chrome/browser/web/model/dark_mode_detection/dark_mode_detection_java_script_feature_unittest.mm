// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/dark_mode_detection/dark_mode_detection_java_script_feature.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

namespace {

const char kPlainPageHtml[] = "<html><body>"
                              "<h1>Hello World</h1>"
                              "</body></html>";

const char kMetaDarkPageHtml[] =
    "<html><head>"
    "<meta name=\"color-scheme\" content=\"light dark\">"
    "</head><body>"
    "<h1>Dark Mode via Meta</h1>"
    "</body></html>";

const char kCssStyleDarkPageHtml[] = "<html><head>"
                                     "<style>"
                                     "  :root { color-scheme: light dark; }"
                                     "</style>"
                                     "</head><body>"
                                     "<h1>Dark Mode via CSS color-scheme</h1>"
                                     "</body></html>";

const char kMediaStyleDarkPageHtml[] =
    "<html><head>"
    "<style>"
    "@media (prefers-color-scheme: dark) {"
    "  body { background-color: black; color: white; }"
    "}"
    "</style>"
    "</head><body>"
    "<h1>Dark Mode via media styles</h1>"
    "</body></html>";

}  // namespace

class DarkModeDetectionJavaScriptFeatureTest : public PlatformTest {
 protected:
  DarkModeDetectionJavaScriptFeatureTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    GetWebClient()->SetJavaScriptFeatures({feature()});

    UIUserInterfaceStyle user_interface_style =
        UITraitCollection.currentTraitCollection.userInterfaceStyle;
    is_dark_mode_ = (user_interface_style == UIUserInterfaceStyleDark);
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }

  web::WebState* web_state() { return web_state_.get(); }

  DarkModeDetectionJavaScriptFeature* feature() {
    return DarkModeDetectionJavaScriptFeature::GetInstance();
  }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  bool is_dark_mode_ = false;
};

TEST_F(DarkModeDetectionJavaScriptFeatureTest, PlainPageNoDarkMode) {
  base::HistogramTester histogram_tester;
  int not_supported_bucket = is_dark_mode_ ? 1 : 0;

  web::test::LoadHtml(base::SysUTF8ToNSString(kPlainPageHtml), web_state());

  // Wait for the script to run and record histograms
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !histogram_tester
                .GetAllSamples("IOS.DarkModeDetection.SupportsDarkMode")
                .empty();
  }));

  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsViaMeta",
                                      not_supported_bucket, 1);
  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsViaCss",
                                      not_supported_bucket, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.DarkModeDetection.SupportsViaMediaQuery", not_supported_bucket, 1);
  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsDarkMode",
                                      not_supported_bucket, 1);
}

TEST_F(DarkModeDetectionJavaScriptFeatureTest, MetaDarkPageSupportsDarkMode) {
  base::HistogramTester histogram_tester;
  int supported_bucket = is_dark_mode_ ? 3 : 2;
  int not_supported_bucket = is_dark_mode_ ? 1 : 0;

  web::test::LoadHtml(base::SysUTF8ToNSString(kMetaDarkPageHtml), web_state());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !histogram_tester
                .GetAllSamples("IOS.DarkModeDetection.SupportsDarkMode")
                .empty();
  }));

  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsViaMeta",
                                      supported_bucket, 1);
  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsViaCss",
                                      not_supported_bucket, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.DarkModeDetection.SupportsViaMediaQuery", not_supported_bucket, 1);
  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsDarkMode",
                                      supported_bucket, 1);
}

TEST_F(DarkModeDetectionJavaScriptFeatureTest,
       CssStyleDarkPageSupportsDarkMode) {
  base::HistogramTester histogram_tester;
  int supported_bucket = is_dark_mode_ ? 3 : 2;
  int not_supported_bucket = is_dark_mode_ ? 1 : 0;

  web::test::LoadHtml(base::SysUTF8ToNSString(kCssStyleDarkPageHtml),
                      web_state());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !histogram_tester
                .GetAllSamples("IOS.DarkModeDetection.SupportsDarkMode")
                .empty();
  }));

  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsViaMeta",
                                      not_supported_bucket, 1);
  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsViaCss",
                                      supported_bucket, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.DarkModeDetection.SupportsViaMediaQuery", not_supported_bucket, 1);
  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsDarkMode",
                                      supported_bucket, 1);
}

TEST_F(DarkModeDetectionJavaScriptFeatureTest,
       MediaStyleDarkPageSupportsDarkMode) {
  base::HistogramTester histogram_tester;
  int supported_bucket = is_dark_mode_ ? 3 : 2;
  int not_supported_bucket = is_dark_mode_ ? 1 : 0;

  web::test::LoadHtml(base::SysUTF8ToNSString(kMediaStyleDarkPageHtml),
                      web_state());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !histogram_tester
                .GetAllSamples("IOS.DarkModeDetection.SupportsDarkMode")
                .empty();
  }));

  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsViaMeta",
                                      not_supported_bucket, 1);
  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsViaCss",
                                      not_supported_bucket, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.DarkModeDetection.SupportsViaMediaQuery", supported_bucket, 1);
  histogram_tester.ExpectUniqueSample("IOS.DarkModeDetection.SupportsDarkMode",
                                      supported_bucket, 1);
}
