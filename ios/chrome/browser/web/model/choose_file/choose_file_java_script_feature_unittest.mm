// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_java_script_feature.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
const char kPageHtml[] = "<html><body>"
                         "<input type=\"file\" id=\"choose_file\" "
                         "ACCEPT_PLACEHOLDER MULTIPLE_PLACEHOLDER/>"
                         "</body></html>";

const char kPageHtmlWithButton[] = "<html><body>"
                                   "<input type=\"button\" id=\"button\"/>"
                                   "</body></html>";
}  // namespace

// Tests metrics are logged when tapping choose file input.
class ChooseFileJavaScriptFeatureTest : public PlatformTest {
 protected:
  ChooseFileJavaScriptFeatureTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    ChooseFileJavaScriptFeature* feature =
        ChooseFileJavaScriptFeature::GetInstance();
    GetWebClient()->SetJavaScriptFeatures({feature});
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }

  web::WebState* web_state() { return web_state_.get(); }

  void LoadHtml(bool has_multiple, bool has_accept, NSString* accept_value) {
    NSString* html = base::SysUTF8ToNSString(kPageHtml);
    if (has_multiple) {
      html = [html stringByReplacingOccurrencesOfString:@"MULTIPLE_PLACEHOLDER"
                                             withString:@"multiple"];
    } else {
      html = [html stringByReplacingOccurrencesOfString:@"MULTIPLE_PLACEHOLDER"
                                             withString:@""];
    }
    if (has_accept) {
      html = [html
          stringByReplacingOccurrencesOfString:@"ACCEPT_PLACEHOLDER"
                                    withString:[NSString stringWithFormat:
                                                             @"accept=\"%@\"",
                                                             accept_value]];

    } else {
      html = [html stringByReplacingOccurrencesOfString:@"ACCEPT_PLACEHOLDER"
                                             withString:@""];
    }
    web::test::LoadHtml(html, web_state());
  }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests that metrics are logged with correct bucket when tapping on an input
// file. One case for each bucket.
TEST_F(ChooseFileJavaScriptFeatureTest, TestMetricsLogging) {
  // See definitions of ChooseFileAccept enum in
  // choose_file_javascript_feature.mm for the order and values.
  NSArray* accept_attributes = @[
    @"", @".pdf,.jpg", @".invalid", @".jpg", @".avi", @".mp3", @".zip", @".pdf",
    @".doc", @".pkpass"
  ];
  NSArray* multiple_attributes = @[ @NO, @YES ];

  for (int accept_index = 0; accept_index < 9; accept_index++) {
    for (int multiple_index = 0; multiple_index < 2; multiple_index++) {
      base::HistogramTester histogram_tester;
      LoadHtml([multiple_attributes[multiple_index] boolValue], true,
               accept_attributes[accept_index]);
      ASSERT_TRUE(
          web::test::TapWebViewElementWithId(web_state(), "choose_file"));
      histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);
      histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                         2 * accept_index + multiple_index, 1);
    }
  }
}

// Extra test cases for metrics logging that test corner cases.
TEST_F(ChooseFileJavaScriptFeatureTest, TestMetricsLoggingExtra) {
  base::HistogramTester histogram_tester;
  int total_count = 0;
  // No accept, no multiple
  LoadHtml(false, false, @"");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kNoAccept*/ 0, 1);

  // No accept, multiple
  LoadHtml(true, false, @"");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kNoAcceptMultiple*/ 1, 1);

  // Multiple empty
  LoadHtml(false, true, @",,,  ,  ,");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kNoAccept*/ 0, 2);

  // Image extension with a dot
  LoadHtml(false, true, @".png");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 1);

  // Image extension without a dot
  LoadHtml(false, true, @"jpg");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 2);

  // Image mime
  LoadHtml(false, true, @"image/png");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 3);

  // Generic Image mime
  LoadHtml(false, true, @"image/*");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 4);

  // Two Image types
  LoadHtml(false, true, @"png, jpg");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 5);

  // Video with spaces
  LoadHtml(false, true, @"  .mp4  ");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kVideoAccept*/ 8, 1);

  // Audio without dot
  LoadHtml(false, true, @"mp3");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kAudioAccept*/ 10, 1);

  // Archive
  LoadHtml(false, true, @"zip , rar ");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kArchiveAccept*/ 12, 1);

  // Unknown and image
  LoadHtml(false, true, @"unknown, jpg");
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 6);
}

// Tests that no metrics are logged when tapping on a normal button.
TEST_F(ChooseFileJavaScriptFeatureTest, TestNoMetricsLoggingOnButtonClick) {
  base::HistogramTester histogram_tester;
  web::test::LoadHtml(base::SysUTF8ToNSString(kPageHtmlWithButton),
                      web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "button"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 0);
}

// Tests that no metrics are logged when tapping on a normal button.
TEST_F(ChooseFileJavaScriptFeatureTest, TestInvalidPayload) {
  base::HistogramTester histogram_tester;
  web::test::LoadHtml(base::SysUTF8ToNSString(kPageHtmlWithButton),
                      web_state());
  // Test synchronisation
  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"__gCrWeb.common.sendWebKitMessage('ChooseFileHandler', "
      @"{'acceptType':0,'hasMultiple':true});",
      ChooseFileJavaScriptFeature::GetInstance());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"__gCrWeb.common.sendWebKitMessage('ChooseFileHandler', {});",
      ChooseFileJavaScriptFeature::GetInstance());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"__gCrWeb.common.sendWebKitMessage('ChooseFileHandler', "
      @"{'acceptType':-2, 'hasMultiple':true});",
      ChooseFileJavaScriptFeature::GetInstance());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"__gCrWeb.common.sendWebKitMessage('ChooseFileHandler', "
      @"{'acceptType':37, 'hasMultiple':true});",
      ChooseFileJavaScriptFeature::GetInstance());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"__gCrWeb.common.sendWebKitMessage('ChooseFileHandler', "
      @"{'acceptType':'invalid', 'hasMultiple':true});",
      ChooseFileJavaScriptFeature::GetInstance());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"__gCrWeb.common.sendWebKitMessage('ChooseFileHandler', "
      @"{'missing':'invalid', 'hasMultiple':true});",
      ChooseFileJavaScriptFeature::GetInstance());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  // Test synchronisation
  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"__gCrWeb.common.sendWebKitMessage('ChooseFileHandler', "
      @"{'acceptType':0,'hasMultiple':true});",
      ChooseFileJavaScriptFeature::GetInstance());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 2);
}

// Tests that `ResetLastChooseFileEvent()` returns the expected file extensions
// and resets the last event.
TEST_F(ChooseFileJavaScriptFeatureTest,
       TestResetLastChooseFileEventFileExtensions) {
  base::test::ScopedFeatureList feature_list(kIOSChooseFromDrive);
  const std::map<std::string, std::vector<std::string>>
      accept_attributes_file_extensions = {
          {"", {}},
          {".jpg", {".jpg"}},
          {".pdf,.jpg", {".pdf", ".jpg"}},
          {".avi", {".avi"}},
          {".zip, .rar", {".zip", ".rar"}},
          {".zip, .rar, .jpg", {".zip", ".rar", ".jpg"}},
          {"video/mp4", {}},
          {"video/mp4, .jpg", {".jpg"}},
          {"video/mp4, .pdf, .jpg", {".pdf", ".jpg"}},
          {"audio/*", {}},
          {"application/zip", {}},
          {".jpg,,,", {".jpg"}},
          {".b,. b,.\",/(,.),./,.;,.=,.?,.[],.{}", {".b"}},
      };
  constexpr bool multiple_attributes[] = {false, true};

  for (const auto& [accept_attribute, expected_file_extensions] :
       accept_attributes_file_extensions) {
    for (const auto& multiple_attribute : multiple_attributes) {
      const bool has_accept = accept_attribute.empty() == false;
      LoadHtml(multiple_attribute, has_accept, @(accept_attribute.c_str()));
      ASSERT_TRUE(
          web::test::TapWebViewElementWithId(web_state(), "choose_file"));
      const std::optional<ChooseFileEvent> event =
          ChooseFileJavaScriptFeature::GetInstance()
              ->ResetLastChooseFileEvent();
      ASSERT_TRUE(event.has_value());
      EXPECT_EQ(expected_file_extensions, event->accept_file_extensions);
      EXPECT_EQ(multiple_attribute, event->allow_multiple_files);
      EXPECT_FALSE(ChooseFileJavaScriptFeature::GetInstance()
                       ->ResetLastChooseFileEvent());
    }
  }
}

// Tests that `ResetLastChooseFileEvent()` returns the expected MIME types and
// resets the last event.
TEST_F(ChooseFileJavaScriptFeatureTest, TestResetLastChooseFileEventMimeTypes) {
  base::test::ScopedFeatureList feature_list(kIOSChooseFromDrive);
  const std::map<std::string, std::vector<std::string>>
      accept_attributes_mime_types = {
          {"", {}},
          {".jpg", {}},
          {".pdf,.jpg", {}},
          {".avi", {}},
          {".zip, .rar", {}},
          {".zip, .rar, .jpg", {}},
          {"video/mp4", {"video/mp4"}},
          {"video/mp4, .jpg", {"video/mp4"}},
          {"video/mp4, .pdf, .jpg", {"video/mp4"}},
          {"audio/*", {"audio/*"}},
          {"application/zip", {"application/zip"}},
          {"application/zip,  image/*", {"application/zip", "image/*"}},
          {"application/zip, .jpg, image/*", {"application/zip", "image/*"}},
          {"application/application/", {}},
          {".jpg,,,", {}},
          {"a/b,a/ b,a/\",a/(,a/),a//,a/;,a/=,a/?,a/[],a/{}", {"a/b"}},
      };
  constexpr bool multiple_attributes[] = {false, true};

  for (const auto& [accept_attribute, expected_mime_types] :
       accept_attributes_mime_types) {
    for (const auto& multiple_attribute : multiple_attributes) {
      const bool has_accept = accept_attribute.empty() == false;
      LoadHtml(multiple_attribute, has_accept, @(accept_attribute.c_str()));
      ASSERT_TRUE(
          web::test::TapWebViewElementWithId(web_state(), "choose_file"));
      const std::optional<ChooseFileEvent> event =
          ChooseFileJavaScriptFeature::GetInstance()
              ->ResetLastChooseFileEvent();
      ASSERT_TRUE(event.has_value());
      EXPECT_EQ(expected_mime_types, event->accept_mime_types);
      EXPECT_EQ(multiple_attribute, event->allow_multiple_files);
      EXPECT_FALSE(ChooseFileJavaScriptFeature::GetInstance()
                       ->ResetLastChooseFileEvent());
    }
  }
}
