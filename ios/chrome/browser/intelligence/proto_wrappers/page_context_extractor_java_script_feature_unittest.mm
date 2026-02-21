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
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
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

    browser_state_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    GetWebClient()->SetJavaScriptFeatures({feature()});

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, kIframe1Path,
        base::BindRepeating(&testing::HandlePageWithHtml, kIframe1Html)));
    test_server_.RegisterRequestHandler(base::BindRepeating(
        net::test_server::HandlePrefixedRequest, kIframe2Path,
        base::BindRepeating(&testing::HandlePageWithHtml, kIframe2Html)));
    ASSERT_TRUE(test_server_.Start());

    xorigin_test_server_.RegisterRequestHandler(base::BindRepeating(
        net::test_server::HandlePrefixedRequest, kIframe2Path,
        base::BindRepeating(&testing::HandlePageWithHtml, kIframe2Html)));
    ASSERT_TRUE(xorigin_test_server_.Start());
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
  std::unique_ptr<TestProfileIOS> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  net::EmbeddedTestServer test_server_;
  net::EmbeddedTestServer xorigin_test_server_;
};

TEST_F(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContextWithCrossOriginFrames) {
  const std::string main_html =
      base::StrCat({"<html><head><title>Main</title></head><body><p>Main frame "
                    "text</p><iframe "
                    "src=\"",
                    xorigin_test_server_.GetURL(kIframe2Path).spec(),
                    "\"></iframe></body></html>"});
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::Value result_value;

  base::RunLoop run_loop;
  feature()->ExtractPageContext(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_anchors=*/false, /*include_cross_origin_frame_content=*/true,
      /*use_rich_extraction=*/false, "nonce", base::Milliseconds(100),
      base::BindOnce(
          [](base::RunLoop* r, base::Value* result_value,
             const base::Value* value) {
            *result_value = value->Clone();
            r->Quit();
          },
          &run_loop, &result_value));
  run_loop.Run();

  // We expect the child to have a remoteToken.
  const base::ListValue* children = result_value.GetDict().FindList("children");
  ASSERT_TRUE(children);
  ASSERT_EQ(1u, children->size());
  const base::Value& child = (*children)[0];
  EXPECT_TRUE(child.GetDict().FindString("remoteToken"));
}

TEST_F(PageContextExtractorJavaScriptFeatureTest, ExtractPageContext) {
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
      /*include_anchors=*/false, /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/false, "nonce", base::Milliseconds(100),
      base::BindOnce(
          [](base::RunLoop* r, base::Value* result_value,
             const base::Value* value) {
            *result_value = value->Clone();
            r->Quit();
          },
          &run_loop, &result_value));
  run_loop.Run();

  base::Value expected_value(
      base::DictValue()
          .Set("currentNodeInnerText", "Main frame text\n\n")
          .Set("title", "Main")
          .Set("sourceUrl", test_server_.GetURL(kMainPagePath).spec())
          .Set(
              "children",
              base::ListValue()
                  .Append(base::DictValue()
                              .Set("currentNodeInnerText", "Child frame 1 text")
                              .Set("title", "Child 1")
                              .Set("sourceUrl",
                                   test_server_.GetURL(kIframe1Path).spec()))
                  .Append(base::DictValue()
                              .Set("currentNodeInnerText", "Child frame 2 text")
                              .Set("title", "Child 2")
                              .Set("sourceUrl",
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
      /*include_anchors=*/true, /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/false, "nonce", base::Milliseconds(100),
      base::BindOnce(
          [](base::RunLoop* r, base::Value* result_value,
             const base::Value* value) {
            *result_value = value->Clone();
            r->Quit();
          },
          &run_loop, &result_value));
  run_loop.Run();

  base::Value expected_value(
      base::DictValue()
          .Set("currentNodeInnerText", "foo")
          .Set("title", "Main")
          .Set("sourceUrl", test_server_.GetURL(kMainPagePath).spec())
          .Set("links",
               base::ListValue().Append(base::DictValue()
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
      /*include_anchors=*/false, /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/false, "nonce", base::Milliseconds(100),
      base::BindOnce(
          [](base::RunLoop* r, base::Value* result_value,
             const base::Value* value) {
            *result_value = value->Clone();
            r->Quit();
          },
          &run_loop, &result_value));
  run_loop.Run();

  base::Value expected_value(
      base::DictValue().Set("shouldDetachPageContext", true));

  EXPECT_THAT(result_value, base::test::IsSupersetOfValue(expected_value));
}

// Test the extraction of the page context with RichExtraction.
TEST_F(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction) {
  const std::string html =
      "<html><head><title>TreeWalker "
      "Test</title></head><body><h1>Heading</h1><p>Paragraph "
      "text</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::Value result_value;
  base::RunLoop run_loop;
  feature()->ExtractPageContext(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_anchors=*/false, /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true, "nonce", base::Seconds(1),
      base::BindOnce(
          [](base::RunLoop* r, base::Value* result_value,
             const base::Value* value) {
            *result_value = value->Clone();
            r->Quit();
          },
          &run_loop, &result_value));
  run_loop.Run();

  const base::DictValue& dict = result_value.GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_GE(children->size(), 2u);

  // Check Heading
  const base::DictValue& heading_node = (*children)[0].GetDict();
  const base::ListValue* heading_children =
      heading_node.FindList("childrenNodes");
  ASSERT_TRUE(heading_children);
  ASSERT_GE(heading_children->size(), 1u);
  const base::DictValue& heading_text_node = (*heading_children)[0].GetDict();
  const std::string* heading_text = heading_text_node.FindStringByDottedPath(
      "contentAttributes.textInfo.textContent");
  ASSERT_TRUE(heading_text);
  EXPECT_EQ(*heading_text, "Heading");

  // Check Paragraph
  const base::DictValue& p_node = (*children)[1].GetDict();
  const base::ListValue* p_children = p_node.FindList("childrenNodes");
  ASSERT_TRUE(p_children);
  ASSERT_GE(p_children->size(), 1u);
  const base::DictValue& p_text_node = (*p_children)[0].GetDict();
  const std::string* p_text = p_text_node.FindStringByDottedPath(
      "contentAttributes.textInfo.textContent");
  ASSERT_TRUE(p_text);
  EXPECT_EQ(*p_text, "Paragraph text");

  // Check Frame Data
  const base::DictValue* frame_data = dict.FindDict("frameData");
  ASSERT_TRUE(frame_data);
  const std::string* title = frame_data->FindString("title");
  ASSERT_TRUE(title);
  EXPECT_EQ(*title, "TreeWalker Test");
}

// Test the extraction of the text size.
TEST_F(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_Text_Size) {
  const std::string html =
      "<html><body style=\"font-size: 16px\">"
      "<p style=\"font-size: 32px\">Extra Large</p>"  // 2.0 -> XL
      "<p style=\"font-size: 19px\">Large</p>"        // ~1.18 -> L
      "<p style=\"font-size: 16px\">Medium</p>"       // 1.0 -> M
      "<p style=\"font-size: 11px\">Small</p>"        // ~0.68 -> S
      "<p style=\"font-size: 10px\">Extra Small</p>"  // ~0.62 -> XS
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::Value result_value;
  base::RunLoop run_loop;
  feature()->ExtractPageContext(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_anchors=*/false, /*include_cross_origin_frame_content=*/false,
      /*use_apc_v2=*/true, "nonce", base::Seconds(1),
      base::BindOnce(
          [](base::RunLoop* r, base::Value* result_value,
             const base::Value* value) {
            *result_value = value->Clone();
            r->Quit();
          },
          &run_loop, &result_value));
  run_loop.Run();

  const base::DictValue& dict = result_value.GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_GE(children->size(), 5u);

  const std::vector<optimization_guide::proto::TextSize> expected_sizes = {
      optimization_guide::proto::TEXT_SIZE_XL,         // "Extra Large"
      optimization_guide::proto::TEXT_SIZE_L,          // "Large"
      optimization_guide::proto::TEXT_SIZE_M_DEFAULT,  // "Medium"
      optimization_guide::proto::TEXT_SIZE_S,          // "Small"
      optimization_guide::proto::TEXT_SIZE_XS,         // "Extra Small"
  };

  for (size_t i = 0; i < expected_sizes.size(); ++i) {
    const base::DictValue& node = (*children)[i].GetDict();
    const base::ListValue* node_children = node.FindList("childrenNodes");
    ASSERT_TRUE(node_children);
    const base::DictValue& text_node = (*node_children)[0].GetDict();

    std::optional<double> size = text_node.FindDoubleByDottedPath(
        "contentAttributes.textInfo.textStyle.textSize");
    int actual_size = static_cast<int>(size.value());
    EXPECT_EQ(actual_size, static_cast<int>(expected_sizes[i]))
        << "Failed at text size "
        << *text_node.FindStringByDottedPath(
               "contentAttributes.textInfo.textContent");
  }
}
