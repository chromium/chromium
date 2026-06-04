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
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/test/values_test_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/features/features.h"
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

// The IPC extraction method used to fetch the page context data.
enum class IPCExtractionMethod { kNative, kJSON };

}  // namespace

class PageContextExtractorJavaScriptFeatureTest
    : public PlatformTest,
      public ::testing::WithParamInterface<IPCExtractionMethod> {
 protected:
  PageContextExtractorJavaScriptFeatureTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    if (GetParam() == IPCExtractionMethod::kJSON) {
      scoped_feature_list_.InitAndEnableFeature(kPageContextIPCOptimization);
    }
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

  // Run the extraction according to the test parameter.
  // TODO(crbug.com/495446456): Clean up once the JSON experiment is done.
  std::optional<base::Value> RunExtraction(
      web::WebFrame* frame,
      bool include_cross_origin_frame_content,
      bool use_rich_extraction,
      bool use_rich_extraction_with_actionable,
      bool extract_paid_content,
      bool attempt_paid_content_json_fixing,
      const std::string& nonce,
      base::TimeDelta timeout) {
    base::test::TestFuture<std::optional<base::Value>> future;
    if (GetParam() == IPCExtractionMethod::kNative) {
      feature()->ExtractPageContext(
          frame, include_cross_origin_frame_content, use_rich_extraction,
          use_rich_extraction_with_actionable, extract_paid_content,
          attempt_paid_content_json_fixing,
          /*include_sensitive_payments_for_redaction=*/false, nonce, timeout,
          base::BindOnce(
              [](base::OnceCallback<void(std::optional<base::Value>)> callback,
                 const base::Value* value) {
                std::move(callback).Run(
                    value ? std::make_optional(value->Clone()) : std::nullopt);
              },
              future.GetCallback()));
    } else {
      feature()->ExtractPageContextJSON(
          frame, include_cross_origin_frame_content, use_rich_extraction,
          use_rich_extraction_with_actionable, extract_paid_content,
          attempt_paid_content_json_fixing,
          /*include_sensitive_payments_for_redaction=*/false, nonce, timeout,
          future.GetCallback());
    }
    return future.Take();
  }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  net::EmbeddedTestServer test_server_;
  net::EmbeddedTestServer xorigin_test_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/504266564): Make sure all the test pages are formatted nicely
// for human readers.

TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContextWithCrossOriginFrames) {
  const std::string main_html =
      base::StrCat({"<html><head><title>Main</title></head><body><p>Main frame "
                    "text</p><iframe "
                    "src=\"",
                    xorigin_test_server_.GetURL(kIframe2Path).spec(),
                    "\"></iframe></body></html>"});
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/true,
      /*use_rich_extraction=*/false,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce",
      base::Milliseconds(100));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  // We expect the child to have a remoteToken.
  const base::ListValue* children =
      result_value->GetDict().FindList("children");
  ASSERT_TRUE(children);
  ASSERT_EQ(1u, children->size());
  const base::Value& child = (*children)[0];
  EXPECT_TRUE(child.GetDict().FindString("remoteToken"));
}

TEST_P(PageContextExtractorJavaScriptFeatureTest, ExtractPageContext) {
  const std::string main_html =
      base::StrCat({"<html><head><title>Main</title></head><body><p>Main frame "
                    "text</p><iframe "
                    "src=\"",
                    kIframe1Path, "\"></iframe><iframe src=\"", kIframe2Path,
                    "\"></iframe></body></html>"});
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/false,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce",
      base::Milliseconds(100));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

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

  EXPECT_THAT(*result_value, base::test::IsSupersetOfValue(expected_value));
}

TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContextWithAnchors) {
  const std::string main_html =
      "<html><head><title>Main</title></head><body><a "
      "href=\"http://foo.com\">foo</a></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/false,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce",
      base::Milliseconds(100));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  base::Value expected_value(
      base::DictValue()
          .Set("currentNodeInnerText", "foo")
          .Set("title", "Main")
          .Set("sourceUrl", test_server_.GetURL(kMainPagePath).spec())
          .Set("links",
               base::ListValue().Append(base::DictValue()
                                            .Set("href", "http://foo.com/")
                                            .Set("linkText", "foo"))));

  EXPECT_THAT(*result_value, base::test::IsSupersetOfValue(expected_value));
}

TEST_P(PageContextExtractorJavaScriptFeatureTest,
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

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/false,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce",
      base::Milliseconds(100));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  base::Value expected_value(
      base::DictValue().Set("shouldDetachPageContext", true));

  EXPECT_THAT(*result_value, base::test::IsSupersetOfValue(expected_value));
}

// Validate that <form> elements with nested <input name="name"> or <input
// name="action"> don't clobber the values in the APC.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContextHandlesFormNamedElementPollution) {
  // Create HTML containing a form with children named "name" and "action".
  // Direct properties form.name and form.action will be overridden by DOM
  // Elements, which would result in unexpected type errors.
  const std::string main_html =
      "<html><head><title>Main</title></head><body>"
      "<form name=\"pollutedForm\" action=\"http://examplesite.com/submit\">"
      "  <input name=\"name\" value=\"polluted-input-element-name\">"
      "  <input name=\"action\" value=\"polluted-input-element-action\">"
      "</form>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(main_html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  const base::DictValue& dict = result_value->GetDict();

  // Drill down to the form node in the extracted Rich Extraction tree:
  // rootNode -> childrenNodes[0] (body) -> childrenNodes[0] (form)
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* body_children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(body_children && !body_children->empty());
  const base::DictValue& form_node = (*body_children)[0].GetDict();

  // Verify formName and actionUrl were safely extracted as strings rather than
  // being polluted or skipped.
  const std::string* form_name =
      form_node.FindStringByDottedPath("contentAttributes.formData.formName");
  ASSERT_TRUE(form_name);
  EXPECT_EQ(*form_name, "pollutedForm");

  const std::string* action_url =
      form_node.FindStringByDottedPath("contentAttributes.formData.actionUrl");
  ASSERT_TRUE(action_url);
  EXPECT_EQ(*action_url, "http://examplesite.com/submit");
}

// Test the extraction of the page context with RichExtraction.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction) {
  const std::string html =
      "<html><head><title>TreeWalker "
      "Test</title></head><body><h1>Heading</h1><p>Paragraph "
      "text</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  const base::DictValue& dict = result_value->GetDict();
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

TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtractionWithActionable) {
  const std::string html =
      "<html><body><button>Click me</button></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_GE(children->size(), 1u);

  const base::DictValue& button_node = (*children)[0].GetDict();
  const base::DictValue* interaction_info =
      button_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(interaction_info);
  EXPECT_TRUE(interaction_info->FindList("clickabilityReasons"));
}

// Test the extraction of the text size.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
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

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);

  const base::DictValue& dict = result_value->GetDict();
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

// Test the extraction of the text color.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_Text_Color) {
  const std::string html = "<html><body><p style=\"color: rgb(0, 255, "
                           "0)\">Green Text</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value) << "Extraction result is empty";
  ASSERT_TRUE(result_value->is_dict())
      << "Result is not a dictionary. Type: " << result_value->type();

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  // Check Paragraph
  const base::DictValue& p_node = (*children)[0].GetDict();
  const base::ListValue* p_children = p_node.FindList("childrenNodes");
  ASSERT_TRUE(p_children);
  ASSERT_EQ(p_children->size(), 1u);
  const base::DictValue& p_text_node = (*p_children)[0].GetDict();
  const std::string* p_text = p_text_node.FindStringByDottedPath(
      "contentAttributes.textInfo.textContent");
  ASSERT_TRUE(p_text);
  EXPECT_EQ(*p_text, "Green Text");

  // Check Color
  // Green: (0, 255, 0) -> (255 << 24) | (0 << 16) | (255 << 8) | 0
  // = 4278190080 | 0 | 65280 | 0 = 4278255360
  const std::string* color_str = p_text_node.FindStringByDottedPath(
      "contentAttributes.textInfo.textStyle.color");
  ASSERT_TRUE(color_str);
  EXPECT_EQ(*color_str, "4278255360");
}

// Test the extraction of the table caption.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_Table_Caption) {
  const std::string html =
      "<html><body>"
      "<table>"
      "  <caption style=\"text-transform: uppercase;\">My Table Name</caption>"
      "  <thead><tr><th>Header</th></tr></thead>"
      "  <tbody><tr><td>Body</td></tr></tbody>"
      "</table>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  const base::DictValue& table_node = (*children)[0].GetDict();
  std::optional<double> attribute_type =
      table_node.FindDoubleByDottedPath("contentAttributes.attributeType");
  ASSERT_TRUE(attribute_type.has_value());
  EXPECT_EQ(
      static_cast<int>(attribute_type.value()),
      static_cast<int>(optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE));

  const std::string* table_name = table_node.FindStringByDottedPath(
      "contentAttributes.tableData.tableName");
  ASSERT_TRUE(table_name);
  EXPECT_EQ(*table_name, "MY TABLE NAME");
}

// Test the extraction of the table caption when the text is nested inside
// other tags.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_Table_Caption_Nested) {
  const std::string html =
      "<html><body>"
      "<table>"
      "  <caption style=\"text-transform: uppercase;\">"
      "    <span>My <strong>Nested Table</strong> Name</span>"
      "  </caption>"
      "  <thead><tr><th>Header</th></tr></thead>"
      "  <tbody><tr><td>Body</td></tr></tbody>"
      "</table>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  const base::DictValue& table_node = (*children)[0].GetDict();
  std::optional<double> attribute_type =
      table_node.FindDoubleByDottedPath("contentAttributes.attributeType");
  ASSERT_TRUE(attribute_type.has_value());
  EXPECT_EQ(
      static_cast<int>(attribute_type.value()),
      static_cast<int>(optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE));

  const std::string* table_name = table_node.FindStringByDottedPath(
      "contentAttributes.tableData.tableName");
  ASSERT_TRUE(table_name);

  // Verifies that text from nested spans and strong tags is correctly extracted
  // and that the uppercase transformation still applies to the nested text.
  EXPECT_EQ(*table_name, "MY NESTED TABLE NAME");
}

// Verifies that internal SVG structural and metadata elements (like <title>,
// <defs>, and <script>) are strictly excluded from extraction to prevent
// non-visible technical strings from polluting the output.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_Svg) {
  const std::string html = "<html><body><svg width=\"100\" height=\"100\">"
                           "<title>SVG Title</title>"
                           "<desc>SVG Desc</desc>"
                           "<text>SVG Text</text>"
                           "<g><text><tspan>Nested Text</tspan></text></g>"
                           "</svg></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  // Check SVG
  const base::DictValue& svg_node = (*children)[0].GetDict();
  std::optional<double> attribute_type =
      svg_node.FindDoubleByDottedPath("contentAttributes.attributeType");
  ASSERT_TRUE(attribute_type.has_value());
  EXPECT_EQ(
      static_cast<int>(attribute_type.value()),
      static_cast<int>(optimization_guide::proto::CONTENT_ATTRIBUTE_SVG_ROOT));

  // Verify that the SVG is NOT treated as a leaf component.
  const base::ListValue* svg_children = svg_node.FindList("childrenNodes");
  ASSERT_TRUE(svg_children);

  // We expect "SVG Text" and the group containing "Nested Text".
  // <title> and <desc> are excluded.
  ASSERT_EQ(svg_children->size(), 2u);

  // Check for the "SVG Text" node.
  const base::DictValue& text_node = (*svg_children)[0].GetDict();
  EXPECT_EQ(*(text_node.FindStringByDottedPath(
                "contentAttributes.textInfo.textContent")),
            "SVG Text");

  // Check for nested text node.
  const base::DictValue& nested_text_node = (*svg_children)[1].GetDict();
  EXPECT_EQ(*(nested_text_node.FindStringByDottedPath(
                "contentAttributes.textInfo.textContent")),
            "Nested Text");
}

// Verifies that SVG anchors are correctly extracted.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_Svg_Anchor) {
  const std::string html =
      "<html><body>"
      "<svg width=\"200\" height=\"40\" xmlns=\"http://www.w3.org/2000/svg\">"
      "  <a href=\"relative_page.html\">"
      "    <text>SVG Text</text>"
      "  </a>"
      "</svg>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  // Check SVG
  const base::DictValue& svg_node = (*children)[0].GetDict();

  const base::ListValue* svg_children = svg_node.FindList("childrenNodes");
  ASSERT_TRUE(svg_children);
  ASSERT_EQ(svg_children->size(), 1u);

  // Check Anchor
  const base::DictValue& anchor_node = (*svg_children)[0].GetDict();
  std::optional<double> attribute_type =
      anchor_node.FindDoubleByDottedPath("contentAttributes.attributeType");
  ASSERT_TRUE(attribute_type.has_value());
  EXPECT_EQ(
      static_cast<int>(attribute_type.value()),
      static_cast<int>(optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR));

  const std::string* url =
      anchor_node.FindStringByDottedPath("contentAttributes.anchorData.url");
  ASSERT_TRUE(url);
  EXPECT_EQ(*url, "relative_page.html");
}

// Verifies that SVG elements rendered invisible via CSS (display/visibility)
// are excluded.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_Svg_Visibility) {
  const std::string html =
      "<html><body><svg width=\"200\" height=\"200\">"
      "<text>Visible Text</text>"
      "<text style=\"display: none\">Hidden Display Text</text>"
      "<text style=\"visibility: hidden\">Hidden Visibility Text</text>"
      "<g style=\"display: none\"><text>Hidden Group Text</text></g>"
      "</svg></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  // Check SVG
  const base::DictValue& svg_node = (*children)[0].GetDict();
  const base::ListValue* svg_children = svg_node.FindList("childrenNodes");
  ASSERT_TRUE(svg_children);

  ASSERT_EQ(svg_children->size(), 1u);
  const base::DictValue& text_node = (*svg_children)[0].GetDict();
  EXPECT_EQ(*((text_node.FindStringByDottedPath(
                "contentAttributes.textInfo.textContent"))),
            "Visible Text");
}

// Test the extraction of the ARIA label and aria-labelledby.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_BothSourcesOfAriaLabels) {
  const std::string html =
      "<html><body>"
      "  <div id=\"label1\">Label 1</div>"
      "  <div id=\"label2\">Label 2</div>"
      "  <div id=\"label3\">Unused label</div>"
      "  <button aria-label=\"Direct Label\" aria-labelledby=\"label1 "
      "label2\">Click me</button>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(4));

  ASSERT_TRUE(result_value);
  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);

  ASSERT_EQ(children->size(), 4u);

  const base::DictValue& button_node = (*children)[3].GetDict();
  const std::string* label =
      button_node.FindStringByDottedPath("contentAttributes.label");
  ASSERT_TRUE(label);
  EXPECT_EQ(*label, "Label 1 Label 2");
}

// Test the extraction of the ARIA label when only aria-label is present.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_AriaLabelOnly) {
  const std::string html =
      "<html><body>"
      "  <button aria-label=\"Direct Label\">Click me</button>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(4));

  ASSERT_TRUE(result_value);
  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);

  ASSERT_EQ(children->size(), 1u);

  const base::DictValue& button_node = (*children)[0].GetDict();
  const std::string* label =
      button_node.FindStringByDottedPath("contentAttributes.label");
  ASSERT_TRUE(label);
  EXPECT_EQ(*label, "Direct Label");
}

// Test that -webkit-text-security is respected for ARIA label and table name
// extraction.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_TextSecurityBypass) {
  const std::string html =
      "<html><body>"
      "  <span id=\"secret\" style=\"-webkit-text-security: "
      "disc\">9999-8888</span>"
      "  <button id=\"btn\" aria-labelledby=\"secret\">Submit</button>"
      "  <table>"
      "    <caption style=\"-webkit-text-security: disc\">My Secret "
      "Table</caption>"
      "    <thead><tr><th>Header</th></tr></thead>"
      "    <tbody><tr><td>Body</td></tr></tbody>"
      "  </table>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(4));

  ASSERT_TRUE(result_value);
  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);

  // Expected children:
  // 1. Span container (with the masked text inside)
  // 2. Button (referencing the span via aria-labelledby)
  // 3. Table (with the caption)
  ASSERT_EQ(children->size(), 3u);

  const std::string expected_mask =
      "\u2022\u2022\u2022\u2022\u2022\u2022\u2022";

  // Verify Span's text node is masked correctly.
  const base::DictValue& text_node = (*children)[0].GetDict();
  const std::string* span_text = text_node.FindStringByDottedPath(
      "contentAttributes.textInfo.textContent");
  ASSERT_TRUE(span_text);
  EXPECT_EQ(*span_text, expected_mask);

  // Verify Button's ARIA label is masked correctly.
  const base::DictValue& button_node = (*children)[1].GetDict();
  const std::string* button_label =
      button_node.FindStringByDottedPath("contentAttributes.label");
  ASSERT_TRUE(button_label);
  EXPECT_EQ(*button_label, expected_mask);

  // Verify Table's caption is masked correctly.
  const base::DictValue& table_node = (*children)[2].GetDict();
  std::optional<double> attribute_type =
      table_node.FindDoubleByDottedPath("contentAttributes.attributeType");
  ASSERT_TRUE(attribute_type.has_value());
  EXPECT_EQ(
      static_cast<int>(attribute_type.value()),
      static_cast<int>(optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE));
  const std::string* table_name = table_node.FindStringByDottedPath(
      "contentAttributes.tableData.tableName");
  ASSERT_TRUE(table_name);
  EXPECT_EQ(*table_name, expected_mask);
}

TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_AriaRoles) {
  const std::string html =
      "<html><body>"
      "  <header role=\"banner\">Header</header>"
      "  <nav role=\"navigation\">Nav</nav>"
      "  <div role=\"search\">Search</div>"
      "  <main role=\"main\">Main</main>"
      "  <article role=\"article\">Article</article>"
      "  <section role=\"region\">Section</section>"
      "  <aside role=\"complementary\">Aside</aside>"
      "  <footer role=\"contentinfo\">Footer</footer>"
      "  <div style=\"content-visibility: hidden;\">Hidden</div>"
      "  <div role=\"unknown search\">Fallback</div>"
      "  <footer role=\"banner\">Multi</footer>"
      "</body></html>";

  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(4));

  ASSERT_TRUE(result_value->is_dict());
  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 11u);

  const std::vector<std::vector<int>> expected_roles = {
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_HEADER)},
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_NAV)},
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_SEARCH)},
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_MAIN)},
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_ARTICLE)},
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_SECTION)},
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_ASIDE)},
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_FOOTER)},
      {static_cast<int>(
          optimization_guide::proto::ANNOTATED_ROLE_CONTENT_HIDDEN)},
      // Skips "unknown", maps "search"
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_SEARCH)},
      // Gets FOOTER from tag, and HEADER from aria
      {static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_FOOTER),
       static_cast<int>(optimization_guide::proto::ANNOTATED_ROLE_HEADER)},
  };

  for (size_t i = 0; i < expected_roles.size(); ++i) {
    const base::DictValue& node = (*children)[i].GetDict();
    const base::ListValue* annotated_roles =
        node.FindListByDottedPath("contentAttributes.annotatedRoles");
    ASSERT_TRUE(annotated_roles);
    ASSERT_EQ(annotated_roles->size(), expected_roles[i].size());

    for (size_t j = 0; j < expected_roles[i].size(); ++j) {
      EXPECT_EQ(static_cast<int>((*annotated_roles)[j].GetDouble()),
                expected_roles[i][j])
          << "mismatch for child " << i << " at role index " << j;
    }
  }
}

TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_AXRole_WithActionable) {
  const std::string html =
      "<html><body>"
      "  <div role=\"button\">Button</div>"
      "  <div role=\"random_unknown_role\" tabindex=\"0\">Unknown</div>"
      "  <div tabindex=\"0\">No Role</div>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(4));

  ASSERT_TRUE(result_value->is_dict());
  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);

  ASSERT_EQ(children->size(), 3u);

  // Check the button node structure.
  const base::DictValue& button_node = (*children)[0].GetDict();
  std::optional<double> button_role =
      button_node.FindDoubleByDottedPath("contentAttributes.ariaRole");
  ASSERT_TRUE(button_role.has_value());
  EXPECT_EQ(static_cast<int>(button_role.value()), 9);  // AX_ROLE_BUTTON

  const base::ListValue* button_content_children =
      button_node.FindList("childrenNodes");
  ASSERT_TRUE(button_content_children);
  ASSERT_EQ(button_content_children->size(), 1u);
  const base::DictValue& button_text_node =
      (*button_content_children)[0].GetDict();
  const std::string* button_text = button_text_node.FindStringByDottedPath(
      "contentAttributes.textInfo.textContent");
  ASSERT_TRUE(button_text);
  EXPECT_EQ(*button_text, "Button");

  // Check the unknown role node structure.
  const base::DictValue& unknown_node = (*children)[1].GetDict();
  std::optional<double> unknown_role =
      unknown_node.FindDoubleByDottedPath("contentAttributes.ariaRole");
  ASSERT_TRUE(unknown_role.has_value());
  EXPECT_EQ(static_cast<int>(unknown_role.value()), 181);  // AX_ROLE_UNKNOWN

  const base::ListValue* unknown_content_children =
      unknown_node.FindList("childrenNodes");
  ASSERT_TRUE(unknown_content_children);
  ASSERT_EQ(unknown_content_children->size(), 1u);
  const base::DictValue& unknown_text_node =
      (*unknown_content_children)[0].GetDict();
  const std::string* unknown_text = unknown_text_node.FindStringByDottedPath(
      "contentAttributes.textInfo.textContent");
  ASSERT_TRUE(unknown_text);
  EXPECT_EQ(*unknown_text, "Unknown");

  // Check the node with NO role (Preserved due to tabindex)
  const base::DictValue& no_role_node = (*children)[2].GetDict();
  std::optional<double> missing_role =
      no_role_node.FindDoubleByDottedPath("contentAttributes.ariaRole");
  ASSERT_TRUE(missing_role.has_value());
  EXPECT_EQ(static_cast<int>(missing_role.value()), 181);  // AX_ROLE_UNKNOWN
}

// Test the extraction of label elements and their associated control IDs.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_LabelForDomNodeId) {
  const std::string html = "<html><body><label for=\"myInput\"><span>My "
                           "<strong>Label</strong></span></label><input "
                           "id=\"myInput\" type=\"text\"></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,  // Required to trigger
                                                     // label control resolution
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 2u);

  const base::DictValue* label_node = &(*children)[0].GetDict();
  const base::DictValue* input_node = &(*children)[1].GetDict();

  std::optional<double> label_attribute_type =
      label_node->FindDoubleByDottedPath("contentAttributes.attributeType");
  ASSERT_EQ(label_attribute_type,
            static_cast<double>(
                optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER));

  std::optional<double> input_attribute_type =
      input_node->FindDoubleByDottedPath("contentAttributes.attributeType");
  ASSERT_EQ(input_attribute_type,
            static_cast<double>(
                optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL));

  std::optional<double> input_dom_node_id =
      input_node->FindDoubleByDottedPath("contentAttributes.domNodeId");
  ASSERT_TRUE(input_dom_node_id.has_value());

  std::optional<double> label_for_dom_node_id =
      label_node->FindDoubleByDottedPath("contentAttributes.labelForDomNodeId");
  ASSERT_TRUE(label_for_dom_node_id.has_value());
  EXPECT_EQ(*label_for_dom_node_id, *input_dom_node_id);
}

// Test the Scroller Info extraction.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_ScrollerInfo) {
  const std::string html =
      "<html><body>"
      "  <div id=\"scroller\" style=\"width: 100px; height: 100px; overflow: "
      "auto;\">"
      "    <div style=\"width: 200px; height: 300px;\"></div>"
      "  </div>"
      "  <script>"
      "    let el = document.getElementById('scroller');"
      "    el.scrollLeft = 50;"
      "    el.scrollTop = 70;"
      "  </script>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);

  if (GetParam() == IPCExtractionMethod::kJSON) {
    // In optimized mode, generic scrollable divs does not extract scroller
    // info.
    const base::DictValue& div_node = (*children)[0].GetDict();
    const base::DictValue* interaction_info =
        div_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
    EXPECT_FALSE(interaction_info);
    return;
  }

  ASSERT_GE(children->size(), 1u);

  const base::DictValue& div_node = (*children)[0].GetDict();
  const base::DictValue* interaction_info =
      div_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(interaction_info);

  const base::DictValue* scroller_info =
      interaction_info->FindDict("scrollerInfo");
  ASSERT_TRUE(scroller_info);

  const base::DictValue* scrolling_bounds =
      scroller_info->FindDict("scrollingBounds");
  ASSERT_TRUE(scrolling_bounds);

  std::optional<double> width = scrolling_bounds->FindDouble("width");
  ASSERT_TRUE(width.has_value());
  EXPECT_EQ(static_cast<int>(width.value()), 200);

  std::optional<double> height = scrolling_bounds->FindDouble("height");
  ASSERT_TRUE(height.has_value());
  EXPECT_EQ(static_cast<int>(height.value()), 300);

  const base::DictValue* visible_area = scroller_info->FindDict("visibleArea");
  ASSERT_TRUE(visible_area);

  std::optional<double> visible_x = visible_area->FindDouble("x");
  ASSERT_TRUE(visible_x.has_value());
  EXPECT_EQ(static_cast<int>(visible_x.value()), 50);

  std::optional<double> visible_y = visible_area->FindDouble("y");
  ASSERT_TRUE(visible_y.has_value());
  EXPECT_EQ(static_cast<int>(visible_y.value()), 70);

  std::optional<double> visible_w = visible_area->FindDouble("width");
  ASSERT_TRUE(visible_w.has_value());
  EXPECT_EQ(static_cast<int>(visible_w.value()), 100);

  std::optional<double> visible_h = visible_area->FindDouble("height");
  ASSERT_TRUE(visible_h.has_value());
  EXPECT_EQ(static_cast<int>(visible_h.value()), 100);

  std::optional<double> visible_t = visible_area->FindDouble("top");
  ASSERT_TRUE(visible_t.has_value());
  EXPECT_EQ(static_cast<int>(visible_t.value()), 70);

  std::optional<double> visible_l = visible_area->FindDouble("left");
  ASSERT_TRUE(visible_l.has_value());
  EXPECT_EQ(static_cast<int>(visible_l.value()), 50);

  std::optional<double> visible_b = visible_area->FindDouble("bottom");
  ASSERT_TRUE(visible_b.has_value());
  EXPECT_EQ(static_cast<int>(visible_b.value()), 170);

  std::optional<double> visible_r = visible_area->FindDouble("right");
  ASSERT_TRUE(visible_r.has_value());
  EXPECT_EQ(static_cast<int>(visible_r.value()), 150);

  std::optional<bool> user_scrollable_horizontal =
      scroller_info->FindBool("userScrollableHorizontal");
  ASSERT_TRUE(user_scrollable_horizontal.has_value());
  EXPECT_TRUE(user_scrollable_horizontal.value());

  std::optional<bool> user_scrollable_vertical =
      scroller_info->FindBool("userScrollableVertical");
  ASSERT_TRUE(user_scrollable_vertical.has_value());
  EXPECT_TRUE(user_scrollable_vertical.value());
}

// Tests that scroller info is extracted for all nodes when a canvas is present.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_ScrollerInfo_Canvas) {
  const std::string html =
      "<html><body>"
      "  <canvas id=\"canvas_scroller\" style=\"display: block; width: 100px; "
      "height: 100px; overflow: auto;\">"
      "    <div style=\"width: 200px; height: 300px;\"></div>"
      "  </canvas>"
      "  <div id=\"div_scroller\" style=\"width: 100px; height: 100px; "
      "overflow: auto;\">"
      "    <div style=\"width: 200px; height: 300px;\"></div>"
      "  </div>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);

  ASSERT_GE(children->size(), 2u);

  // 1. Check canvas_scroller
  const base::DictValue& canvas_node = (*children)[0].GetDict();
  const base::DictValue* canvas_interaction_info =
      canvas_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(canvas_interaction_info);
  const base::DictValue* canvas_scroller_info =
      canvas_interaction_info->FindDict("scrollerInfo");
  ASSERT_TRUE(canvas_scroller_info);
  ASSERT_TRUE(canvas_scroller_info->FindDict("scrollingBounds"));
  ASSERT_TRUE(canvas_scroller_info->FindDict("visibleArea"));

  // 2. Check div_scroller
  const base::DictValue& div_node = (*children)[1].GetDict();
  const base::DictValue* div_interaction_info =
      div_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(div_interaction_info);
  const base::DictValue* div_scroller_info =
      div_interaction_info->FindDict("scrollerInfo");
  ASSERT_TRUE(div_scroller_info);
  ASSERT_TRUE(div_scroller_info->FindDict("scrollingBounds"));
  ASSERT_TRUE(div_scroller_info->FindDict("visibleArea"));
}

// Test that absolute positioned elements are not clipped by static ancestors
// with overflow: hidden.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_Geometry_AbsoluteClipping) {
  const std::string html =
      "<html><body>"
      "  <div style=\"position: static; overflow: hidden; width: 100px; "
      "height: 100px;\">"
      "    <div id=\"target\" role=\"region\" style=\"position: absolute; top: "
      "120px; left: 120px; width: 50px; height: 50px;\">"
      "      Absolute Content"
      "    </div>"
      "  </div>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);

  // We expect the static div to be extracted (as it has overflow: hidden and
  // acts as container).
  ASSERT_GE(children->size(), 1u);
  const base::DictValue& div_node = (*children)[0].GetDict();
  const base::ListValue* div_children = div_node.FindList("childrenNodes");
  ASSERT_TRUE(div_children);
  ASSERT_GE(div_children->size(), 1u);

  const base::DictValue& target_node = (*div_children)[0].GetDict();

  const base::DictValue* geometry =
      target_node.FindDictByDottedPath("contentAttributes.geometry");
  ASSERT_TRUE(geometry) << "Geometry not found for target node";

  const base::DictValue* visible_box = geometry->FindDict("visibleBoundingBox");
  ASSERT_TRUE(visible_box) << "visibleBoundingBox not found";

  // If it was clipped to the parent (100x100), width/height would likely be 0
  // or negative since it's at 120,120. So if width is 50, it means it was NOT
  // clipped.
  std::optional<double> width = visible_box->FindDouble("width");
  ASSERT_TRUE(width.has_value());
  EXPECT_EQ(static_cast<int>(width.value()), 50);

  std::optional<double> height = visible_box->FindDouble("height");
  ASSERT_TRUE(height.has_value());
  EXPECT_EQ(static_cast<int>(height.value()), 50);

  const base::DictValue* outer_box = geometry->FindDict("outerBoundingBox");
  ASSERT_TRUE(outer_box);

  // Verify that outer bounds equal visible bounds when there is no clipping.
  EXPECT_EQ(outer_box->FindDouble("width"), width);
  EXPECT_EQ(outer_box->FindDouble("height"), height);
  EXPECT_EQ(outer_box->FindDouble("x"), visible_box->FindDouble("x"));
  EXPECT_EQ(outer_box->FindDouble("y"), visible_box->FindDouble("y"));
}

// Test that absolute positioned elements are clipped by positioned ancestors
// with overflow: hidden.
TEST_P(
    PageContextExtractorJavaScriptFeatureTest,
    ExtractPageContext_RichExtraction_Geometry_AbsoluteClipping_PositionedAncestor) {
  const std::string html =
      "<html><body>"
      "  <div style=\"position: relative; overflow: hidden; width: 100px; "
      "height: 100px;\">"
      "    <div id=\"target\" role=\"region\" style=\"position: absolute; top: "
      "120px; left: 120px; width: 50px; height: 50px;\">"
      "      Absolute Content"
      "    </div>"
      "  </div>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);

  // We expect the relative div to be extracted.
  ASSERT_GE(children->size(), 1u);
  const base::DictValue& div_node = (*children)[0].GetDict();
  const base::ListValue* div_children = div_node.FindList("childrenNodes");
  ASSERT_TRUE(div_children);

  // Since the child is positioned at top: 120px, left: 120px relative to the
  // parent, and the parent container size is 100px x 100px with overflow:
  // hidden, the child is completely clipped. If the element gets extracted, its
  // visible bounding box is omitted by the script.
  if (div_children->size() == 0) {
    // If not extracted because not visible, that's also a valid result of
    // clipping.
    return;
  }

  ASSERT_GE(div_children->size(), 1u);
  const base::DictValue& target_node = (*div_children)[0].GetDict();

  const base::DictValue* geometry =
      target_node.FindDictByDottedPath("contentAttributes.geometry");
  ASSERT_TRUE(geometry);

  const base::DictValue* visible_box = geometry->FindDict("visibleBoundingBox");
  // Since the child is positioned at top: 120px, left: 120px relative to its
  // parent, and the parent container size is 100px x 100px with overflow:
  // hidden, the child is fully clipped and its visible bounding box is omitted
  // by the script.
  EXPECT_FALSE(visible_box)
      << "Expected visibleBoundingBox to be missing for fully clipped element";
}

// Test that absolute positioned elements are clipped by static ancestors with
// overflow: hidden if their containing block is a descendant of the clipping
// ancestor.
// In this scenario, the parent is positioned relative, so it is clipped by the
// static grandparent container. This visible clip bounds constraint is then
// correctly inherited by its absolute positioned child. If the parent instead
// had absolute positioning, it would skip the static grandparent entirely,
// meaning the absolute child would not be clipped.
TEST_P(
    PageContextExtractorJavaScriptFeatureTest,
    ExtractPageContext_RichExtraction_Geometry_AbsoluteClipping_ChainedContainingBlock) {
  const std::string html =
      "<html><body>"
      "  <div style=\"position: static; overflow: hidden; width: 100px; "
      "height: 100px;\">"
      "    <div role=\"region\" style=\"position: relative; overflow: visible; "
      "width: 200px; height: 200px;\">"
      "      <div id=\"target\" role=\"region\" style=\"position: absolute; "
      "top: 120px; left: 120px; width: 50px; height: 50px;\">"
      "        Absolute Content"
      "      </div>"
      "    </div>"
      "  </div>"
      "</body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);

  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);

  ASSERT_GE(children->size(), 1u);
  const base::DictValue& static_div = (*children)[0].GetDict();
  const base::ListValue* static_div_children =
      static_div.FindList("childrenNodes");
  ASSERT_TRUE(static_div_children);

  ASSERT_GE(static_div_children->size(), 1u);
  const base::DictValue& relative_div = (*static_div_children)[0].GetDict();
  const base::ListValue* relative_div_children =
      relative_div.FindList("childrenNodes");
  ASSERT_TRUE(relative_div_children);

  if (relative_div_children->size() == 0) {
    return;
  }

  ASSERT_GE(relative_div_children->size(), 1u);
  const base::DictValue& target_node = (*relative_div_children)[0].GetDict();

  const base::DictValue* geometry =
      target_node.FindDictByDottedPath("contentAttributes.geometry");
  ASSERT_TRUE(geometry);

  const base::DictValue* visible_box = geometry->FindDict("visibleBoundingBox");
  EXPECT_FALSE(visible_box) << "Expected visibleBoundingBox to be missing for "
                               "chained fully clipped element";
}

// Verifies that ExtractPageContext payload is a string when IPC optimization
// is enabled and a dictionary otherwise.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       VerifiesRawJavascriptExtractionResultType) {
  const std::string html = "<html><body><p>Test</p></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  base::test::TestFuture<base::Value> future;

  base::ListValue parameters;
  parameters.Append("nonce");
  parameters.Append(false);  // include_cross_origin_frame_content
  parameters.Append(false);  // use_rich_extraction
  parameters.Append(false);  // use_rich_extraction_with_actionable
  parameters.Append(false);  // extract_paid_content
  parameters.Append(false);  // attempt_paid_content_json_fixing

  feature()
      ->GetWebFramesManager(web_state())
      ->GetMainWebFrame()
      ->CallJavaScriptFunction(
          "pageContextExtractor.extractPageContext", parameters,
          base::BindOnce(
              [](base::OnceCallback<void(base::Value)> callback,
                 const base::Value* value) {
                std::move(callback).Run(value ? value->Clone() : base::Value());
              },
              future.GetCallback()),
          base::Seconds(1));

  base::Value raw_result = future.Take();

  if (GetParam() == IPCExtractionMethod::kJSON) {
    EXPECT_TRUE(raw_result.is_string())
        << "Expected a string payload when IPC optimization is enabled.";
  } else {
    EXPECT_TRUE(raw_result.is_dict())
        << "Expected a dictionary payload when native extraction is used.";
  }
}

// Test the extraction of Z-order in a standard scenario where the actionable
// element is fully visible and reachable.
//
// Layout and Expected Z-Order (Painting Order):
//
// +-----------------------------------+
// |                                   |
// |  [=========] (1. Button)          |
// |                                   |
// +-----------------------------------+
//
// Expectations:
// - A single actionable button is on the screen.
// - It should receive a documentScopedZOrder of 1.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_ZOrder_Generic) {
  const std::string html = R"(
    <html>
      <body style="margin: 0; padding: 0;">
        <button style="position: absolute; top: 10px; left: 10px; width: 100px; height: 50px;">
          Click me
        </button>
      </body>
    </html>
  )";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_GE(children->size(), 1u);

  const base::DictValue& button_node = (*children)[0].GetDict();
  const base::DictValue* interaction_info =
      button_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(interaction_info);
  EXPECT_EQ(interaction_info->FindDouble("documentScopedZOrder"), 1.0);
}

// Test the extraction of Z-order when an actionable element is physically
// covered by another element, but the covering element has
// pointer-events: none, making the actionable element underneath reachable.
//
// Layout and Expected Z-Order (Painting Order):
//
// +-----------------------------------+
// |                                   |
// | +-----------------------+         |
// | |///////////////////////|         |
// | |//[=========]//////////|         |
// | |//(1. Button)//////////|         |
// | |///////////////////////|         |
// | +-----------------------+         |
// | (2. Overlay - pointer-events:none)|
// |                                   |
// +-----------------------------------+
//
// Legend:
// - "/": Area covered by the "transparent" overlay where the elements
//        underneath are reachable.
//
// Expectations:
// - The overlay covers the button but has pointer-events: none, making the
// button reachable via raycasts.
// - The overlay is a generic container and not actionable.
// - The button should successfully receive a
// documentScopedZOrder of 1.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_ZOrder_PointerEventsNone) {
  const std::string html = R"(
    <html>
      <body style="margin: 0; padding: 0;">
        <button style="position: absolute; top: 10px; left: 10px; width: 100px; height: 50px;">
          Click me
        </button>
        <div style="position: absolute; top: 0; left: 0; width: 200px; height: 100px; background: red; pointer-events: none;">
          Overlay
        </div>
      </body>
    </html>
  )";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 2u);

  const base::DictValue& button_node = (*children)[0].GetDict();
  const base::DictValue* interaction_info =
      button_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(interaction_info);
  EXPECT_EQ(interaction_info->FindDouble("documentScopedZOrder"), 1.0);

  const base::DictValue& div_node = (*children)[1].GetDict();
  EXPECT_FALSE(
      div_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo"));
}

// Test the extraction of Z-order when the topmost element at the hit test
// coordinate is a child/descendant of the actionable element (e.g., a span
// inside a button). The parent element should still receive a Z-order.
//
// Layout and Expected Z-Order (Painting Order):
//
// +-----------------------------------+
// |                                   |
// |  [=========]                      |
// |  [ (Span)  ] (1. Button)          |
// |  [=========]                      |
// |                                   |
// +-----------------------------------+
//
// Expectations:
// - The span completely covers the button, but it is a child of the button.
// - Hit tests on the button will hit the span, which is a descendant, meaning
// the button is still considered reachable.
// - The button should receive a documentScopedZOrder of 1.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_ZOrder_ChildElementTopMost) {
  const std::string html = R"(
    <html>
      <body style="margin: 0; padding: 0;">
        <button style="position: absolute; top: 10px; left: 10px; width: 100px; height: 50px;">
          <span style="display: block; width: 100%; height: 100%;">
            Click me
          </span>
        </button>
      </body>
    </html>
  )";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_GE(children->size(), 1u);

  const base::DictValue& button_node = (*children)[0].GetDict();
  const base::DictValue* interaction_info =
      button_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(interaction_info);
  EXPECT_EQ(interaction_info->FindDouble("documentScopedZOrder"), 1.0);
}

// Test the extraction of Z-order when an actionable element is completely
// outside the visible viewport. It should not receive a Z-order.
//
// Layout and Expected Z-Order (Painting Order):
//
// +-----------------------------------+ (Visible Viewport)
// |                                   |
// |                                   |
// +-----------------------------------+
//  . . . . . . . . . . . . . . . . . .
//  .                                 .
//  . [=========] (Button)            .
//  .                                 .
//  . . . . . . . . . . . . . . . . . .
//
// Expectations:
// - The button is rendered completely outside the visible viewport.
// - elementsFromPoint at an offscreen coordinate returns null/empty.
// - It should not receive a Z-order.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_ZOrder_OffScreen) {
  const std::string html = R"(
    <html>
      <body style="margin: 0; padding: 0;">
        <button style="position: absolute; top: 2000px; left: 10px; width: 100px; height: 50px;">
          Click me
        </button>
      </body>
    </html>
  )";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_GE(children->size(), 1u);

  const base::DictValue& button_node = (*children)[0].GetDict();
  const base::DictValue* interaction_info =
      button_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(interaction_info);
  EXPECT_FALSE(interaction_info->FindDouble("documentScopedZOrder"));
}

// Tests extraction of Z-order for overlapping actionable elements.
//
// Layout and Expected Z-Order (Painting Order):
//
// +-----------------------------------+ (0. Background)     Y-axis
// |                                   |                       | 0px
// |  [=========] (1. Button 1)        |                       | 10px
// |                                   |                       |
// |                                   |                       |
// | +-----------------------+ - - - - |                       | 90px
// | |///////////////////////|         |                       |
// | |//[=========]//////////|         |                       | 100px
// | |//(2. Button 2)////////|         |                       |
// | |///////////////////////|         |                       |
// | +-----------------------+         |                       | 190px
// | (3. Red Overlay - z-index: 999)   |                       |
// |                                   |                       v
// +-----------------------------------+
//
// Legend:
// - "/": Area covered by the "opaque" overlay where the elements
//        underneath are not reachable.
//
// Expectations:
// - Background and Overlay are generic containers/divs and are thus not
//   considered "actionable". They will not receive a Z-order in the output.
// - Button 1 and Button 2 are actionable elements.
// - Because both buttons possess valid geometry and interaction info, they will
//   both be processed and sorted relative to each other based on their visual
//   stacking. Button 1 receives Z-order 1, and Button 2 receives Z-order 2.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_RichExtraction_ZOrder_Overlap) {
  const std::string html = R"(
    <html>
      <body style="margin: 0; padding: 0;">
        <div style="width: 100vw; height: 100vh; position: absolute; top: 0; left: 0; background: white;"></div>
        <input type="button" id="btn1" value="Click Me" style="position: absolute; top: 10px; left: 10px; width: 100px; height: 50px;"/>
        <input type="button" id="btn2" value="Hidden" style="position: absolute; top: 100px; left: 10px; width: 100px; height: 50px;"/>
        <div id="overlay" style="position: absolute; top: 90px; left: 0; width: 200px; height: 100px; background: red; z-index: 999;">
          Overlay
        </div>
      </body>
    </html>
  )";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_GE(children->size(), 3u);

  const base::DictValue* btn1_node = &(*children)[0].GetDict();
  const base::DictValue* btn2_node = &(*children)[1].GetDict();

  const base::DictValue* interaction_info1 =
      btn1_node->FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(interaction_info1);
  EXPECT_EQ(interaction_info1->FindDouble("documentScopedZOrder"), 1.0);

  const base::DictValue* interaction_info2 =
      btn2_node->FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  ASSERT_TRUE(interaction_info2);
  EXPECT_EQ(interaction_info2->FindDouble("documentScopedZOrder"), 2.0);

  // Validate the overlay does not receive a Z-Order.
  const base::DictValue* overlay_node = &(*children)[2].GetDict();
  const base::DictValue* interaction_info3 = overlay_node->FindDictByDottedPath(
      "contentAttributes.nodeInteractionInfo");
  EXPECT_FALSE(interaction_info3 &&
               interaction_info3->FindDouble("documentScopedZOrder"));
}

TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_FormDisabledWithPollution) {
  // A form element containing an input with name="disabled".
  // Since HTMLFormElement has [LegacyOverrideBuiltIns], accessing form.disabled
  // returns the input element rather than undefined. Strictly checking
  // form.disabled === true prevents the form from being incorrectly marked as
  // disabled.
  const std::string html = R"(
    <html>
      <body>
        <form id="myForm">
          <input name="disabled" type="text" value="Not really disabled">
        </form>
      </body>
    </html>
  )";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/true,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_dict());

  const base::DictValue& dict = result_value->GetDict();
  const base::DictValue* root_node = dict.FindDict("rootNode");
  ASSERT_TRUE(root_node);
  const base::ListValue* children = root_node->FindList("childrenNodes");
  ASSERT_TRUE(children);
  ASSERT_GE(children->size(), 1u);

  // The form element should be the first node in this hierarchy.
  const base::DictValue& form_node = (*children)[0].GetDict();
  std::optional<double> attribute_type =
      form_node.FindDoubleByDottedPath("contentAttributes.attributeType");
  ASSERT_TRUE(attribute_type.has_value());
  ASSERT_EQ(
      static_cast<int>(attribute_type.value()),
      static_cast<int>(optimization_guide::proto::CONTENT_ATTRIBUTE_FORM));

  // Verify the form itself does NOT have isDisabled set to true.
  // Note: If the pollution bug occurred, `nodeInteractionInfo` would be
  // incorrectly populated with `isDisabled = true`. In the correct/fixed case,
  // `nodeInteractionInfo` is either not populated (null) or has `isDisabled =
  // false`.
  const base::DictValue* interaction_info =
      form_node.FindDictByDottedPath("contentAttributes.nodeInteractionInfo");
  if (interaction_info) {
    EXPECT_FALSE(interaction_info->FindBool("isDisabled").value_or(false));
  }
}

// Tests that page context extraction does not throw an error when the document
// has no documentElement.
TEST_P(PageContextExtractorJavaScriptFeatureTest,
       ExtractPageContext_NoDocumentElement) {
  const std::string html = "<html><body></body></html>";
  web::test::LoadHtml(base::SysUTF8ToNSString(html),
                      test_server_.GetURL(kMainPagePath), web_state());

  // Remove the documentElement.
  ASSERT_TRUE(web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(), @"document.documentElement.remove(); true;", feature()));

  std::optional<base::Value> result_value = RunExtraction(
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      /*include_cross_origin_frame_content=*/false,
      /*use_rich_extraction=*/true,
      /*use_rich_extraction_with_actionable=*/false,
      /*extract_paid_content=*/false,
      /*attempt_paid_content_json_fixing=*/false, "nonce", base::Seconds(1));

  // Should successfully return none/null, rather than throwing a JS error
  // (which returns std::nullopt).
  ASSERT_TRUE(result_value.has_value());
  EXPECT_TRUE(result_value->is_none());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageContextExtractorJavaScriptFeatureTest,
                         ::testing::Values(IPCExtractionMethod::kNative,
                                           IPCExtractionMethod::kJSON));
