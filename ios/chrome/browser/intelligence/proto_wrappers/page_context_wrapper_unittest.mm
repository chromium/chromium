// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import <vector>

#import "base/base64.h"
#import "base/containers/span.h"
#import "base/files/scoped_temp_dir.h"
#import "base/memory/raw_ptr.h"
#import "base/no_destructor.h"
#import "base/run_loop.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/values_test_util.h"
#import "base/time/time.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/autofill/ios/form_util/remote_frame_registration_java_script_feature.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_test_java_script_feature.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/common/features.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/origin.h"

// TODO(crbug.com/458081684): Move away from all autofill dependencies once
// the migration in ios/web is done for frame registration.

// TODO(crbug.com/475577435): Extend test coverage for Rich Extraction.

namespace {

const char kMainPagePath[] = "/main.html";

// Corresponds to MASKED_TEXT_LENGTH in
// resources/annotated_page_content_extraction.ts.
constexpr size_t MASKED_TEXT_LENGTH = 7;

// A fake web state that can be controlled to simulate PDF generation failures.
class FakeWebStateForFailureTest : public web::FakeWebState {
 public:
  void CreateFullPagePdf(base::OnceCallback<void(NSData*)> callback) override {
    std::move(callback).Run(nil);
  }
};

}  // namespace

// A fake snapshot generator delegate that can be controlled to simulate
// failures.
@interface ControllableFakeSnapshotGeneratorDelegate
    : FakeSnapshotGeneratorDelegate
// This property controls the value returned by -canTakeSnapshotForWebState:
// method of the SnapshotGeneratorDelegate protocol.
@property(nonatomic, assign) BOOL canTakeSnapshot;
@end

@implementation ControllableFakeSnapshotGeneratorDelegate
- (instancetype)init {
  if ((self = [super init])) {
    _canTakeSnapshot = YES;
  }
  return self;
}
- (BOOL)canTakeSnapshotWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
  return self.canTakeSnapshot;
}
@end

struct PrintToStringParamName {
  std::string operator()(const testing::TestParamInfo<bool>& info) const {
    return info.param ? "NewRefactoredVersion" : "OldVersion";
  }
};

// Test fixture to test the PageContextWrapper.
class PageContextWrapperTest : public PlatformTest,
                               public testing::WithParamInterface<bool> {
 protected:
  PageContextWrapperTest()
      : PageContextWrapperTest(
            base::test::TaskEnvironment::TimeSource::DEFAULT) {}
  explicit PageContextWrapperTest(
      base::test::TaskEnvironment::TimeSource time_source)
      : task_environment_(time_source),
        web_client_(std::make_unique<web::FakeWebClient>()) {}
  ~PageContextWrapperTest() override { [snapshot_storage_ shutdown]; }

  void SetUp() override {
    PlatformTest::SetUp();

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(kPageActionMenu);
    enabled_features.push_back(autofill::features::kAutofillAcrossIframesIos);
    if (IsRefactored()) {
      enabled_features.push_back(kPageContextExtractorRefactored);
    } else {
      disabled_features.push_back(kPageContextExtractorRefactored);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build(temp_dir_.GetPath());

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView().frame = CGRectMake(0, 0, 100, 100);
    UIViewController* root_view_controller = [[UIViewController alloc] init];
    root_view_controller.view = web_state_->GetView();
    scoped_window_.Get().rootViewController = root_view_controller;
    web_state_->WasShown();
    web_state_->SetKeepRenderProcessAlive(true);

    SnapshotTabHelper::CreateForWebState(web_state_.get());
    SnapshotSourceTabHelper::CreateForWebState(web_state_.get());
    snapshot_storage_ = CreateSnapshotStorage(temp_dir_.GetPath());
    SnapshotTabHelper* snapshot_tab_helper =
        SnapshotTabHelper::FromWebState(web_state_.get());
    snapshot_tab_helper->SetSnapshotStorage(snapshot_storage_);
    snapshot_delegate_ =
        [[ControllableFakeSnapshotGeneratorDelegate alloc] init];
    snapshot_delegate_.view = web_state_->GetView();
    snapshot_tab_helper->SetDelegate(snapshot_delegate_);

    GetWebClient()->SetJavaScriptFeatures({
        web::FindInPageJavaScriptFeature::GetInstance(),
        extractor_feature(),
        PageContextWrapperTestJavaScriptFeature::GetInstance(),
    });

    // Set the fake env used for testing errors.
    profile2_ = TestProfileIOS::Builder().Build();
    web::test::OverrideJavaScriptFeatures(
        profile2_.get(), {web::FindInPageJavaScriptFeature::GetInstance()});
    fake_web_state_ = std::make_unique<FakeWebStateForFailureTest>();

    // Set fake web frames managers for scenarios to simulate a state where
    // no frames are available from the manager which can be used for testing
    // scenarios where APC/InnerText cannot be retrieved.
    for (web::ContentWorld world : {web::ContentWorld::kIsolatedWorld,
                                    web::ContentWorld::kPageContentWorld}) {
      auto fake_frames_manager = std::make_unique<web::FakeWebFramesManager>();
      static_cast<web::FakeWebState*>(fake_web_state_.get())
          ->SetWebFramesManager(world, std::move(fake_frames_manager));
    }

    // Initialize the helper with the servers.
    page_helper_ = std::make_unique<PageContext>(
        &test_server_, &xorigin_test_server_, &xorigin_test_server_b_,
        &xorigin_test_server_c_);
  }

  // Calls a script on the webview of the web_state() in the right ContentWorld.
  id CallJavascript(const std::string_view script) {
    if (IsRefactored()) {
      return web::test::ExecuteJavaScriptForFeatureAndReturnResult(
          web_state(), base::SysUTF8ToNSString(script), extractor_feature());
    } else {
      return web::test::ExecuteJavaScript(base::SysUTF8ToNSString(script),
                                          web_state());
    }
  }

  // Runs the PageContextWrapper with a default configuration. Use this when the
  // test does not require specific configuration options.
  PageContextWrapperCallbackResponse RunPageContextWrapper(
      web::WebState* web_state,
      void (^configuration_block)(PageContextWrapper*),
      base::TimeDelta timeout = base::Seconds(5)) {
    return RunPageContextWrapperWithConfig(
        web_state, PageContextWrapperConfigBuilder().Build(),
        configuration_block, timeout);
  }

  // Runs the PageContextWrapper with the provided configuration. This method
  // handles the boilerplate of creating the wrapper, setting up the completion
  // callback, running the run loop, and capturing the response. Put in the
  // `configuration_block` anything you need to do on the wrapper before
  // calling `populatePageContextFieldsAsyncWithTimeout`.
  PageContextWrapperCallbackResponse RunPageContextWrapperWithConfig(
      web::WebState* web_state,
      PageContextWrapperConfig config,
      void (^configuration_block)(PageContextWrapper*),
      base::TimeDelta timeout = base::Seconds(5)) {
    base::RunLoop run_loop;
    PageContextWrapperCallbackResponse captured_response;

    PageContextWrapper* wrapper = [[PageContextWrapper alloc]
          initWithWebState:web_state
                    config:config
        completionCallback:base::BindOnce(
                               [](base::RunLoop* run_loop,
                                  PageContextWrapperCallbackResponse*
                                      out_response,
                                  PageContextWrapperCallbackResponse response) {
                                 *out_response = std::move(response);
                                 run_loop->Quit();
                               },
                               &run_loop, &captured_response)];

    if (configuration_block) {
      configuration_block(wrapper);
    }

    [wrapper populatePageContextFieldsAsyncWithTimeout:timeout];
    run_loop.Run();

    return captured_response;
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }
  bool IsRefactored() { return GetParam(); }
  web::WebState* web_state() { return web_state_.get(); }
  PageContextExtractorJavaScriptFeature* extractor_feature() {
    return PageContextExtractorJavaScriptFeature::GetInstance();
  }

  // Getters for fake env.
  TestProfileIOS* profile2() { return profile2_.get(); }
  web::FakeWebState* fake_web_state() { return fake_web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  ScopedKeyWindow scoped_window_;
  web::ScopedTestingWebClient web_client_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  net::EmbeddedTestServer test_server_;
  net::EmbeddedTestServer xorigin_test_server_;
  net::EmbeddedTestServer xorigin_test_server_b_;
  net::EmbeddedTestServer xorigin_test_server_c_;
  id<SnapshotStorage> snapshot_storage_ = nil;
  ControllableFakeSnapshotGeneratorDelegate* snapshot_delegate_ = nil;
  std::unique_ptr<TestProfileIOS> profile2_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<PageContext> page_helper_;
};

// Tests that the page context is correctly populated with the page URL, title,
// inner text, and annotated page content (including iframes).
//
// The page layout is as follows:
//      +----------------------------------+
//      | Main page (Origin M)             |  - Main frame (Origin M)
//      |                                  |    |
//      |   +--------------------------+   |    +-- Iframe 1 (Origin M)
//      |   | Iframe 1 (Origin M)      |   |    |     |
//      |   |   +------------------+   |   |    |     +-- Iframe 3 (Origin M)
//      |   |   | Iframe 3         |   |   |    |
//      |   |   +------------------+   |   |    +-- Iframe 2 (Origin M)
//      |   +--------------------------+   |
//      |   +--------------------------+   |
//      |   | Iframe 2 (Origin M)      |   |
//      |   +--------------------------+   |
//      +----------------------------------+
TEST_P(PageContextWrapperTest, PopulatePageContext) {
  // Define the page structure using building blocks.
  auto page_structure = HtmlPage(
      "Main", Paragraph("Main frame text"),
      Iframe(
          TestOrigin::kMain,
          HtmlPage("Child 1", Paragraph("Child frame 1 text"),
                   Iframe(TestOrigin::kMain,
                          HtmlPage("Child 3", Paragraph("Child frame 3 text")),
                          "iframe_3")),
          "iframe_1"),
      Iframe(TestOrigin::kMain,
             HtmlPage("Child 2", Paragraph("Child frame 2 text")), "iframe_2"));

  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
        wrapper.shouldGetInnerText = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  EXPECT_EQ(page_context->url(), test_server_.GetURL(kMainPagePath).spec());
  EXPECT_EQ(page_context->title(), "Main");
  ASSERT_TRUE(page_context->has_annotated_page_content());
  ASSERT_TRUE(page_context->has_inner_text());

  const auto& inner_text = page_context->inner_text();
  EXPECT_THAT(inner_text, testing::HasSubstr("Main frame text"));
  EXPECT_THAT(inner_text, testing::HasSubstr("Child frame 1 text"));
  EXPECT_THAT(inner_text, testing::HasSubstr("Child frame 2 text"));
  EXPECT_THAT(inner_text, testing::HasSubstr("Child frame 3 text"));

  const auto& annotated_page_content = page_context->annotated_page_content();
  EXPECT_EQ(annotated_page_content.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);

  const auto& root_node = annotated_page_content.root_node();
  EXPECT_EQ(root_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  const auto& main_frame_data = annotated_page_content.main_frame_data();
  EXPECT_EQ(main_frame_data.title(), "Main");
  EXPECT_EQ(main_frame_data.url(), test_server_.GetURL(kMainPagePath).spec());
  const auto& main_frame_origin = main_frame_data.security_origin();
  EXPECT_EQ(main_frame_origin.value(), test_server_.GetOrigin().Serialize());
  EXPECT_FALSE(main_frame_origin.opaque());

  // There should be one text node and two iframe nodes. The nested iframe on
  // the same origin should not be a direct children of the root node.
  ASSERT_EQ(root_node.children_nodes_size(), 3);

  const auto& text_node = root_node.children_nodes(0);
  EXPECT_EQ(text_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(text_node.content_attributes().text_data().text_content(),
            "Main frame text");

  const auto& iframe1_node = root_node.children_nodes(1);
  EXPECT_EQ(iframe1_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe1_frame_data =
      iframe1_node.content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe1_frame_data.title(), "Child 1");
  EXPECT_EQ(iframe1_frame_data.url(),
            page_helper_->GetUrlForId("iframe_1").spec());
  const auto& iframe1_origin = iframe1_frame_data.security_origin();
  EXPECT_EQ(iframe1_origin.value(), test_server_.GetOrigin().Serialize());
  EXPECT_FALSE(iframe1_origin.opaque());

  // Check for nested iframe (Child 3).
  ASSERT_EQ(iframe1_node.children_nodes_size(), 1);
  const auto& iframe1_root_node = iframe1_node.children_nodes(0);
  ASSERT_EQ(iframe1_root_node.children_nodes_size(), 2);
  const auto& iframe1_text_node = iframe1_root_node.children_nodes(0);
  EXPECT_EQ(iframe1_text_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(iframe1_text_node.content_attributes().text_data().text_content(),
            "Child frame 1 text");
  const auto& iframe3_node = iframe1_root_node.children_nodes(1);
  EXPECT_EQ(iframe3_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe3_frame_data =
      iframe3_node.content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe3_frame_data.title(), "Child 3");
  EXPECT_EQ(iframe3_frame_data.url(),
            page_helper_->GetUrlForId("iframe_3").spec());

  const auto& iframe3_origin = iframe3_frame_data.security_origin();
  EXPECT_EQ(iframe3_origin.value(), test_server_.GetOrigin().Serialize());
  EXPECT_FALSE(iframe3_origin.opaque());

  ASSERT_EQ(iframe3_node.children_nodes_size(), 1);
  const auto& iframe3_root_node = iframe3_node.children_nodes(0);
  ASSERT_EQ(iframe3_root_node.children_nodes_size(), 1);
  const auto& iframe3_text_node = iframe3_root_node.children_nodes(0);
  EXPECT_EQ(iframe3_text_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(iframe3_text_node.content_attributes().text_data().text_content(),
            "Child frame 3 text");

  const auto& iframe2_node = root_node.children_nodes(2);
  EXPECT_EQ(iframe2_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe2_frame_data =
      iframe2_node.content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe2_frame_data.title(), "Child 2");
  EXPECT_EQ(iframe2_frame_data.url(),
            page_helper_->GetUrlForId("iframe_2").spec());
  const auto& iframe2_origin = iframe2_frame_data.security_origin();
  EXPECT_EQ(iframe2_origin.value(), test_server_.GetOrigin().Serialize());
  EXPECT_FALSE(iframe2_origin.opaque());

  ASSERT_EQ(iframe2_node.children_nodes_size(), 1);
  const auto& iframe2_root_node = iframe2_node.children_nodes(0);
  ASSERT_EQ(iframe2_root_node.children_nodes_size(), 1);
  const auto& iframe2_text_node = iframe2_root_node.children_nodes(0);
  EXPECT_EQ(iframe2_text_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(iframe2_text_node.content_attributes().text_data().text_content(),
            "Child frame 2 text");
}

// Tests that the completion callback is called even when no async fields are
// requested.
TEST_P(PageContextWrapperTest, PopulatePageContext_NoFieldsRequested) {
  auto page_structure = HtmlPage("No Fields", Paragraph("Hello"));

  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), nil);

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  EXPECT_EQ(page_context->url(), test_server_.GetURL(kMainPagePath).spec());
  EXPECT_EQ(page_context->title(), "No Fields");
  EXPECT_FALSE(page_context->has_inner_text());
  EXPECT_FALSE(page_context->has_annotated_page_content());
  EXPECT_FALSE(page_context->has_tab_screenshot());
  EXPECT_FALSE(page_context->has_pdf_data());
}

// Tests that the page context is correctly populated with the page content as a
// PDF.
TEST_P(PageContextWrapperTest, PopulatePageContextWithPDFVerification) {
  auto page_structure = HtmlPage("", Paragraph("Hello PDF"));
  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetFullPagePDF = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_pdf_data());
  const std::string& base64_pdf_data = page_context->pdf_data();
  EXPECT_FALSE(base64_pdf_data.empty());

  std::string decoded_pdf_data;
  ASSERT_TRUE(base::Base64Decode(base64_pdf_data, &decoded_pdf_data));

  // Check if the data starts with the PDF header.
  const std::string pdf_header = "%PDF-";
  EXPECT_EQ(decoded_pdf_data.rfind(pdf_header, 0), 0u);
}

// Tests that the page context is correctly populated with a snapshot of the
// page.
TEST_P(PageContextWrapperTest, PopulatePageContextWithSnapshotVerification) {
  auto page_structure = HtmlPage("", Paragraph("Hello Snapshot"));
  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetSnapshot = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_tab_screenshot());
  const std::string& base64_screenshot_data = page_context->tab_screenshot();
  EXPECT_FALSE(base64_screenshot_data.empty());

  std::string decoded_screenshot_data;
  ASSERT_TRUE(
      base::Base64Decode(base64_screenshot_data, &decoded_screenshot_data));

  // Check if the data starts with the PNG header.
  const std::string png_header = "\x89PNG\r\n\x1a\n";
}

// Tests that the page context can take a snapshot of the page with specified
// text highlighted. Verifies that highlighting was applied in both the
// DOM and in the snapshot.
TEST_P(PageContextWrapperTest, PopulatePageContextWithTextHighlighting) {
  auto page_structure = HtmlPage("", Paragraph("Hello Highlight"));
  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  // Inject a MutationObserver to detect when the highlight is applied.
  const char kMutationObserverScript[] = R"(
      window.__highlight_detected = false;
      const observer = new MutationObserver((mutations) => {
        for (const mutation of mutations) {
          for (const node of mutation.addedNodes) {
            if (node.nodeType === Node.ELEMENT_NODE &&
                node.classList.contains('find_in_page')) {
              window.__highlight_detected = true;
              observer.disconnect();
              return;
            }
          }
        }
      });
      observer.observe(document.body, { childList: true, subtree: true });
  )";
  CallJavascript(kMutationObserverScript);

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetSnapshot = YES;
        wrapper.textToHighlight = @"Highlight";
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_tab_screenshot());

  // Check that the highlighting was applied from DOM mutations. This is the
  // most reliable and less flaky way for verifying that highlighting was
  // triggered.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^{
    id result = CallJavascript("window.__highlight_detected");
    return [result boolValue];
  }));
}

// Tests that anchor tags are correctly extracted when the feature is enabled.
TEST_P(PageContextWrapperTest, PopulatePageContextWithAnchors) {
  auto page_structure =
      HtmlPage("", RawHtml("<a href=\"http://foo.com\">foo</a>"));
  std::string main_html = page_helper_->Build(page_structure);

  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_annotated_page_content());

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();
  ASSERT_EQ(root_node.children_nodes_size(), 2);

  const auto& text_node = root_node.children_nodes(0);
  EXPECT_EQ(text_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(text_node.content_attributes().text_data().text_content(), "foo");

  const auto& anchor_node = root_node.children_nodes(1);
  EXPECT_EQ(anchor_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR);
  EXPECT_EQ(anchor_node.content_attributes().anchor_data().url(),
            "http://foo.com/");
  ASSERT_EQ(anchor_node.children_nodes_size(), 1);

  const auto& anchor_text_node = anchor_node.children_nodes(0);
  EXPECT_EQ(anchor_text_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(anchor_text_node.content_attributes().text_data().text_content(),
            "foo");
}

// Tests that the wrapper correctly handles a failure in one of the async tasks.
TEST_P(PageContextWrapperTest, PopulatePageContext_SnapshotFailure) {
  auto page_structure = HtmlPage("", Paragraph("Hello"));
  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  // Set the snapshot delegate to cause a failure.
  snapshot_delegate_.canTakeSnapshot = NO;

  PageContextWrapperCallbackResponse captured_response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetSnapshot = YES;
      });

  // Verify that the callback was called with a screenshot error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(),
            PageContextWrapperError::kScreenshotError);
}

// Tests that the wrapper correctly handles a force detach signal from the
// JavaScript feature.
TEST_P(PageContextWrapperTest, PopulatePageContext_ForceDetach) {
  auto page_structure = HtmlPage("", Paragraph("Hello"));
  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  // Set up the feature to return a script that forces detachment.
  const char kForceDetachScript[] = R"(
      if (!window.__gCrWeb) { window.__gCrWeb = {}; }
      if (!window.__gCrWeb.pageContext) { window.__gCrWeb.pageContext = {}; }
      window.__gCrWeb.pageContext.shouldDetach = true;
  )";
  CallJavascript(kForceDetachScript);

  PageContextWrapperCallbackResponse captured_response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  // Verify that the callback was called with a force detach error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(),
            PageContextWrapperError::kForceDetachError);
}

// Tests that the wrapper correctly times out if the async operations take too
// long. Keep the extraction busy by using an infinite while loop as the should
// detach script.
TEST_P(PageContextWrapperTest, TimeoutVerification) {
  auto page_structure = HtmlPage("", Paragraph("Hello"));
  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  // Inject JS to cause a timeout.
  const char kTimeoutScript[] = R"(
      if (!window.__gCrWeb) { window.__gCrWeb = {}; }
      if (!window.__gCrWeb.pageContext) { window.__gCrWeb.pageContext = {}; }
      window.__gCrWeb.pageContext.shouldTimeout = true;
  )";
  CallJavascript(kTimeoutScript);

  PageContextWrapperCallbackResponse captured_response = RunPageContextWrapper(
      web_state(),
      ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetInnerText = YES;
      },
      base::Milliseconds(5));

  // Fast forward time past the timeout duration to trigger the timeout.
  // task_environment_.FastForwardBy(base::Milliseconds(150));

  // Verify that the callback was called with a timeout error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(), PageContextWrapperError::kTimeout);
}

// Tests that the wrapper correctly handles a failure in PDF generation.
TEST_P(PageContextWrapperTest, PopulatePageContext_PDFGenerationFailure) {
  PageContextWrapperCallbackResponse captured_response =
      RunPageContextWrapper(fake_web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetFullPagePDF = YES;
      });

  // Verify that the callback was called with a PDF data error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(), PageContextWrapperError::kPDFDataError);
}

// Tests that the wrapper correctly handles a failure in APC generation.
TEST_P(PageContextWrapperTest, PopulatePageContext_APCGenerationFailure) {
  PageContextWrapperCallbackResponse captured_response =
      RunPageContextWrapper(fake_web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  // Verify that the callback was called with an APC error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(), PageContextWrapperError::kAPCError);
}

// Tests that the wrapper correctly handles a failure in inner text generation.
TEST_P(PageContextWrapperTest, PopulatePageContext_InnerTextGenerationFailure) {
  PageContextWrapperCallbackResponse captured_response =
      RunPageContextWrapper(fake_web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetInnerText = YES;
      });

  // Verify that the callback was called with an inner text error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(),
            PageContextWrapperError::kInnerTextError);
}

// Tests that extraction works across origins.
// The page layout is as follows:
//      +----------------------------------+
//      | Main page (Origin M)             |  - Main frame (Origin M)
//      |                                  |    |
//      |   +--------------------------+   |    +-- Iframe (Origin A)
//      |   | Iframe (Origin A)        |   |
//      |   +--------------------------+   |
//      |                                  |
//      +----------------------------------+
TEST_P(PageContextWrapperTest, PopulatePageContextWithCrossOriginFrame) {
  auto page_structure =
      HtmlPage("Main Cross Origin", Paragraph("Main frame cross-origin text"),
               Iframe(TestOrigin::kCrossA,
                      HtmlPage("Child Cross Origin",
                               Paragraph("Child frame cross-origin text")),
                      "iframe_0"));

  std::string main_html = page_helper_->Build(page_structure);
  GURL main_url = test_server_.GetURL(kMainPagePath);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html), main_url,
                      web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
        wrapper.shouldGetInnerText = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  EXPECT_EQ(page_context->url(), main_url.spec());
  EXPECT_EQ(page_context->title(), "Main Cross Origin");
  ASSERT_TRUE(page_context->has_annotated_page_content());
  ASSERT_TRUE(page_context->has_inner_text());

  const auto& inner_text = page_context->inner_text();
  EXPECT_THAT(inner_text, testing::HasSubstr("Main frame cross-origin text"));
  EXPECT_THAT(inner_text, testing::HasSubstr("Child frame cross-origin text"));

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();
  // There should be one text node and one iframe node.
  ASSERT_EQ(root_node.children_nodes_size(), 2);

  const optimization_guide::proto::ContentNode* text_node = nullptr;
  const optimization_guide::proto::ContentNode* iframe_node = nullptr;

  for (const auto& node : root_node.children_nodes()) {
    if (node.content_attributes().attribute_type() ==
        optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT) {
      text_node = &node;
    } else if (node.content_attributes().attribute_type() ==
               optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
      iframe_node = &node;
    }
  }

  ASSERT_TRUE(text_node);
  ASSERT_TRUE(iframe_node);

  EXPECT_EQ(text_node->content_attributes().text_data().text_content(),
            "Main frame cross-origin text");

  const auto& iframe_frame_data =
      iframe_node->content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe_frame_data.title(), "Child Cross Origin");
  std::string iframe_url = page_helper_->GetUrlForId("iframe_0").spec();
  EXPECT_EQ(iframe_frame_data.url(), iframe_url);

  ASSERT_EQ(iframe_node->children_nodes_size(), 1);
  const auto& iframe_root_node = iframe_node->children_nodes(0);
  EXPECT_EQ(iframe_root_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
  ASSERT_EQ(iframe_root_node.children_nodes_size(), 1);
  const auto& iframe_text_node = iframe_root_node.children_nodes(0);
  EXPECT_EQ(iframe_text_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(iframe_text_node.content_attributes().text_data().text_content(),
            "Child frame cross-origin text");
}

// Tests that extraction works across origins with nested same-origin frames.
// The page layout is as follows:
//      +----------------------------------+
//      | Main page (Origin M)             |  - Main frame (Origin M)
//      |                                  |    |
//      |   +--------------------------+   |    +-- Iframe 1 (Origin A)
//      |   | Iframe 1 (Origin A)      |   |    |     |
//      |   |   +------------------+   |   |    |     +-- Iframe 2 (Origin A)
//      |   |   | Iframe 2         |   |   |
//      |   |   +------------------+   |   |
//      |   +--------------------------+   |
//      |                                  |
//      +----------------------------------+
TEST_P(PageContextWrapperTest,
       PopulatePageContextWithNestedSameCrossOriginFrame) {
  auto page_structure = HtmlPage(
      "Main Cross Origin", Paragraph("Main frame cross-origin text"),
      Iframe(TestOrigin::kCrossA,
             HtmlPage("Child Cross Origin 1",
                      Paragraph("Child frame cross-origin text 1"),
                      Iframe(TestOrigin::kCrossA,
                             HtmlPage("Child Cross Origin 2",
                                      Paragraph("Child frame cross-origin "
                                                "text 2")),
                             "iframe_2"))));

  std::string main_html = page_helper_->Build(page_structure);
  GURL main_url = test_server_.GetURL(kMainPagePath);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html), main_url,
                      web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
        wrapper.shouldGetInnerText = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);

  const auto& inner_text = page_context->inner_text();
  EXPECT_THAT(inner_text, testing::HasSubstr("Main frame cross-origin text"));
  EXPECT_THAT(inner_text,
              testing::HasSubstr("Child frame cross-origin text 1"));
  EXPECT_THAT(inner_text,
              testing::HasSubstr("Child frame cross-origin text 2"));

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();
  ASSERT_EQ(root_node.children_nodes_size(), 2);

  const optimization_guide::proto::ContentNode* iframe_node = nullptr;
  for (const auto& node : root_node.children_nodes()) {
    if (node.content_attributes().attribute_type() ==
        optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
      iframe_node = &node;
    }
  }
  ASSERT_TRUE(iframe_node);

  const auto& iframe_frame_data =
      iframe_node->content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe_frame_data.title(), "Child Cross Origin 1");

  ASSERT_EQ(iframe_node->children_nodes_size(), 1);
  const auto& iframe_root_node = iframe_node->children_nodes(0);
  ASSERT_EQ(iframe_root_node.children_nodes_size(), 2);

  const auto& nested_iframe_node = iframe_root_node.children_nodes(1);
  EXPECT_EQ(nested_iframe_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& nested_iframe_frame_data =
      nested_iframe_node.content_attributes().iframe_data().frame_data();
  EXPECT_EQ(nested_iframe_frame_data.title(), "Child Cross Origin 2");
  std::string nested_iframe_url = page_helper_->GetUrlForId("iframe_2").spec();
  EXPECT_EQ(nested_iframe_frame_data.url(), nested_iframe_url);
}

// Tests that extraction works across origins with nested different-origin
// frames.
//      +----------------------------------+
//      | Main page (test_server_)         |  - Main frame (Origin M)
//      |                                  |    |
//      |   +--------------------------+   |    +-- Iframe A (Origin A)
//      |   | Iframe A (Origin A)      |   |    |     |
//      |   |                          |   |    |     +-- Iframe B (Origin B)
//      |   |   +------------------+   |   |
//      |   |   | Iframe B         |   |   |
//      |   |   | (Origin B)       |   |   |
//      |   |   +------------------+   |   |
//      |   |                          |   |
//      |   +--------------------------+   |
//      |                                  |
//      +----------------------------------+
TEST_P(PageContextWrapperTest,
       PopulatePageContextWithNestedDifferentCrossOriginFrame) {
  auto page_structure = HtmlPage(
      "Main Cross Origin", Paragraph("Main frame cross-origin text"),
      Iframe(TestOrigin::kCrossA,
             HtmlPage("Child Cross Origin A",
                      Paragraph("Child frame cross-origin text A"),
                      Iframe(TestOrigin::kCrossB,
                             HtmlPage("Child Cross Origin B",
                                      Paragraph("Child frame cross-origin "
                                                "text B")),
                             "iframe_b")),
             "iframe_a"));

  std::string main_html = page_helper_->Build(page_structure);
  GURL main_url = test_server_.GetURL(kMainPagePath);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html), main_url,
                      web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
        wrapper.shouldGetInnerText = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);

  const auto& inner_text = page_context->inner_text();
  EXPECT_THAT(inner_text, testing::HasSubstr("Main frame cross-origin text"));
  EXPECT_THAT(inner_text,
              testing::HasSubstr("Child frame cross-origin text A"));
  EXPECT_THAT(inner_text,
              testing::HasSubstr("Child frame cross-origin text B"));

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();
  ASSERT_EQ(root_node.children_nodes_size(), 3);

  const optimization_guide::proto::ContentNode* text_node = nullptr;
  const optimization_guide::proto::ContentNode* iframe_a_node = nullptr;
  const optimization_guide::proto::ContentNode* iframe_b_node = nullptr;

  for (const auto& node : root_node.children_nodes()) {
    if (node.content_attributes().attribute_type() ==
        optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT) {
      text_node = &node;
    } else if (node.content_attributes().attribute_type() ==
               optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
      auto title = node.content_attributes().iframe_data().frame_data().title();
      if (title == "Child Cross Origin A") {
        iframe_a_node = &node;
      } else if (title == "Child Cross Origin B") {
        iframe_b_node = &node;
      }
    }
  }

  ASSERT_TRUE(text_node);
  ASSERT_TRUE(iframe_a_node);
  ASSERT_TRUE(iframe_b_node);

  EXPECT_EQ(text_node->content_attributes().text_data().text_content(),
            "Main frame cross-origin text");

  // Verify iframe A.
  const auto& iframe_a_frame_data =
      iframe_a_node->content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe_a_frame_data.title(), "Child Cross Origin A");
  EXPECT_EQ(iframe_a_frame_data.url(),
            page_helper_->GetUrlForId("iframe_a").spec());

  ASSERT_EQ(iframe_a_node->children_nodes_size(), 1);
  const auto& iframe_a_root_node = iframe_a_node->children_nodes(0);
  ASSERT_EQ(iframe_a_root_node.children_nodes_size(), 1);
  const auto& iframe_a_text_node = iframe_a_root_node.children_nodes(0);
  EXPECT_EQ(iframe_a_text_node.content_attributes().text_data().text_content(),
            "Child frame cross-origin text A");

  // Verify iframe B.
  const auto& iframe_b_frame_data =
      iframe_b_node->content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe_b_frame_data.title(), "Child Cross Origin B");
  EXPECT_EQ(iframe_b_frame_data.url(),
            page_helper_->GetUrlForId("iframe_b").spec());

  ASSERT_EQ(iframe_b_node->children_nodes_size(), 1);
  const auto& iframe_b_root_node = iframe_b_node->children_nodes(0);
  ASSERT_EQ(iframe_b_root_node.children_nodes_size(), 1);
  const auto& iframe_b_text_node = iframe_b_root_node.children_nodes(0);
  EXPECT_EQ(iframe_b_text_node.content_attributes().text_data().text_content(),
            "Child frame cross-origin text B");
}

// Tests that the wrapper correctly handles a destroyed WebState during a forced
// snapshot update.
TEST_P(PageContextWrapperTest,
       PopulatePageContext_WebStateDestroyedDuringForcedSnapshot) {
  // Capture pointer to web_state_ to allow binding in the block.
  auto* web_state_ptr = &web_state_;
  PageContextWrapperCallbackResponse captured_response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetSnapshot = YES;
        wrapper.shouldForceUpdateMissingSnapshots = YES;

        // Simulate a snapshot failure, which will trigger a forced update.
        snapshot_delegate_.canTakeSnapshot = NO;

        // Make the web_state hidden to trigger the async snapshot retrieval
        // path.
        web_state()->WasHidden();

        // Destroy the web state immediately after the async work has started
        // (by posting a task to run after the wrapper's method returns).
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(
                           [](std::unique_ptr<web::WebState>* web_state) {
                             web_state->reset();
                           },
                           web_state_ptr));
      });

  // Verify that the callback was called with a generic error because the
  // WebState was destroyed during the operation.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(), PageContextWrapperError::kGenericError);
}

// Tests that the wrapper correctly handles a destroyed WebState.
TEST_P(PageContextWrapperTest, PopulatePageContext_WebStateDestroyed) {
  PageContextWrapperCallbackResponse captured_response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetSnapshot = YES;
        wrapper.shouldGetAnnotatedPageContent = YES;
        wrapper.shouldGetInnerText = YES;
        wrapper.shouldGetFullPagePDF = YES;

        // Destroy the web state after initializing the wrapper.
        web_state_.reset();
      });

  // Verify that the callback was called with a generic error because the
  // WebState was destroyed.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(), PageContextWrapperError::kGenericError);
}

// Tests that the page context correctly handles data URLs by truncating them.
TEST_P(PageContextWrapperTest, PopulatePageContext_DataURL) {
  const std::string data_url = "data:text/html,<p>Hello Data</p>";
  web::test::LoadHtml(@"<p>Hello Data</p>", GURL(data_url), web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  EXPECT_EQ(page_context->url(), "data:");
  ASSERT_TRUE(page_context->has_annotated_page_content());
  const auto& main_frame_data =
      page_context->annotated_page_content().main_frame_data();
  EXPECT_EQ(main_frame_data.url(), "data:");
}

// Tests that an iframe with a data URL is treated as cross-origin (opaque
// origin).
TEST_P(PageContextWrapperTest, PopulatePageContext_DataURLIframe) {
  const std::string data_iframe_content =
      "<html><body><p>Data Iframe</p></body></html>";
  const std::string data_iframe_url = "data:text/html," + data_iframe_content;

  auto page_structure =
      HtmlPage("Main", Paragraph("Main"), Iframe(data_iframe_url));
  std::string main_html = page_helper_->Build(page_structure);

  // We need to use a real URL for the main frame to have a distinct origin.
  GURL main_url = test_server_.GetURL(kMainPagePath);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html), main_url,
                      web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_annotated_page_content());

  const auto& root_node = page_context->annotated_page_content().root_node();

  // Find the iframe node.
  const optimization_guide::proto::ContentNode* iframe_node = nullptr;
  for (const auto& node : root_node.children_nodes()) {
    if (node.content_attributes().attribute_type() ==
        optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
      iframe_node = &node;
      break;
    }
  }
  ASSERT_TRUE(iframe_node);

  // Check the URL of the iframe.
  EXPECT_EQ(iframe_node->content_attributes().iframe_data().frame_data().url(),
            "data:");

  // Check security origin
  // The main frame origin should be http://127.0.0.1:...
  // The iframe origin should be opaque (unique).
  const auto& main_frame_data =
      page_context->annotated_page_content().main_frame_data();
  const auto& iframe_frame_data =
      iframe_node->content_attributes().iframe_data().frame_data();

  // Main frame should NOT be opaque.
  EXPECT_FALSE(main_frame_data.security_origin().opaque());
  EXPECT_FALSE(main_frame_data.security_origin().value().empty());

  // Iframe should be opaque.
  EXPECT_TRUE(iframe_frame_data.security_origin().opaque());
  EXPECT_FALSE(iframe_frame_data.security_origin().value().empty());

  // The value of an opaque origin is a nonce, so it should be different from
  // the main frame's value.
  EXPECT_NE(main_frame_data.security_origin().value(),
            iframe_frame_data.security_origin().value());
}

// Tests that SVG anchor tags are correctly extracted which are special anchors
// that have a .href that isn't a string which require special
// handling. This is to validate that http://crbug.com/475208453 is fixed.
TEST_P(PageContextWrapperTest, PopulatePageContextWithSVGAnchors) {
  auto page_structure = HtmlPage(
      "", RawHtml("<svg width=\"200\" height=\"40\">"
                  "  <a href=\"http://example.com\">"
                  "    <text x=\"10\" y=\"25\" fill=\"blue\">SVG Text</text>"
                  "  </a>"
                  "</svg>"));
  std::string main_html = page_helper_->Build(page_structure);

  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperCallbackResponse response =
      RunPageContextWrapper(web_state(), ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_annotated_page_content());

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();

  // Verify that the SVG anchor is extracted.
  bool found_anchor = false;
  for (const auto& node : root_node.children_nodes()) {
    if (node.content_attributes().attribute_type() ==
        optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR) {
      found_anchor = true;
      EXPECT_EQ(node.content_attributes().anchor_data().url(),
                "http://example.com");
    }
  }
  EXPECT_TRUE(found_anchor);
}

// Tests that extraction can preserve the tree structure for nested frames on
// different origins when frame grafting is enabled.
//      +----------------------------------+
//      | Main page (test_server_)         |  - Main frame (Origin M)
//      |                                  |    |
//      |   +--------------------------+   |    +-- Iframe A (Origin A)
//      |   | Iframe A (Origin A)      |   |    |     |
//      |   |                          |   |    |     +-- Iframe B (Origin B)
//      |   |   +------------------+   |   |
//      |   |   | Iframe B         |   |   |
//      |   |   | (Origin B)       |   |   |
//      |   |   +------------------+   |   |
//      |   |                          |   |
//      |   +--------------------------+   |
//      |                                  |
//      +----------------------------------+
TEST_P(PageContextWrapperTest,
       PopulatePageContextWithNestedDifferentCrossOriginFrame_GraftingEnabled) {
  if (!IsRefactored()) {
    GTEST_SKIP()
        << "Frame grafter not supported for the non-refactored APC wrapper";
  }

  auto page_structure = HtmlPage(
      "Main Cross Origin", Paragraph("Main frame cross-origin text"),
      Iframe(TestOrigin::kCrossA,
             HtmlPage("Child Cross Origin A",
                      Paragraph("Child frame cross-origin text A"),
                      Iframe(TestOrigin::kCrossB,
                             HtmlPage("Child Cross Origin B",
                                      Paragraph("Child frame cross-origin "
                                                "text B")),
                             "iframe_b")),
             "iframe_a"));

  std::string main_html = page_helper_->Build(page_structure);
  GURL main_url = test_server_.GetURL(kMainPagePath);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html), main_url,
                      web_state());

  // Wait for all frames to load.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^{
    return web_state()
               ->GetWebFramesManager(web::ContentWorld::kIsolatedWorld)
               ->GetAllWebFrames()
               .size() == 3;
  }));

  GURL iframe_a_url = page_helper_->GetUrlForId("iframe_a");
  GURL iframe_b_url = page_helper_->GetUrlForId("iframe_b");

  PageContextWrapperConfig config = PageContextWrapperConfigBuilder()
                                        .SetGraftCrossOriginFrameContent(true)
                                        .Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
        wrapper.shouldGetInnerText = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);

  const auto& inner_text = page_context->inner_text();
  EXPECT_THAT(inner_text, testing::HasSubstr("Main frame cross-origin text"));
  EXPECT_THAT(inner_text,
              testing::HasSubstr("Child frame cross-origin text A"));
  EXPECT_THAT(inner_text,
              testing::HasSubstr("Child frame cross-origin text B"));

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();
  ASSERT_EQ(root_node.children_nodes_size(), 2);

  const optimization_guide::proto::ContentNode* text_node = nullptr;
  const optimization_guide::proto::ContentNode* iframe_a_node = nullptr;

  for (const auto& node : root_node.children_nodes()) {
    if (node.content_attributes().attribute_type() ==
        optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT) {
      text_node = &node;
    } else if (node.content_attributes().attribute_type() ==
               optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
      if (node.content_attributes().iframe_data().frame_data().url() ==
          iframe_a_url.spec()) {
        iframe_a_node = &node;
      }
    }
  }

  ASSERT_TRUE(text_node);
  ASSERT_TRUE(iframe_a_node);

  EXPECT_EQ(text_node->content_attributes().text_data().text_content(),
            "Main frame cross-origin text");

  // Verify iframe A.
  const auto& iframe_a_frame_data =
      iframe_a_node->content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe_a_frame_data.title(), "Child Cross Origin A");
  ASSERT_EQ(iframe_a_node->children_nodes_size(), 1);
  const auto& iframe_a_root_node = iframe_a_node->children_nodes(0);
  ASSERT_EQ(iframe_a_root_node.children_nodes_size(), 2);

  const auto& iframe_a_text_node = iframe_a_root_node.children_nodes(0);
  EXPECT_EQ(iframe_a_text_node.content_attributes().text_data().text_content(),
            "Child frame cross-origin text A");

  // Verify iframe B is nested inside iframe A.
  const auto& iframe_b_node = iframe_a_root_node.children_nodes(1);
  EXPECT_EQ(iframe_b_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_b_frame_data =
      iframe_b_node.content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe_b_frame_data.title(), "Child Cross Origin B");
  EXPECT_EQ(iframe_b_frame_data.url(), iframe_b_url.spec());
  ASSERT_EQ(iframe_b_node.children_nodes_size(), 1);
  const auto& iframe_b_root_node = iframe_b_node.children_nodes(0);
  ASSERT_EQ(iframe_b_root_node.children_nodes_size(), 1);
  const auto& iframe_b_text_node = iframe_b_root_node.children_nodes(0);
  EXPECT_EQ(iframe_b_text_node.content_attributes().text_data().text_content(),
            "Child frame cross-origin text B");
}

// Tests that the grafter correctly handles unregistered frame content.
TEST_P(PageContextWrapperTest,
       PopulatePageContextWithOrphanFrame_GraftingEnabled) {
  if (!IsRefactored()) {
    GTEST_SKIP()
        << "Frame grafter not supported for the non-refactored APC wrapper";
  }

  auto page_structure = HtmlPage(
      "Main Title", Paragraph("Main frame text"),
      Iframe(TestOrigin::kCrossA,
             HtmlPage("Child Cross Origin A",
                      Paragraph("Child frame cross-origin text A"),
                      Iframe(TestOrigin::kCrossB,
                             HtmlPage("Child Cross Origin B",
                                      Paragraph("Child frame cross-origin "
                                                "text B")),
                             "iframe_b")),
             "iframe_a"));

  std::string main_html = page_helper_->Build(page_structure);
  GURL main_url = test_server_.GetURL(kMainPagePath);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html), main_url,
                      web_state());

  // Wait for all frames to load.
  web::WebFramesManager* frames_manager =
      web_state()->GetWebFramesManager(web::ContentWorld::kIsolatedWorld);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^{
    return frames_manager->GetAllWebFrames().size() == 3;
  }));

  // We use the URL to find the frame because we don't have the frame ID easily
  // exposed by the helper.
  GURL iframe_a_url = page_helper_->GetUrlForId("iframe_a");
  GURL iframe_b_url = page_helper_->GetUrlForId("iframe_b");

  // Sabotage remote frame discovery in frame A (it won't see frame B hence not
  // claim its content which will remain orphan).
  web::WebFrame* iframe_a = nullptr;
  for (web::WebFrame* frame : frames_manager->GetAllWebFrames()) {
    if (frame->GetSecurityOrigin() == url::Origin::Create(iframe_a_url)) {
      iframe_a = frame;
      break;
    }
  }
  ASSERT_TRUE(iframe_a);
  ASSERT_TRUE(iframe_a->ExecuteJavaScript(
      u"Element.prototype.getElementsByTagName = () => []; true"));

  PageContextWrapperConfig config = PageContextWrapperConfigBuilder()
                                        .SetGraftCrossOriginFrameContent(true)
                                        .Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();
  // Expect text node, iframe_a node, and a placeholder for iframe_b.
  ASSERT_EQ(root_node.children_nodes_size(), 3);

  const optimization_guide::proto::ContentNode* text_node = nullptr;
  const optimization_guide::proto::ContentNode* iframe_a_node = nullptr;
  const optimization_guide::proto::ContentNode* iframe_b_node = nullptr;

  for (const auto& node : root_node.children_nodes()) {
    if (node.content_attributes().attribute_type() ==
        optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT) {
      text_node = &node;
    } else if (node.content_attributes().iframe_data().frame_data().url() ==
               iframe_a_url.spec()) {
      iframe_a_node = &node;
    } else if (node.content_attributes().iframe_data().frame_data().url() ==
               iframe_b_url.spec()) {
      iframe_b_node = &node;
    }
  }

  ASSERT_TRUE(text_node);
  ASSERT_TRUE(iframe_a_node);
  ASSERT_TRUE(iframe_b_node);

  // Verify main frame text.
  EXPECT_EQ(text_node->content_attributes().text_data().text_content(),
            "Main frame text");

  // Verify iframe A.
  EXPECT_EQ(iframe_a_node->content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_a_frame_data =
      iframe_a_node->content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe_a_frame_data.title(), "Child Cross Origin A");
  ASSERT_EQ(iframe_a_node->children_nodes_size(), 1);
  const auto& iframe_a_root_node = iframe_a_node->children_nodes(0);
  ASSERT_EQ(iframe_a_root_node.children_nodes_size(), 1);

  const auto& iframe_a_text_node = iframe_a_root_node.children_nodes(0);
  EXPECT_EQ(iframe_a_text_node.content_attributes().text_data().text_content(),
            "Child frame cross-origin text A");

  // Verify iframe B is nested inside the main frame content instead of frame A
  // because the frame B was left orphan by the grafter.
  EXPECT_EQ(iframe_b_node->content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_b_frame_data =
      iframe_b_node->content_attributes().iframe_data().frame_data();
  EXPECT_EQ(iframe_b_frame_data.title(), "Child Cross Origin B");
  EXPECT_EQ(iframe_b_frame_data.url(), iframe_b_url.spec());
  ASSERT_EQ(iframe_b_node->children_nodes_size(), 1);
  const auto& iframe_b_root_node = iframe_b_node->children_nodes(0);
  ASSERT_EQ(iframe_b_root_node.children_nodes_size(), 1);
  const auto& iframe_b_text_node = iframe_b_root_node.children_nodes(0);
  EXPECT_EQ(iframe_b_text_node.content_attributes().text_data().text_content(),
            "Child frame cross-origin text B");
}

// Tests that extraction with frame grafting works across origins with a complex
// frame structure. Remote frame registration is disabled for Iframe D to make
// it an unregistered frame that should be directly grafted under the main
// frame.
//      +----------------------------------+
//      | Main page (Origin M)             |  - Main frame (Origin M)
//      |                                  |    |
//      |   +--------------------------+   |    +-- Iframe A (Origin A)
//      |   | Iframe A (Origin A)      |   |    |     |
//      |   |  +------------------+    |   |    |     +-- Iframe A1 (Origin A)
//      |   |  | Iframe A1        |    |   |    |     |
//      |   |  +------------------+    |   |    |     +-- Iframe B (Origin B)
//      |   |  +------------------+    |   |    |        |
//      |   |  | Iframe B         |    |   |    |        +-- Iframe B1
//      |   |  | (Origin B)       |    |   |    |
//      |   |  |  +------------+  |    |   |    |        |   (Origin B)
//      |   |  |  | Iframe B1  |  |    |   |    |        |
//      |   |  |  +------------+  |    |   |    |        +-- Iframe D
//      |   |  |                  |    |   |    |            (Origin C)
//      |   |  |  +------------+  |    |   |    |            Orphan frame
//      |   |  |  | Iframe D   |  |    |   |    |
//      |   |  |  | (Origin C) |  |    |   |    |
//      |   |  |  +------------+  |    |   |    |
//      |   |  +------------------+    |   |    |
//      |   +--------------------------+   |    |
//      |   +--------------------------+   |    +-- Iframe C (Origin B)
//      |   | Iframe C (Origin B)      |   |
//      |   +--------------------------+   |
//      +----------------------------------+
TEST_P(PageContextWrapperTest,
       PopulatePageContextWithComplexFrameTree_GraftingEnabled) {
  if (!IsRefactored()) {
    GTEST_SKIP()
        << "Frame grafter not supported for the non-refactored APC wrapper";
  }

  auto page_structure = HtmlPage(
      "Main", Paragraph("Main frame text"),
      Iframe(
          TestOrigin::kCrossA,
          HtmlPage(
              "Child A", Paragraph("Child frame A text"),
              Iframe(TestOrigin::kCrossA,
                     HtmlPage("Child A1", Paragraph("Child frame A1 text"))),
              Iframe(TestOrigin::kCrossB,
                     HtmlPage(
                         "Child B", Paragraph("Child frame B text"),
                         Iframe(TestOrigin::kCrossB,
                                HtmlPage("Child B1", Paragraph("Child frame B1 "
                                                               "text"))),
                         Iframe(TestOrigin::kCrossC,
                                HtmlPage("Child D", Paragraph("Child frame D "
                                                              "text")),
                                "iframe_d")),
                     "iframe_b")),
          "iframe_a"),
      Iframe(TestOrigin::kCrossB,
             HtmlPage("Child C", Paragraph("Child frame C text")), "iframe_c"));

  std::string main_html = page_helper_->Build(page_structure);
  GURL main_url = test_server_.GetURL(kMainPagePath);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html), main_url,
                      web_state());

  // Wait for all 7 frames to load.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^{
    return web_state()
               ->GetWebFramesManager(web::ContentWorld::kIsolatedWorld)
               ->GetAllWebFrames()
               .size() == 7;
  }));

  GURL iframe_b_url = page_helper_->GetUrlForId("iframe_b");
  GURL iframe_a_url = page_helper_->GetUrlForId("iframe_a");
  GURL iframe_c_url = page_helper_->GetUrlForId("iframe_c");
  GURL iframe_d_url = page_helper_->GetUrlForId("iframe_d");

  // Sabotage remote frame discovery in frame B (it won't see frame B2 hence not
  // claim its content which will remain orphan).
  web::WebFrame* iframe_b = nullptr;
  base::RunLoop find_frame_run_loop;
  std::set<web::WebFrame*> all_frames =
      web_state()
          ->GetWebFramesManager(web::ContentWorld::kIsolatedWorld)
          ->GetAllWebFrames();
  size_t pending_callbacks = all_frames.size();
  for (web::WebFrame* frame : all_frames) {
    frame->ExecuteJavaScript(
        u"window.location.href",
        base::BindOnce(
            [](web::WebFrame* frame, GURL expected_url,
               web::WebFrame** out_frame, size_t* pending_callbacks,
               base::RepeatingClosure quit_closure, const base::Value* result) {
              if (result && result->is_string()) {
                if (GURL(result->GetString()) == expected_url) {
                  *out_frame = frame;
                  quit_closure.Run();
                  return;
                }
              }
              (*pending_callbacks)--;
              if (*pending_callbacks == 0) {
                quit_closure.Run();
              }
            },
            frame, iframe_b_url, &iframe_b, &pending_callbacks,
            find_frame_run_loop.QuitClosure()));
  }
  find_frame_run_loop.Run();

  ASSERT_TRUE(iframe_b);

  std::string script = base::StringPrintf(R"(
    (() => {
    const originalGetElementsByTagName = Element.prototype.getElementsByTagName;
    Element.prototype.getElementsByTagName = function(tagName) {
      const elements = originalGetElementsByTagName.call(this, tagName);
      if (tagName.toLowerCase() !== 'iframe') {
        return elements;
      }
      return Array.from(elements).filter(el => el.src !== '%s');
    };
    return true;
    })(); true;)",
                                          iframe_d_url.spec().c_str());

  iframe_b->ExecuteJavaScript(base::UTF8ToUTF16(script));

  PageContextWrapperConfig config = PageContextWrapperConfigBuilder()
                                        .SetGraftCrossOriginFrameContent(true)
                                        .Build();
  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();
  ASSERT_EQ(root_node.children_nodes_size(), 4);

  const optimization_guide::proto::ContentNode* iframe_a_node = nullptr;
  const optimization_guide::proto::ContentNode* iframe_c_node = nullptr;

  for (const auto& node : root_node.children_nodes()) {
    if (node.content_attributes().attribute_type() ==
        optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
      if (node.content_attributes().iframe_data().frame_data().url() ==
          iframe_a_url.spec()) {
        iframe_a_node = &node;
      } else if (node.content_attributes().iframe_data().frame_data().url() ==
                 iframe_c_url.spec()) {
        iframe_c_node = &node;
      }
    }
  }

  ASSERT_TRUE(iframe_a_node);
  ASSERT_TRUE(iframe_c_node);

  // Verify Iframe A's subtree.
  ASSERT_EQ(iframe_a_node->children_nodes_size(), 1);
  const auto& iframe_a_root = iframe_a_node->children_nodes(0);
  ASSERT_EQ(iframe_a_root.children_nodes_size(), 3);  // text, A1, B

  const auto& iframe_a1_node = iframe_a_root.children_nodes(1);
  EXPECT_EQ(
      iframe_a1_node.content_attributes().iframe_data().frame_data().title(),
      "Child A1");

  const auto& iframe_b_node = iframe_a_root.children_nodes(2);
  EXPECT_EQ(
      iframe_b_node.content_attributes().iframe_data().frame_data().title(),
      "Child B");

  // Verify Iframe B's subtree (nested in A).
  ASSERT_EQ(iframe_b_node.children_nodes_size(), 1);
  const auto& iframe_b_root = iframe_b_node.children_nodes(0);
  ASSERT_EQ(iframe_b_root.children_nodes_size(), 2);  // text, B1

  const auto& iframe_b1_node = iframe_b_root.children_nodes(1);
  EXPECT_EQ(
      iframe_b1_node.content_attributes().iframe_data().frame_data().title(),
      "Child B1");

  // Verify Iframe C's content.
  ASSERT_EQ(iframe_c_node->children_nodes_size(), 1);
  const auto& iframe_c_root = iframe_c_node->children_nodes(0);
  ASSERT_EQ(iframe_c_root.children_nodes_size(), 1);  // text
  EXPECT_EQ(iframe_c_root.children_nodes(0)
                .content_attributes()
                .text_data()
                .text_content(),
            "Child frame C text");
}

// Tests extraction when the registration of a cross-origin frame fails.
TEST_P(PageContextWrapperTest, PopulatePageContextWithRegistrationFailure) {
  if (!IsRefactored()) {
    GTEST_SKIP()
        << "Frame grafter not supported for the non-refactored APC wrapper";
  }

  auto page_structure =
      HtmlPage("Main", Paragraph("Main frame text"),
               Iframe(TestOrigin::kCrossA,
                      HtmlPage("Child Cross Origin",
                               Paragraph("Child frame cross-origin text")),
                      "iframe_cross"));

  std::string main_html = page_helper_->Build(page_structure);
  GURL main_url = test_server_.GetURL(kMainPagePath);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html), main_url,
                      web_state());

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^{
    return web_state()
               ->GetWebFramesManager(web::ContentWorld::kIsolatedWorld)
               ->GetAllWebFrames()
               .size() == 2;
  }));

  // Sabotage registration in the cross-origin frame by blocking the
  // registration message in the child frame.
  web::WebFrame* child_frame = nullptr;
  for (web::WebFrame* frame :
       web_state()
           ->GetWebFramesManager(web::ContentWorld::kIsolatedWorld)
           ->GetAllWebFrames()) {
    if (!frame->IsMainFrame()) {
      child_frame = frame;
      break;
    }
  }
  ASSERT_TRUE(child_frame);
  child_frame->ExecuteJavaScript(uR"(
    window.addEventListener('message', function(event) {
        if (event.data && event.data.command === 'registerAsChildFrame') {
            event.stopImmediatePropagation();
            event.preventDefault();
        }
    }, true);
    true;
  )");

  PageContextWrapperConfig config = PageContextWrapperConfigBuilder()
                                        .SetGraftCrossOriginFrameContent(true)
                                        .Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  // This should finish only after the registration timeout expires.
  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();

  // We expect:
  // 1. Main Text.
  // 2. Empty placeholder node (from main frame's iframe tag).
  // 3. Child frame content node (orphan).
  ASSERT_EQ(root_node.children_nodes_size(), 3);

  // Verify that the content of the unregistered frame was still added despite
  // the timeout in the default location (as a children of the root node).
  bool found_unregistered_frame_text = false;
  for (const auto& node : root_node.children_nodes()) {
    if (node.content_attributes().has_iframe_data()) {
      // Check if it's the full child frame (orphan).
      if (node.children_nodes_size() > 0 &&
          node.children_nodes(0).children_nodes_size() > 0 &&
          node.children_nodes(0)
                  .children_nodes(0)
                  .content_attributes()
                  .text_data()
                  .text_content() == "Child frame cross-origin text") {
        found_unregistered_frame_text = true;
      }
    }
  }
  EXPECT_TRUE(found_unregistered_frame_text);
}

// Tests extraction on a complex page with Rich Extraction.
// This covers the nodes and nesting structures that are currently supported.
//
// Layout:
//      +----------------------------------+
//      | Main page (test_server_)         |  - Main frame (Origin M)
//      |                                  |
//      |   +--------------------------+   |
//      |   | Div (Scrollable)         |   |
//      |   |   - P ("Bold Text")      |   |
//      |   |   - Img                  |   |
//      |   |   - Anchor ("Link")      |   |
//      |   +--------------------------+   |
//      |                                  |
//      |   +--------------------------+   |
//      |   | Iframe (Cross-Origin)    |   |  - Grafted frame
//      |   |   - P ("Child ...")      |   |
//      |   +--------------------------+   |
//      |                                  |
//      |   +--------------------------+   |
//      |   | Iframe (Same-Origin)     |   |  - Same-origin frame
//      |   |   - P ("Child frame 3")  |   |
//      |   +--------------------------+   |
//      +----------------------------------+
TEST_P(PageContextWrapperTest, PopulatePageContext_RichExtraction) {
  if (!IsRefactored()) {
    GTEST_SKIP()
        << "Rich Extraction not supported for the non-refactored APC wrapper";
  }

  auto page_structure = HtmlPage(
      "Test Title",
      RawHtml(
          "<div style='width: 100px; height: 100px; overflow: scroll;' "
          "id='scrollable'>"
          "    <p style='font-weight: bold; font-size: 20px; margin: 0;'>Bold "
          "Text</p>"
          "    <img src='test.png' alt='Test Image' style='width: 50px; "
          "height: "
          "50px; display: block;'>"
          "    <a href='https://example.com' style='display: block;' "
          "       rel=\"noopener noreferrer\">Link</a>"
          "    <div style='width: 200px; height: 200px;'></div>"
          "</div>"),
      Iframe(TestOrigin::kCrossA,
             HtmlPage("Child Cross Origin",
                      Paragraph("Child frame cross-origin text")),
             "iframe_cross"),
      Iframe(TestOrigin::kMain,
             HtmlPage("Child 3", Paragraph("Child frame 3 text")),
             "iframe_same"));

  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  // Wait for all 3 frames to be available (Main frame + 2 iframes).
  web::WebFramesManager* frames_manager = web_state()->GetWebFramesManager(
      extractor_feature()->GetSupportedContentWorld());
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^{
    return frames_manager->GetAllWebFrames().size() == 3;
  }));

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  const auto& actual_apc = page_context->annotated_page_content();

  EXPECT_EQ(actual_apc.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);

  // Main frame data
  const auto& main_frame = actual_apc.main_frame_data();
  const auto& origin = main_frame.security_origin();
  EXPECT_FALSE(origin.opaque());
  EXPECT_EQ(origin.value(), test_server_.GetOrigin().Serialize());
  EXPECT_EQ(main_frame.title(), "Test Title");
  EXPECT_EQ(main_frame.url(), test_server_.GetURL(kMainPagePath).spec());

  const auto& root = actual_apc.root_node();
  EXPECT_EQ(root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  ASSERT_EQ(root.children_nodes_size(), 3);

  // Verify root node content.

  // ---------------------------------------------------------
  // Section 1: Div (Scrollable)
  // ---------------------------------------------------------
  //   |   +--------------------------+
  //   |   | Div (Scrollable)         |
  //   |   |   - P ("Bold Text")      |
  //   |   |   - Img                  |
  //   |   |   - Anchor ("Link")      |
  //   |   +--------------------------+
  const auto& div = root.children_nodes(0);
  EXPECT_EQ(div.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER);

  ASSERT_EQ(div.children_nodes_size(), 3);

  // 1.1 Paragraph
  {
    const auto& p = div.children_nodes(0);
    EXPECT_EQ(p.content_attributes().attribute_type(),
              optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);

    ASSERT_EQ(p.children_nodes_size(), 1);
    const auto& p_text = p.children_nodes(0);
    EXPECT_EQ(p_text.content_attributes().attribute_type(),
              optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
    EXPECT_EQ(p_text.content_attributes().text_data().text_content(),
              "Bold Text");
    EXPECT_TRUE(
        p_text.content_attributes().text_data().text_style().has_emphasis());
    EXPECT_EQ(p_text.content_attributes().text_data().text_style().text_size(),
              optimization_guide::proto::TEXT_SIZE_L);
  }

  // 1.2 Image
  const auto& img = div.children_nodes(1);
  EXPECT_EQ(img.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE);
  EXPECT_EQ(img.content_attributes().image_data().image_caption(),
            "Test Image");

  // 1.3 Anchor
  const auto& a = div.children_nodes(2);
  EXPECT_EQ(a.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR);
  EXPECT_EQ(a.content_attributes().anchor_data().url(), "https://example.com/");
  const auto& anchor_rel = a.content_attributes().anchor_data().rel();
  ASSERT_EQ(anchor_rel.size(), 2);
  bool has_no_opener = false;
  bool has_no_referrer = false;
  for (const auto& rel : anchor_rel) {
    if (rel == optimization_guide::proto::ANCHOR_REL_NO_OPENER) {
      has_no_opener = true;
    }
    if (rel == optimization_guide::proto::ANCHOR_REL_NO_REFERRER) {
      has_no_referrer = true;
    }
  }
  EXPECT_TRUE(has_no_opener);
  EXPECT_TRUE(has_no_referrer);

  ASSERT_EQ(a.children_nodes_size(), 1);
  const auto& a_text = a.children_nodes(0);
  EXPECT_EQ(a_text.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(a_text.content_attributes().text_data().text_content(), "Link");
  EXPECT_FALSE(
      a_text.content_attributes().text_data().text_style().has_emphasis());
  EXPECT_EQ(a_text.content_attributes().text_data().text_style().text_size(),
            optimization_guide::proto::TEXT_SIZE_M_DEFAULT);

  // ---------------------------------------------------------
  // Section 2: Cross-Origin Iframe (Grafted)
  // ---------------------------------------------------------
  //   |   +--------------------------+
  //   |   | Iframe (Cross-Origin)    |
  //   |   |   - P ("Child ...")      |
  //   |   +--------------------------+
  const auto& iframe = root.children_nodes(1);
  EXPECT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  EXPECT_EQ(iframe.content_attributes().iframe_data().frame_data().url(),
            page_helper_->GetUrlForId("iframe_cross").spec());
  EXPECT_EQ(iframe.content_attributes().iframe_data().frame_data().title(),
            "Child Cross Origin");

  ASSERT_EQ(iframe.children_nodes_size(), 1);

  // Grafted root
  const auto& iframe_root = iframe.children_nodes(0);
  EXPECT_EQ(iframe_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  ASSERT_EQ(iframe_root.children_nodes_size(), 1);

  // 2.1 Paragraph
  {
    const auto& p = iframe_root.children_nodes(0);
    EXPECT_EQ(p.content_attributes().attribute_type(),
              optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);

    ASSERT_EQ(p.children_nodes_size(), 1);
    const auto& text = p.children_nodes(0);
    EXPECT_EQ(text.content_attributes().attribute_type(),
              optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
    EXPECT_EQ(text.content_attributes().text_data().text_content(),
              "Child frame cross-origin text");
    EXPECT_FALSE(
        text.content_attributes().text_data().text_style().has_emphasis());
  }

  // ---------------------------------------------------------
  // Section 3: Same-Origin Iframe
  // ---------------------------------------------------------
  //   |   +--------------------------+
  //   |   | Iframe (Same-Origin)     |
  //   |   |   - P ("Child frame 3")  |
  //   |   +--------------------------+
  const auto& same_origin_iframe = root.children_nodes(2);
  EXPECT_EQ(same_origin_iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  EXPECT_EQ(same_origin_iframe.content_attributes()
                .iframe_data()
                .frame_data()
                .title(),
            "Child 3");
  EXPECT_EQ(
      same_origin_iframe.content_attributes().iframe_data().frame_data().url(),
      page_helper_->GetUrlForId("iframe_same").spec());

  const auto& same_origin_iframe_origin =
      same_origin_iframe.content_attributes()
          .iframe_data()
          .frame_data()
          .security_origin();
  EXPECT_FALSE(same_origin_iframe_origin.opaque());
  EXPECT_EQ(same_origin_iframe_origin.value(),
            test_server_.GetOrigin().Serialize());

  // Verify iframe root.
  ASSERT_EQ(same_origin_iframe.children_nodes_size(), 1);
  const auto& same_origin_iframe_root = same_origin_iframe.children_nodes(0);
  EXPECT_EQ(same_origin_iframe_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
  ASSERT_EQ(same_origin_iframe_root.children_nodes_size(), 1);

  // 3.1 Paragraph

  const auto& same_origin_iframe_p = same_origin_iframe_root.children_nodes(0);
  EXPECT_EQ(same_origin_iframe_p.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);

  ASSERT_EQ(same_origin_iframe_p.children_nodes_size(), 1);
  const auto& same_origin_iframe_text = same_origin_iframe_p.children_nodes(0);
  EXPECT_EQ(same_origin_iframe_text.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(
      same_origin_iframe_text.content_attributes().text_data().text_content(),
      "Child frame 3 text");
}

// Tests that all the nested iframes on different origins are put under their
// parent respecting the frame tree when Rich Extraction is enabled and frame
// grafting is enabled.
//
// Layout:
//      +----------------------------------+
//      | Main page (test_server_)         |  - Main frame (Origin M)
//      |   - P ("Main frame text")        |
//      |                                  |
//      |   +--------------------------+   |
//      |   | Iframe (Cross-Origin)    |   |  - Middle frame (Origin A)
//      |   |   - P ("Middle ...")     |   |
//      |   |                          |   |
//      |   |   +------------------+   |   |
//      |   |   | Iframe           |   |   |  - Inner frame (Origin M)
//      |   |   | (Same-Origin)    |   |   |    (Same as Main)
//      |   |   | - P ("Child 3")  |   |   |
//      |   |   +------------------+   |   |
//      |   +--------------------------+   |
//      +----------------------------------+
TEST_P(PageContextWrapperTest,
       PopulatePageContext_RichExtraction_NestedSameCrossOriginFrame) {
  if (!IsRefactored()) {
    return;
  }

  auto page_structure = HtmlPage(
      "Main Frame", Paragraph("Main frame text"),
      Iframe(
          TestOrigin::kCrossA,
          HtmlPage("Middle Frame", Paragraph("Middle frame text"),
                   Iframe(TestOrigin::kMain,
                          HtmlPage("Child 3", Paragraph("Child frame 3 text")),
                          "iframe_inner")),
          "iframe_middle"));

  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  auto* frames_manager = web_state()->GetWebFramesManager(
      extractor_feature()->GetSupportedContentWorld());
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^{
    return frames_manager->GetAllWebFrames().size() == 3;
  }));

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root = annotated_page_content.root_node();
  ASSERT_EQ(root.children_nodes_size(), 2);

  // ---------------------------------------------------------
  // Section 1: Paragraph (Main Frame)
  // ---------------------------------------------------------
  //   |   - P ("Main frame text")        |
  {
    const auto& p = root.children_nodes(0);
    EXPECT_EQ(p.content_attributes().attribute_type(),
              optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
    ASSERT_EQ(p.children_nodes_size(), 1);
    const auto& p_text = p.children_nodes(0);
    EXPECT_EQ(p_text.content_attributes().text_data().text_content(),
              "Main frame text");
  }

  // ---------------------------------------------------------
  // Section 2: Middle Frame (Grafted)
  // ---------------------------------------------------------
  //   |   +--------------------------+   |
  //   |   | Iframe (Cross-Origin)    |   |  - Middle frame (Origin A)
  //   |   |   - P ("Middle ...")     |   |
  //   |   |                          |   |
  //   |   |   +------------------+   |   |
  //   |   |   | Iframe           |   |   |  - Inner frame (Origin M)
  //   |   |   | (Same-Origin)    |   |   |    (Same as Main)
  //   |   |   | - P ("Child 3")  |   |   |
  //   |   |   +------------------+   |   |
  //   |   +--------------------------+   |
  const auto& middle_frame_node = root.children_nodes(1);
  EXPECT_EQ(middle_frame_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  EXPECT_EQ(
      middle_frame_node.content_attributes().iframe_data().frame_data().title(),
      "Middle Frame");

  // Verify Middle Frame Layout:
  // - Root (Grafted Root)
  //   - Paragraph ("Middle frame text")
  //   - Inner Frame (Grafted)

  ASSERT_GE(middle_frame_node.children_nodes_size(), 1);
  const auto& middle_frame_root = middle_frame_node.children_nodes(0);
  EXPECT_EQ(middle_frame_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  ASSERT_GE(middle_frame_root.children_nodes_size(), 2);

  // 2.1 Paragraph (Middle Frame)
  {
    const auto& middle_p = middle_frame_root.children_nodes(0);
    EXPECT_EQ(middle_p.content_attributes().attribute_type(),
              optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);

    ASSERT_GE(middle_p.children_nodes_size(), 1);
    EXPECT_EQ(middle_p.children_nodes(0)
                  .content_attributes()
                  .text_data()
                  .text_content(),
              "Middle frame text");
  }

  // ---------------------------------------------------------
  // Section 3: Inner Frame (Grafted inside Middle)
  // ---------------------------------------------------------
  const auto& inner_frame_node = middle_frame_root.children_nodes(1);
  EXPECT_EQ(inner_frame_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  EXPECT_EQ(
      inner_frame_node.content_attributes().iframe_data().frame_data().title(),
      "Child 3");

  // Verify Inner Frame.

  ASSERT_GE(inner_frame_node.children_nodes_size(), 1);
  const auto& inner_frame_root = inner_frame_node.children_nodes(0);
  EXPECT_EQ(inner_frame_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  ASSERT_GE(inner_frame_root.children_nodes_size(), 1);
  const auto& inner_p = inner_frame_root.children_nodes(0);
  EXPECT_EQ(inner_p.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  ASSERT_GE(inner_p.children_nodes_size(), 1);
  EXPECT_EQ(
      inner_p.children_nodes(0).content_attributes().text_data().text_content(),
      "Child frame 3 text");
}

// Tests that the ancestor mapping is correct.
TEST_P(PageContextWrapperTest,
       PopulatePageContext_RichExtraction_AncestorMapping) {
  if (!IsRefactored()) {
    return;
  }

  auto page_structure =
      HtmlPage("Pruning Test",
               RawHtml("<ul><li>Child 1</li><li>Child 2</li></ul>"
                       "<figure><span><p>Deep Child</p></span></figure>"));

  std::string html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  auto page_context = std::move(response.value());
  ASSERT_TRUE(page_context);

  const auto& root_node = page_context->annotated_page_content().root_node();
  ASSERT_EQ(root_node.children_nodes_size(), 2);

  const auto& list_node = root_node.children_nodes(0);
  EXPECT_EQ(list_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_UNORDERED_LIST);

  // If pruning was broken (list removed from map), items would attach to root,
  // making list empty.
  ASSERT_EQ(list_node.children_nodes_size(), 2);

  EXPECT_EQ(list_node.children_nodes(0).content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_LIST_ITEM);
  EXPECT_EQ(list_node.children_nodes(1).content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_LIST_ITEM);

  // Check that the ignored 'span' was skipped and 'p' attached to 'figure'.
  const auto& figure_node = root_node.children_nodes(1);
  EXPECT_EQ(figure_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER);
  ASSERT_EQ(figure_node.children_nodes_size(), 1);

  const auto& p_node = figure_node.children_nodes(0);
  EXPECT_EQ(p_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  ASSERT_EQ(1, p_node.children_nodes_size());
  ASSERT_EQ(
      p_node.children_nodes(0).content_attributes().text_data().text_content(),
      "Deep Child");
}

// Tests that the extraction logic correctly handles visibility:
// - Invisible containers with visible content are KEPT.
// - Visible containers with invisible content are KEPT.
// - Invisible containers with no visible content are PRUNED.
// - Invisible leaf nodes (Img, Input) are SKIPPED.
TEST_P(PageContextWrapperTest, PopulatePageContext_RichExtraction_Visibility) {
  if (!IsRefactored()) {
    GTEST_SKIP() << "Visibility logic is improved in the refactored version";
  }

  auto page_structure = HtmlPage(
      "RichExtraction_Visibility",
      // 1. Invisible Container with Visible Content -> KEPT
      RawHtml("<div style='visibility:hidden'><p "
              "style='visibility:visible'>Visible Paragraph in Hidden "
              "Div</p></div>"),
      // 2. Invisible Container with Hidden Content -> PRUNED
      RawHtml("<div style='visibility:hidden'><p style='display:none'>Hidden "
              "Paragraph</p></div>"),
      // 3. Invisible Leaf -> SKIPPED
      RawHtml("<img src='hidden.png' style='visibility:hidden'>"),
      // 4. Visible Leaf -> KEPT
      RawHtml("<img src='visible.png'>"),
      // 5. Deeply Nested Invisible Containers with Visible Content -> KEPT
      RawHtml("<div style='visibility:hidden'><section "
              "style='visibility:hidden'><p style='visibility:visible'>Deep "
              "Visible Paragraph</p></section></div>"),
      // 6. Invisible Input (Leaf) -> SKIPPED
      RawHtml(
          "<input type='text' value='hidden input' style='visibility:hidden'>"),
      // 7. Visible Container with Hidden Content -> KEPT
      RawHtml("<div style='width: 100px; height: 100px; overflow: scroll; "
              "visibility:visible'><p style='visibility:hidden'>Hidden "
              "Paragraph</p></div>"));

  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  const auto& page_context = *response.value();
  const auto& root_node = page_context.annotated_page_content().root_node();

  // Expected Children of Root:
  // 1. P (Visible Paragraph in Hidden Div)
  // 2. Img (visible.png)
  // 3. P (Deep Visible Paragraph)
  // 4. Div (Visible generic container with hidden content)
  // Hidden items (Img, Input, Hidden P) are skipped/pruned.
  ASSERT_EQ(root_node.children_nodes_size(), 4);

  // Verify Child 1: P
  const auto& child1 = root_node.children_nodes(0);
  EXPECT_EQ(child1.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  ASSERT_EQ(child1.children_nodes_size(), 1);
  EXPECT_EQ(
      child1.children_nodes(0).content_attributes().text_data().text_content(),
      "Visible Paragraph in Hidden Div");

  // Verify Child 2: Img
  const auto& child2 = root_node.children_nodes(1);
  EXPECT_EQ(child2.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE);

  // Verify Child 3: P
  const auto& child3 = root_node.children_nodes(2);
  EXPECT_EQ(child3.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  ASSERT_EQ(child3.children_nodes_size(), 1);
  EXPECT_EQ(
      child3.children_nodes(0).content_attributes().text_data().text_content(),
      "Deep Visible Paragraph");

  // Verify Child 4: DIV
  const auto& child4 = root_node.children_nodes(3);
  EXPECT_EQ(child4.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER);
  ASSERT_EQ(child4.children_nodes_size(), 0);
}

// Tests that styles in a same-origin iframe are computed correctly.
TEST_P(PageContextWrapperTest,
       PopulatePageContext_RichExtraction_Visibility_OnSameOriginIframe) {
  if (!IsRefactored()) {
    GTEST_SKIP() << "Refactored version only";
  }

  // Use srcdoc to define same-origin content inline.
  // 1. Hidden Div (display: none) -> Should be strictly skipped/rejected.
  // 2. Visible Div (visibility: visible) -> Content should be extracted.
  std::string srcdoc_content =
      "<div style='display: none'><p>Hidden Content</p></div>"
      "<div style='visibility: visible'><p>Visible Content</p></div>";

  auto page_structure =
      HtmlPage("SameOriginIframe_StyleIsolation",
               RawHtml("<iframe srcdoc=\"" + srcdoc_content + "\"></iframe>"));

  std::string main_html = page_helper_->Build(page_structure);
  // Start the server if not already running (required for GetURL).
  if (!test_server_.Started()) {
    ASSERT_TRUE(test_server_.Start());
  }
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  const auto& root_node =
      response.value()->annotated_page_content().root_node();

  // Validate structure: Root -> Iframe.
  ASSERT_GT(root_node.children_nodes_size(), 0);
  const auto& iframe_node = root_node.children_nodes(0);
  EXPECT_EQ(iframe_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  // Validate the inner structure of the iframe based on layout changes.
  // The invisible div enclosing 'Hidden Content' gets rejected, leaving only
  // the 'Visible Content'.
  ASSERT_EQ(iframe_node.children_nodes_size(), 1);
  const auto& iframe_body_node = iframe_node.children_nodes(0);
  EXPECT_EQ(iframe_body_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  ASSERT_EQ(iframe_body_node.children_nodes_size(), 1);
  const auto& p_node = iframe_body_node.children_nodes(0);
  EXPECT_EQ(p_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);

  ASSERT_EQ(p_node.children_nodes_size(), 1);
  const auto& text_node = p_node.children_nodes(0);
  EXPECT_EQ(text_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(text_node.content_attributes().text_data().text_content(),
            "Visible Content");
}

// Tests that the extraction logic respects the maximum recursion depth limit.
// Nested frames beyond the limit should not be extracted.
TEST_P(PageContextWrapperTest,
       PopulatePageContext_RichExtraction_DepthLimit_Saturation) {
  if (!IsRefactored()) {
    GTEST_SKIP() << "Depth tracking is improved in the refactored version";
  }

  // Create a deeply nested structure of DIVs to test the depth limit.
  // We want to exceed the limit slightly.
  //
  // MAX_APC_RESPONSE_DEPTH = 200.
  // MAX_APC_NODE_DEPTH = 10
  // Max Apc Response Depth = MAX_APC_RESPONSE_DEPTH - MAX_APC_NODE_DEPTH =
  // 190 Min Depth Cost per node = 2
  //
  // Max apc node depth = 190/2 = 95
  std::string deep_html;
  // Pick a number of divs that will exceed the depth limit (100 > 95) where
  // truncation will occur.
  const int kDivCount = 100;
  for (int i = 1; i <= kDivCount; ++i) {
    deep_html +=
        base::StringPrintf("<div style=\"overflow:scroll\">Depth %d", i);
  }
  for (int i = 1; i <= kDivCount; ++i) {
    deep_html += "</div>";
  }

  auto page_structure = HtmlPage("Deep Page", RawHtml(deep_html));
  std::string html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  optimization_guide::proto::PageContext page_context =
      std::move(*response.value());
  const auto& root_node = page_context.annotated_page_content().root_node();

  // Verify that the first node starting from the root is a div node.
  ASSERT_EQ(1, root_node.children_nodes_size());
  ASSERT_EQ(root_node.children_nodes(0).content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER);

  // Start walking from the first div node as the root so we start the depth
  // counter at 1.
  int actual_depth = 1;
  const optimization_guide::proto::ContentNode* lastDivNode =
      &root_node.children_nodes(0);
  const optimization_guide::proto::ContentNode* parentOfLastDivNode =
      lastDivNode;

  // Get the last container node (div) in the apc tree and update parent.
  while (lastDivNode->children_nodes_size() > 1) {
    parentOfLastDivNode = lastDivNode;
    lastDivNode = &lastDivNode->children_nodes(1);
    actual_depth++;
  }

  // Verify that the tree was cut off at depth 95 (190 / 2).
  EXPECT_EQ(95, actual_depth);
  EXPECT_EQ(optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER,
            lastDivNode->content_attributes().attribute_type());

  // The parent of the last div node should be at depth 94 (one level above).
  // We can't verify the depth id of the last div node because it was
  // truncated.
  EXPECT_EQ("Depth 94", parentOfLastDivNode->children_nodes(0)
                            .content_attributes()
                            .text_data()
                            .text_content());
}

// Tests that no truncation takes place below the depth limit.
TEST_P(PageContextWrapperTest,
       PopulatePageContext_RichExtraction_DepthLimit_BelowSaturation) {
  if (!IsRefactored()) {
    GTEST_SKIP() << "Depth tracking is improved in the refactored version";
  }

  // Create a deeply nested structure of DIVs to test the depth limit.
  // We want to be slighly below the limit so no truncation occurs.
  //
  // MAX_APC_RESPONSE_DEPTH = 200.
  // MAX_APC_NODE_DEPTH = 10
  // Max Apc Response Depth = MAX_APC_RESPONSE_DEPTH - MAX_APC_NODE_DEPTH =
  // 190 Min Depth Cost per APC node object = 2
  //
  // Max apc node depth = 190/2 = 95
  std::string deep_html;
  const int kDivCount = 90;
  for (int i = 1; i <= kDivCount; ++i) {
    deep_html +=
        base::StringPrintf("<div style=\"overflow:scroll\">Depth %d", i);
  }
  deep_html += "</div>";
  for (int i = 1; i <= kDivCount; ++i) {
    deep_html += "</div>";
  }

  auto page_structure = HtmlPage("Deep Page Safe", RawHtml(deep_html));
  std::string html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  optimization_guide::proto::PageContext page_context =
      std::move(*response.value());
  const auto& root_node = page_context.annotated_page_content().root_node();

  // Verify that the first node starting from the root is a div node.
  ASSERT_EQ(1, root_node.children_nodes_size());
  ASSERT_EQ(root_node.children_nodes(0).content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER);

  // Start walking from the first div node as the root so we start the depth
  // counter at 1.
  int actual_depth = 1;
  const optimization_guide::proto::ContentNode* lastDivNode =
      &root_node.children_nodes(0);
  const optimization_guide::proto::ContentNode* parentOfLastDivNode =
      lastDivNode;

  // Get the last container node (div) in the apc tree and update parent.
  while (lastDivNode->children_nodes_size() > 1) {
    parentOfLastDivNode = lastDivNode;
    lastDivNode = &lastDivNode->children_nodes(1);
    actual_depth++;
  }

  // Verify that the tree is complete reaching a depth of 90.
  EXPECT_EQ(90, actual_depth);
  EXPECT_EQ(optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER,
            lastDivNode->content_attributes().attribute_type());

  // Sanity Check: The parent of the last div node should be at depth 89 (one
  // level above).
  EXPECT_EQ("Depth 89", parentOfLastDivNode->children_nodes(0)
                            .content_attributes()
                            .text_data()
                            .text_content());

  // The last node should be complete and at depth 90, not truncated, hence
  // contain a text node.
  ASSERT_EQ(1, lastDivNode->children_nodes_size());
  EXPECT_EQ("Depth 90", lastDivNode->children_nodes(0)
                            .content_attributes()
                            .text_data()
                            .text_content());
}

// Tests that text transforms and masking are applied correctly.
TEST_P(PageContextWrapperTest,
       PopulatePageContext_RichExtraction_Text_PasswordMaskingAndTransforms) {
  if (!IsRefactored()) {
    return;
  }

  auto page_structure = HtmlPage(
      "RichExtraction_Text_PasswordMaskingAndTransforms",
      RawHtml("<div style='text-transform: uppercase'>uppercase</div>"),
      RawHtml("<div style='-webkit-text-security: disc'>password</div>"),
      RawHtml("<div style='text-transform: uppercase; -webkit-text-security: "
              "square'>both</div>"));

  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_annotated_page_content());

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();

  ASSERT_EQ(3, root_node.children_nodes_size());

  // 1. Uppercase Div
  const auto& uppercase_div = root_node.children_nodes(0);
  EXPECT_EQ("UPPERCASE",
            uppercase_div.content_attributes().text_data().text_content());

  // 2. Password Disc Div
  const auto& password_div = root_node.children_nodes(1);
  std::string bullet = "\u2022";
  std::string expected_password_mask = "";
  for (size_t i = 0; i < MASKED_TEXT_LENGTH; ++i) {
    expected_password_mask += bullet;
  }
  EXPECT_EQ(expected_password_mask,
            password_div.content_attributes().text_data().text_content());

  // 3. Both (Uppercase + Square) Div
  const auto& both_div = root_node.children_nodes(2);
  std::string square = "\u25A0";
  std::string expected_both_mask = "";
  for (size_t i = 0; i < MASKED_TEXT_LENGTH; ++i) {
    expected_both_mask += square;
  }
  EXPECT_EQ(expected_both_mask,
            both_div.content_attributes().text_data().text_content());
}

// Tests text extraction with collapsible whitespaces where whitespaces are
// collapsed in normal whitespace but preserved in pre and pre-line.
TEST_P(PageContextWrapperTest,
       PopulatePageContext_RichExtraction_Text_CollapsibleWhitespaces) {
  if (!IsRefactored()) {
    return;
  }

  auto page_structure =
      HtmlPage("RichExtraction_Text_CollapsibleWhitespaces",
               // Should be pruned (normal whitespace collapsing)
               RawHtml("<div style='white-space: normal'>   </div>"),
               // Should be preserved
               RawHtml("<div style='white-space: pre'>   </div>"),
               // Should be preserved
               RawHtml("<div style='white-space: pre-wrap'> </div>"));

  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_annotated_page_content());

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();

  // The first div is pruned, so we expect only 2 text nodes from the other
  // divs. The Diks themselves are generic containers and are skipped,
  // promoting text to root.
  ASSERT_EQ(2, root_node.children_nodes_size());

  // 1. Pre (3 spaces)
  const auto& pre_node = root_node.children_nodes(0);
  EXPECT_EQ("   ", pre_node.content_attributes().text_data().text_content());

  // 2. Pre-wrap (1 space)
  const auto& pre_wrap_node = root_node.children_nodes(1);
  EXPECT_EQ(" ", pre_wrap_node.content_attributes().text_data().text_content());
}

// Tests text extraction with collapsible newlines where newlines are collapsed
// with normal whitespace styles but preserved in pre and pre-line.
TEST_P(PageContextWrapperTest,
       PopulatePageContext_RichExtraction_Text_CollapsibleNewlines) {
  if (!IsRefactored()) {
    return;
  }

  auto page_structure =
      HtmlPage("RichExtraction_Text_CollapsibleNewlines",
               // Should be pruned (normal whitespace collapsing)
               RawHtml("<div style='white-space: normal'>\n\n</div>"),
               // Should be preserved
               RawHtml("<div style='white-space: pre'>\n</div>"),
               // Should be preserved (pre-line preserves newlines)
               RawHtml("<div style='white-space: pre-line'>\n</div>"));

  std::string main_html = page_helper_->Build(page_structure);
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();

  PageContextWrapperCallbackResponse response = RunPageContextWrapperWithConfig(
      web_state(), config, ^(PageContextWrapper* wrapper) {
        wrapper.shouldGetAnnotatedPageContent = YES;
      });

  ASSERT_TRUE(response.has_value());
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::move(response.value());

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_annotated_page_content());

  const auto& annotated_page_content = page_context->annotated_page_content();
  const auto& root_node = annotated_page_content.root_node();

  // The first div is pruned.
  ASSERT_EQ(2, root_node.children_nodes_size());

  // 1. Pre (\n)
  const auto& pre_node = root_node.children_nodes(0);
  EXPECT_EQ("\n", pre_node.content_attributes().text_data().text_content());

  // 2. Pre-line (\n)
  const auto& pre_line_node = root_node.children_nodes(1);
  EXPECT_EQ("\n",
            pre_line_node.content_attributes().text_data().text_content());
}

INSTANTIATE_TEST_SUITE_P(,
                         PageContextWrapperTest,
                         testing::Bool(),
                         PrintToStringParamName());
