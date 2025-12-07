// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"

#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/values_test_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

const char kMainPagePath[] = "/main.html";
const char kIframe1Path[] = "/iframe1.html";
const char kIframe2Path[] = "/iframe2.html";

const char kIframe1Html[] =
    "<html><head><title>Child 1</title></head><body><p>Child frame 1 "
    "text</p></body></html>";
const char kIframe2Html[] =
    "<html><head><title>Child 2</title></head><body><p>Child frame 2 "
    "text</p></body></html>";

}  // namespace

class PageContextExtractorJavaScriptFeatureTest : public PlatformTest {
 protected:
  PageContextExtractorJavaScriptFeatureTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    browser_state_ = std::make_unique<web::FakeBrowserState>();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    GetWebClient()->SetJavaScriptFeatures(
        {feature(), web::java_script_features::GetCommonJavaScriptFeature()});

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, kIframe1Path,
        base::BindRepeating(&testing::HandlePageWithHtml, kIframe1Html)));
    test_server_.RegisterRequestHandler(base::BindRepeating(
        net::test_server::HandlePrefixedRequest, kIframe2Path,
        base::BindRepeating(&testing::HandlePageWithHtml, kIframe2Html)));
    ASSERT_TRUE(test_server_.Start());
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }
  web::WebState* web_state() { return web_state_.get(); }
  PageContextExtractorJavaScriptFeature* feature() {
    return PageContextExtractorJavaScriptFeature::GetInstance();
  }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  net::EmbeddedTestServer test_server_;
};

// TODO(crbug.com/455761581): Test is flaky.
TEST_F(PageContextExtractorJavaScriptFeatureTest, FLAKY_ExtractPageContext) {
  const std::string main_html =
      base::StrCat({"<html><head><title>Main</title></head><body><p>Main frame "
                    "text</p><iframe "
                    "src=\"",
                    kIframe1Path, "\"></iframe><iframe src=\"", kIframe2Path,
                    "\"></iframe></body></html>"});
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::Value result_value;

  base::RunLoop run_loop;
  feature()->ExtractPageContext(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_anchors=*/false, "nonce", base::Milliseconds(100),
      base::BindOnce(
          [](base::RunLoop* r, base::Value* result_value,
             const base::Value* value) {
            *result_value = value->Clone();
            r->Quit();
          },
          &run_loop, &result_value));
  run_loop.Run();

  base::Value expected_value(
      base::Value::Dict()
          .Set("currentNodeInnerText", "Main frame text\n\n")
          .Set("title", "Main")
          .Set("sourceURL", test_server_.GetURL(kMainPagePath).spec())
          .Set(
              "children",
              base::Value::List()
                  .Append(base::Value::Dict()
                              .Set("currentNodeInnerText", "Child frame 1 text")
                              .Set("title", "Child 1")
                              .Set("sourceURL",
                                   test_server_.GetURL(kIframe1Path).spec()))
                  .Append(base::Value::Dict()
                              .Set("currentNodeInnerText", "Child frame 2 text")
                              .Set("title", "Child 2")
                              .Set("sourceURL",
                                   test_server_.GetURL(kIframe2Path).spec()))));

  EXPECT_THAT(result_value, base::test::IsSupersetOfValue(expected_value));
}

TEST_F(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContextWithAnchors) {
  const std::string main_html =
      "<html><head><title>Main</title></head><body><a "
      "href=\"http://foo.com\">foo</a></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::Value result_value;

  base::RunLoop run_loop;
  feature()->ExtractPageContext(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_anchors=*/true, "nonce", base::Milliseconds(100),
      base::BindOnce(
          [](base::RunLoop* r, base::Value* result_value,
             const base::Value* value) {
            *result_value = value->Clone();
            r->Quit();
          },
          &run_loop, &result_value));
  run_loop.Run();

  base::Value expected_value(
      base::Value::Dict()
          .Set("currentNodeInnerText", "foo")
          .Set("title", "Main")
          .Set("sourceURL", test_server_.GetURL(kMainPagePath).spec())
          .Set("links",
               base::Value::List().Append(base::Value::Dict()
                                              .Set("href", "http://foo.com/")
                                              .Set("linkText", "foo"))));

  EXPECT_THAT(result_value, base::test::IsSupersetOfValue(expected_value));
}

TEST_F(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContextWithForceDetach) {
  const std::string main_html = "<html><body><p>Hello</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  const char kForceDetachScript[] = R"(
      if (!window.__gCrWeb) { window.__gCrWeb = {}; }
      if (!window.__gCrWeb.pageContext) { window.__gCrWeb.pageContext = {}; }
      window.__gCrWeb.pageContext.shouldDetach = true;
      true;
  )";
  ASSERT_TRUE(web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(), base::SysUTF8ToNSString(kForceDetachScript), feature()));

  base::Value result_value;
  base::RunLoop run_loop;
  feature()->ExtractPageContext(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_anchors=*/false, "nonce", base::Milliseconds(100),
      base::BindOnce(
          [](base::RunLoop* r, base::Value* result_value,
             const base::Value* value) {
            *result_value = value->Clone();
            r->Quit();
          },
          &run_loop, &result_value));
  run_loop.Run();

  base::Value expected_value(
      base::Value::Dict().Set("shouldDetachPageContext", true));

  EXPECT_THAT(result_value, base::test::IsSupersetOfValue(expected_value));
}
