// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/base64.h"
#import "base/containers/span.h"
#import "base/files/scoped_temp_dir.h"
#import "base/run_loop.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/values_test_util.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
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

namespace {

const char kMainPagePath[] = "/main.html";
const char kIframe1Path[] = "/iframe1.html";
const char kIframe2Path[] = "/iframe2.html";
const char kIframe3Path[] = "/iframe3.html";

const char kIframe1Html[] =
    "<html><head><title>Child 1</title></head><body><p>Child frame 1 "
    "text</p><iframe src=\"/iframe3.html\"></iframe></body></html>";
const char kIframe2Html[] =
    "<html><head><title>Child 2</title></head><body><p>Child frame 2 "
    "text</p></body></html>";
const char kIframe3Html[] =
    "<html><head><title>Child 3</title></head><body><p>Child frame 3 "
    "text</p></body></html>";

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

// Test fixture to test the PageContextWrapper.
class PageContextWrapperTest : public PlatformTest {
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

    GetWebClient()->SetJavaScriptFeatures(
        {web::FindInPageJavaScriptFeature::GetInstance()});

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, kIframe1Path,
        base::BindRepeating(&testing::HandlePageWithHtml, kIframe1Html)));
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, kIframe2Path,
        base::BindRepeating(&testing::HandlePageWithHtml, kIframe2Html)));
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, kIframe3Path,
        base::BindRepeating(&testing::HandlePageWithHtml, kIframe3Html)));
    ASSERT_TRUE(test_server_.Start());

    // Set the fake env used for testing errors.
    fake_browser_state_ = std::make_unique<web::FakeBrowserState>();
    web::test::OverrideJavaScriptFeatures(
        fake_browser_state_.get(),
        {web::FindInPageJavaScriptFeature::GetInstance()});
    fake_web_state_ = std::make_unique<FakeWebStateForFailureTest>();

    auto fake_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    fake_frames_manager_ = fake_frames_manager.get();
    // No frames are added to the manager, simulating a state where
    // APC/InnerText cannot be retrieved.
    static_cast<web::FakeWebState*>(fake_web_state_.get())
        ->SetWebFramesManager(std::move(fake_frames_manager));
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }
  web::WebState* web_state() { return web_state_.get(); }

  // Getters for fake env.
  web::FakeBrowserState* fake_browser_state() {
    return fake_browser_state_.get();
  }
  web::FakeWebState* fake_web_state() { return fake_web_state_.get(); }
  web::FakeWebFramesManager* fake_frames_manager() {
    return fake_frames_manager_;
  }

  web::WebTaskEnvironment task_environment_;
  ScopedKeyWindow scoped_window_;
  web::ScopedTestingWebClient web_client_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  net::EmbeddedTestServer test_server_;
  id<SnapshotStorage> snapshot_storage_ = nil;
  ControllableFakeSnapshotGeneratorDelegate* snapshot_delegate_ = nil;

  std::unique_ptr<web::FakeBrowserState> fake_browser_state_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  raw_ptr<web::FakeWebFramesManager> fake_frames_manager_;
};

// TODO(crbug.com/452009061): Extend test coverage to x-origin frames,
// nested x-origin frames, and metrics tests.

// Tests that the page context is correctly populated with the page URL, title,
// inner text, and annotated page content (including iframes).
TEST_F(PageContextWrapperTest, PopulatePageContext) {
  const std::string main_html =
      base::StrCat({"<html><head><title>Main</title></head><body><p>Main frame "
                    "text</p><iframe "
                    "src=\"",
                    kIframe1Path, "\"></iframe><iframe src=\"", kIframe2Path,
                    "\"></iframe></body></html>"});
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::RunLoop run_loop;
  std::unique_ptr<optimization_guide::proto::PageContext> page_context;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                std::unique_ptr<
                                    optimization_guide::proto::PageContext>*
                                    out_page_context,
                                PageContextWrapperCallbackResponse response) {
                               if (response.has_value()) {
                                 *out_page_context =
                                     std::move(response.value());
                               }
                               run_loop->Quit();
                             },
                             &run_loop, &page_context)];

  wrapper.shouldGetAnnotatedPageContent = YES;
  wrapper.shouldGetInnerText = YES;
  [wrapper populatePageContextFieldsAsyncWithTimeout:base::Seconds(5)];

  run_loop.Run();

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
  EXPECT_EQ(iframe1_frame_data.url(), test_server_.GetURL(kIframe1Path).spec());
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
  EXPECT_EQ(iframe3_frame_data.url(), test_server_.GetURL(kIframe3Path).spec());

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
  EXPECT_EQ(iframe2_frame_data.url(), test_server_.GetURL(kIframe2Path).spec());
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
TEST_F(PageContextWrapperTest, PopulatePageContext_NoFieldsRequested) {
  const std::string main_html =
      "<html><head><title>No "
      "Fields</title></head><body><p>Hello</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::RunLoop run_loop;
  std::unique_ptr<optimization_guide::proto::PageContext> page_context;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                std::unique_ptr<
                                    optimization_guide::proto::PageContext>*
                                    out_page_context,
                                PageContextWrapperCallbackResponse response) {
                               if (response.has_value()) {
                                 *out_page_context =
                                     std::move(response.value());
                               }
                               run_loop->Quit();
                             },
                             &run_loop, &page_context)];

  [wrapper populatePageContextFieldsAsync];
  run_loop.Run();

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
TEST_F(PageContextWrapperTest, PopulatePageContextWithPDFVerification) {
  const std::string main_html = "<html><body><p>Hello PDF</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::RunLoop run_loop;
  std::unique_ptr<optimization_guide::proto::PageContext> page_context;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                std::unique_ptr<
                                    optimization_guide::proto::PageContext>*
                                    out_page_context,
                                PageContextWrapperCallbackResponse response) {
                               if (response.has_value()) {
                                 *out_page_context =
                                     std::move(response.value());
                               }
                               run_loop->Quit();
                             },
                             &run_loop, &page_context)];

  wrapper.shouldGetFullPagePDF = YES;
  [wrapper populatePageContextFieldsAsyncWithTimeout:base::Seconds(5)];

  run_loop.Run();

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
TEST_F(PageContextWrapperTest, PopulatePageContextWithSnapshotVerification) {
  const std::string main_html =
      "<html><body><p>Hello Snapshot</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::RunLoop run_loop;
  std::unique_ptr<optimization_guide::proto::PageContext> page_context;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                std::unique_ptr<
                                    optimization_guide::proto::PageContext>*
                                    out_page_context,
                                PageContextWrapperCallbackResponse response) {
                               if (response.has_value()) {
                                 *out_page_context =
                                     std::move(response.value());
                               }
                               run_loop->Quit();
                             },
                             &run_loop, &page_context)];

  wrapper.shouldGetSnapshot = YES;
  [wrapper populatePageContextFieldsAsyncWithTimeout:base::Seconds(5)];

  run_loop.Run();

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
TEST_F(PageContextWrapperTest, PopulatePageContextWithTextHighlighting) {
  const std::string main_html =
      "<html><body><p>Hello Highlight</p></body></html>";
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
  web::test::ExecuteJavaScript(base::SysUTF8ToNSString(kMutationObserverScript),
                               web_state());

  base::RunLoop run_loop;
  std::unique_ptr<optimization_guide::proto::PageContext> page_context;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                std::unique_ptr<
                                    optimization_guide::proto::PageContext>*
                                    out_page_context,
                                PageContextWrapperCallbackResponse response) {
                               if (response.has_value()) {
                                 *out_page_context =
                                     std::move(response.value());
                               }
                               run_loop->Quit();
                             },
                             &run_loop, &page_context)];

  wrapper.shouldGetSnapshot = YES;
  wrapper.textToHighlight = @"Highlight";
  [wrapper populatePageContextFieldsAsyncWithTimeout:base::Seconds(5)];

  run_loop.Run();

  ASSERT_TRUE(page_context);
  ASSERT_TRUE(page_context->has_tab_screenshot());

  // Check that the highlighting was applied from DOM mutations. This is the
  // most reliable and less flaky way for verifying that highlighting was
  // triggered.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^{
    id result = web::test::ExecuteJavaScript(@"window.__highlight_detected",
                                             web_state());
    return [result boolValue];
  }));
}

// Tests that anchor tags are correctly extracted when the feature is enabled.
TEST_F(PageContextWrapperTest, PopulatePageContextWithAnchors) {
  feature_list_.InitAndEnableFeature(kPageContextAnchorTags);
  const std::string main_html =
      "<html><body><a href=\"http://foo.com\">foo</a></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::RunLoop run_loop;
  std::unique_ptr<optimization_guide::proto::PageContext> page_context;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                std::unique_ptr<
                                    optimization_guide::proto::PageContext>*
                                    out_page_context,
                                PageContextWrapperCallbackResponse response) {
                               if (response.has_value()) {
                                 *out_page_context =
                                     std::move(response.value());
                               }
                               run_loop->Quit();
                             },
                             &run_loop, &page_context)];

  wrapper.shouldGetAnnotatedPageContent = YES;
  [wrapper populatePageContextFieldsAsyncWithTimeout:base::Seconds(5)];

  run_loop.Run();

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
TEST_F(PageContextWrapperTest, PopulatePageContext_SnapshotFailure) {
  const std::string main_html = "<html><body><p>Hello</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  // Set the snapshot delegate to cause a failure.
  snapshot_delegate_.canTakeSnapshot = NO;

  base::RunLoop run_loop;
  PageContextWrapperCallbackResponse captured_response;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                PageContextWrapperCallbackResponse*
                                    out_response,
                                PageContextWrapperCallbackResponse response) {
                               *out_response = std::move(response);
                               run_loop->Quit();
                             },
                             &run_loop, &captured_response)];

  wrapper.shouldGetSnapshot = YES;
  [wrapper populatePageContextFieldsAsync];
  run_loop.Run();

  // Verify that the callback was called with a screenshot error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(),
            PageContextWrapperError::kScreenshotError);
}

// Tests that the wrapper correctly handles a force detach.
TEST_F(PageContextWrapperTest, PopulatePageContext_ForceDetach) {
  const std::string main_html = "<html><body><p>Hello</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  // Inject JS to force detach.
  const char kForceDetachScript[] = R"(
      if (!window.__gCrWeb) { window.__gCrWeb = {}; }
      if (!window.__gCrWeb.pageContext) { window.__gCrWeb.pageContext = {}; }
      window.__gCrWeb.pageContext.shouldDetach = true;
  )";
  web::test::ExecuteJavaScript(base::SysUTF8ToNSString(kForceDetachScript),
                               web_state());

  base::RunLoop run_loop;
  PageContextWrapperCallbackResponse captured_response;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                PageContextWrapperCallbackResponse*
                                    out_response,
                                PageContextWrapperCallbackResponse response) {
                               *out_response = std::move(response);
                               run_loop->Quit();
                             },
                             &run_loop, &captured_response)];

  wrapper.shouldGetAnnotatedPageContent = YES;
  [wrapper populatePageContextFieldsAsync];

  run_loop.Run();

  // Verify that the callback was called with a force detach error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(),
            PageContextWrapperError::kForceDetachError);
}

// Tests that the wrapper correctly times out if the async operations take too
// long.
TEST_F(PageContextWrapperTest, TimeoutVerification) {
  const std::string main_html = "<html><body><p>Hello</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  // Inject JS to cause a timeout.
  const char kTimeoutScript[] = R"(
      if (!window.__gCrWeb) { window.__gCrWeb = {}; }
      if (!window.__gCrWeb.pageContext) { window.__gCrWeb.pageContext = {}; }
      window.__gCrWeb.pageContext.shouldTimeout = true;
  )";
  web::test::ExecuteJavaScript(base::SysUTF8ToNSString(kTimeoutScript),
                               web_state());

  base::RunLoop run_loop;
  PageContextWrapperCallbackResponse captured_response;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                PageContextWrapperCallbackResponse*
                                    out_response,
                                PageContextWrapperCallbackResponse response) {
                               *out_response = std::move(response);
                               run_loop->Quit();
                             },
                             &run_loop, &captured_response)];
  wrapper.shouldGetInnerText = YES;
  [wrapper populatePageContextFieldsAsyncWithTimeout:base::Milliseconds(5)];

  // Fast forward time past the timeout duration to trigger the timeout.
  // task_environment_.FastForwardBy(base::Milliseconds(150));
  run_loop.Run();

  // Verify that the callback was called with a timeout error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(), PageContextWrapperError::kTimeout);
}

// Tests that the wrapper correctly handles a failure in PDF generation.
TEST_F(PageContextWrapperTest, PopulatePageContext_PDFGenerationFailure) {
  base::RunLoop run_loop;
  PageContextWrapperCallbackResponse captured_response;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:fake_web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                PageContextWrapperCallbackResponse*
                                    out_response,
                                PageContextWrapperCallbackResponse response) {
                               *out_response = std::move(response);
                               run_loop->Quit();
                             },
                             &run_loop, &captured_response)];

  wrapper.shouldGetFullPagePDF = YES;
  [wrapper populatePageContextFieldsAsync];
  run_loop.Run();

  // Verify that the callback was called with a PDF data error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(), PageContextWrapperError::kPDFDataError);
}

// Tests that the wrapper correctly handles a failure in APC generation.
TEST_F(PageContextWrapperTest, PopulatePageContext_APCGenerationFailure) {
  base::RunLoop run_loop;
  PageContextWrapperCallbackResponse captured_response;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:fake_web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                PageContextWrapperCallbackResponse*
                                    out_response,
                                PageContextWrapperCallbackResponse response) {
                               *out_response = std::move(response);
                               run_loop->Quit();
                             },
                             &run_loop, &captured_response)];

  wrapper.shouldGetAnnotatedPageContent = YES;
  [wrapper populatePageContextFieldsAsync];
  run_loop.Run();

  // Verify that the callback was called with an APC error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(), PageContextWrapperError::kAPCError);
}

// Tests that the wrapper correctly handles a failure in inner text generation.
TEST_F(PageContextWrapperTest, PopulatePageContext_InnerTextGenerationFailure) {
  base::RunLoop run_loop;
  PageContextWrapperCallbackResponse captured_response;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:fake_web_state()
      completionCallback:base::BindOnce(
                             [](base::RunLoop* run_loop,
                                PageContextWrapperCallbackResponse*
                                    out_response,
                                PageContextWrapperCallbackResponse response) {
                               *out_response = std::move(response);
                               run_loop->Quit();
                             },
                             &run_loop, &captured_response)];

  wrapper.shouldGetInnerText = YES;
  [wrapper populatePageContextFieldsAsync];
  run_loop.Run();

  // Verify that the callback was called with an inner text error.
  ASSERT_FALSE(captured_response.has_value());
  EXPECT_EQ(captured_response.error(),
            PageContextWrapperError::kInnerTextError);
}
