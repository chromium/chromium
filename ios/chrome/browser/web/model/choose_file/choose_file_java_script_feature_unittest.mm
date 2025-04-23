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
#import "ios/chrome/browser/web/model/choose_file/choose_file_event_holder.h"
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
// A page template that contains a file input in different states.
const char kPageHtml[] =
    "<html><body>"
    "<input type=\"file\" id=\"choose_file\" "
    "ACCEPT_PLACEHOLDER MULTIPLE_PLACEHOLDER/>"
    "<script>"
    "const input = document.getElementById(\"choose_file\");"
    "SET_FILE_PLACEHOLDER"
    "</script>"
    "</body></html>";

// A page template that contains a button which, when clicked, creates a file
// input in different states and calls `click()` on it.
const char kPageHtmlForSimulatedClick[] =
    "<html><body>"
    "<button id=\"choose_file\" onclick=\"createInput()\"/>Choose file</button>"
    "<script>"
    "function createInput() {"
    "  const input = document.createElement('input');"
    "  input.type = 'file';"
    "  ACCEPT_PLACEHOLDER"
    "  MULTIPLE_PLACEHOLDER"
    "  SET_FILE_PLACEHOLDER"
    "  input.click();"
    "}"
    "</script>"
    "</body></html>";

// A script that adds a dummy file to the input.
const char kSetFileScript[] = "f = new File([], \"bar\");"
                              "dt = new DataTransfer();"
                              "dt.items.add(f);"
                              "input.files = dt.files;";

// A page with t normal (not file) button.
const char kPageHtmlWithButton[] = "<html><body>"
                                   "<input type=\"button\" id=\"button\"/>"
                                   "</body></html>";
}  // namespace

struct FeatureParam {
  bool support_simulated_click;
  bool test_simulated_click;
};

// Tests metrics are logged when tapping choose file input.
class ChooseFileJavaScriptFeatureTest
    : public testing::WithParamInterface<FeatureParam>,
      public PlatformTest {
 protected:
  ChooseFileJavaScriptFeatureTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {
    if (GetParam().support_simulated_click) {
      feature_list_.InitAndEnableFeature(kIOSChooseFromDriveSimulatedClick);
    } else {
      feature_list_.InitAndDisableFeature(kIOSChooseFromDriveSimulatedClick);
    }
  }

  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    java_script_feature_ = std::make_unique<ChooseFileJavaScriptFeature>();
    GetWebClient()->SetJavaScriptFeatures({java_script_feature_.get()});
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }

  web::WebState* web_state() { return web_state_.get(); }

  void LoadHtml(bool has_multiple,
                bool has_accept,
                NSString* accept_value,
                BOOL already_has_file) {
    NSString* html = GetParam().test_simulated_click
                         ? @(kPageHtmlForSimulatedClick)
                         : @(kPageHtml);
    if (has_multiple) {
      NSString* replacement = GetParam().test_simulated_click
                                  ? @"input.setAttribute('multiple', true);"
                                  : @"multiple";
      html = [html stringByReplacingOccurrencesOfString:@"MULTIPLE_PLACEHOLDER"
                                             withString:replacement];
    } else {
      html = [html stringByReplacingOccurrencesOfString:@"MULTIPLE_PLACEHOLDER"
                                             withString:@""];
    }
    if (has_accept) {
      NSString* replacement_template =
          GetParam().test_simulated_click
              ? @"input.setAttribute('accept', '%@');"
              : @"accept=\"%@\"";
      html = [html
          stringByReplacingOccurrencesOfString:@"ACCEPT_PLACEHOLDER"
                                    withString:[NSString
                                                   stringWithFormat:
                                                       replacement_template,
                                                       accept_value]];

    } else {
      html = [html stringByReplacingOccurrencesOfString:@"ACCEPT_PLACEHOLDER"
                                             withString:@""];
    }
    if (already_has_file) {
      html = [html stringByReplacingOccurrencesOfString:@"SET_FILE_PLACEHOLDER"
                                             withString:base::SysUTF8ToNSString(
                                                            kSetFileScript)];
    } else {
      html = [html stringByReplacingOccurrencesOfString:@"SET_FILE_PLACEHOLDER"
                                             withString:@""];
    }
    web::test::LoadHtml(html, web_state());
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ChooseFileJavaScriptFeature> java_script_feature_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests that metrics are logged with correct bucket when tapping on an input
// file. One case for each bucket.
TEST_P(ChooseFileJavaScriptFeatureTest, TestMetricsLogging) {
  // See definitions of ChooseFileAccept enum in
  // choose_file_javascript_feature.mm for the order and values.
  NSArray* accept_attributes = @[
    @"", @".pdf,.jpg", @".invalid", @".jpg", @".avi", @".mp3", @".zip", @".pdf",
    @".doc", @".pkpass"
  ];
  NSArray* bool_attributes = @[ @NO, @YES ];

  for (int accept_index = 0; accept_index < 9; accept_index++) {
    for (int multiple_index = 0; multiple_index < 2; multiple_index++) {
      for (int has_file_index = 0; has_file_index < 2; has_file_index++) {
        base::HistogramTester histogram_tester;
        LoadHtml([bool_attributes[multiple_index] boolValue], true,
                 accept_attributes[accept_index],
                 [bool_attributes[has_file_index] boolValue]);
        ASSERT_TRUE(
            web::test::TapWebViewElementWithId(web_state(), "choose_file"));
        histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);
        histogram_tester.ExpectBucketCount(
            "IOS.Web.FileInput.Clicked", 2 * accept_index + multiple_index, 1);
        histogram_tester.ExpectTotalCount("IOS.Web.FileInput.ContentState", 1);
        histogram_tester.ExpectBucketCount("IOS.Web.FileInput.ContentState",
                                           2 * multiple_index + has_file_index,
                                           1);
      }
    }
  }
}

// Extra test cases for metrics logging that test corner cases.
TEST_P(ChooseFileJavaScriptFeatureTest, TestMetricsLoggingExtra) {
  base::HistogramTester histogram_tester;
  int total_count = 0;
  // No accept, no multiple
  LoadHtml(false, false, @"", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kNoAccept*/ 0, 1);

  // No accept, multiple
  LoadHtml(true, false, @"", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kNoAcceptMultiple*/ 1, 1);

  // Multiple empty
  LoadHtml(false, true, @",,,  ,  ,", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kNoAccept*/ 0, 2);

  // Image extension with a dot
  LoadHtml(false, true, @".png", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 1);

  // Image extension without a dot
  LoadHtml(false, true, @"jpg", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 2);

  // Image mime
  LoadHtml(false, true, @"image/png", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 3);

  // Generic Image mime
  LoadHtml(false, true, @"image/*", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 4);

  // Two Image types
  LoadHtml(false, true, @"png, jpg", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 5);

  // Video with spaces
  LoadHtml(false, true, @"  .mp4  ", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kVideoAccept*/ 8, 1);

  // Audio without dot
  LoadHtml(false, true, @"mp3", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kAudioAccept*/ 10, 1);

  // Archive
  LoadHtml(false, true, @"zip , rar ", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kArchiveAccept*/ 12, 1);

  // Unknown and image
  LoadHtml(false, true, @"unknown, jpg", false);
  total_count++;
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "choose_file"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", total_count);
  histogram_tester.ExpectBucketCount("IOS.Web.FileInput.Clicked",
                                     /*kImageAccept*/ 6, 6);
}

// Tests that no metrics are logged when tapping on a normal button.
TEST_P(ChooseFileJavaScriptFeatureTest, TestNoMetricsLoggingOnButtonClick) {
  base::HistogramTester histogram_tester;
  web::test::LoadHtml(base::SysUTF8ToNSString(kPageHtmlWithButton),
                      web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "button"));
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 0);
}

// Tests that no metrics are logged when tapping on a normal button.
TEST_P(ChooseFileJavaScriptFeatureTest, TestInvalidPayload) {
  base::HistogramTester histogram_tester;
  web::test::LoadHtml(base::SysUTF8ToNSString(kPageHtmlWithButton),
                      web_state());
  // Test synchronisation
  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"window.webkit.messageHandlers['ChooseFileHandler'].postMessage("
      @"{'acceptType':0,'hasMultiple':true,'hasSelectedFile':false});",
      java_script_feature_.get());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"window.webkit.messageHandlers['ChooseFileHandler'].postMessage({});",
      java_script_feature_.get());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"window.webkit.messageHandlers['ChooseFileHandler'].postMessage("
      @"{'acceptType':-2, 'hasMultiple':true,'hasSelectedFile':false});",
      java_script_feature_.get());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"window.webkit.messageHandlers['ChooseFileHandler'].postMessage("
      @"{'acceptType':37, 'hasMultiple':true,'hasSelectedFile':false});",
      java_script_feature_.get());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"window.webkit.messageHandlers['ChooseFileHandler'].postMessage("
      @"{'acceptType':'invalid', 'hasMultiple':true,'hasSelectedFile':false});",
      java_script_feature_.get());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"window.webkit.messageHandlers['ChooseFileHandler'].postMessage("
      @"{'missing':'invalid', 'hasMultiple':true,'hasSelectedFile':false});",
      java_script_feature_.get());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 1);

  // Test synchronisation
  web::test::ExecuteJavaScriptForFeature(
      web_state_.get(),
      @"window.webkit.messageHandlers['ChooseFileHandler'].postMessage("
      @"{'acceptType':0,'hasMultiple':true,'hasSelectedFile':false});",
      java_script_feature_.get());
  histogram_tester.ExpectTotalCount("IOS.Web.FileInput.Clicked", 2);
}

// Tests that `ResetLastChooseFileEvent()` returns the expected file extensions
// and resets the last event.
TEST_P(ChooseFileJavaScriptFeatureTest,
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
  constexpr bool bool_attributes[] = {false, true};

  for (const auto& [accept_attribute, expected_file_extensions] :
       accept_attributes_file_extensions) {
    for (const auto& multiple_attribute : bool_attributes) {
      for (const auto& has_file_attributes : bool_attributes) {
        const bool has_accept = accept_attribute.empty() == false;
        LoadHtml(multiple_attribute, has_accept, @(accept_attribute.c_str()),
                 has_file_attributes);
        ASSERT_TRUE(
            web::test::TapWebViewElementWithId(web_state(), "choose_file"));
        const std::optional<ChooseFileEvent> event =
            ChooseFileEventHolder::GetInstance()->ResetLastChooseFileEvent();
        ASSERT_TRUE(event.has_value());
        EXPECT_EQ(expected_file_extensions, event->accept_file_extensions);
        EXPECT_EQ(multiple_attribute, event->allow_multiple_files);
        EXPECT_EQ(has_file_attributes, event->has_selected_file);
        EXPECT_FALSE(
            ChooseFileEventHolder::GetInstance()->ResetLastChooseFileEvent());
      }
    }
  }
}

// Tests that `ResetLastChooseFileEvent()` returns the expected MIME types and
// resets the last event.
TEST_P(ChooseFileJavaScriptFeatureTest, TestResetLastChooseFileEventMimeTypes) {
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
  constexpr bool bool_attributes[] = {false, true};

  for (const auto& [accept_attribute, expected_mime_types] :
       accept_attributes_mime_types) {
    for (const auto& multiple_attribute : bool_attributes) {
      for (const auto& has_file_attributes : bool_attributes) {
        const bool has_accept = accept_attribute.empty() == false;
        LoadHtml(multiple_attribute, has_accept, @(accept_attribute.c_str()),
                 has_file_attributes);
        ASSERT_TRUE(
            web::test::TapWebViewElementWithId(web_state(), "choose_file"));
        const std::optional<ChooseFileEvent> event =
            ChooseFileEventHolder::GetInstance()->ResetLastChooseFileEvent();
        ASSERT_TRUE(event.has_value());
        EXPECT_EQ(expected_mime_types, event->accept_mime_types);
        EXPECT_EQ(multiple_attribute, event->allow_multiple_files);
        EXPECT_EQ(has_file_attributes, event->has_selected_file);
        EXPECT_FALSE(
            ChooseFileEventHolder::GetInstance()->ResetLastChooseFileEvent());
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    ChooseFileJavaScriptFeatureTest,
    testing::Values(FeatureParam{/* support_simulated_click= */ false,
                                 /* test_simulated_click= */ false},
                    FeatureParam{/* support_simulated_click= */ true,
                                 /* test_simulated_click= */ false},
                    FeatureParam{/* support_simulated_click= */ true,
                                 /* test_simulated_click= */ true}));
