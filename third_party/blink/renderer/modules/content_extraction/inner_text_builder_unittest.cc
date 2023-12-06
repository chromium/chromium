// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/inner_text_builder.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

TEST(InnerTextBuilderTest, Basic) {
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();
  // ScopedNullExecutionContext execution_context;
  ASSERT_TRUE(helper.LocalMainFrame());
  frame_test_helpers::LoadHTMLString(
      helper.LocalMainFrame(),
      "<body>container<iframe id='x'>iframe</iframe>after</body>",
      url_test_helpers::ToKURL("http://foobar.com"));
  auto iframe = helper.LocalMainFrame()->GetDocument().GetElementById("x");
  ASSERT_FALSE(iframe.IsNull());
  mojom::blink::InnerTextParams params;
  params.node_id = iframe.GetDomNodeId();
  auto frame =
      InnerTextBuilder::Build(*helper.LocalMainFrame()->GetFrame(), params);
  ASSERT_TRUE(frame);
  ASSERT_EQ(4u, frame->segments.size());
  ASSERT_TRUE(frame->segments[0]->is_text());
  EXPECT_EQ("container", frame->segments[0]->get_text());
  ASSERT_TRUE(frame->segments[1]->is_node_location());
  ASSERT_TRUE(frame->segments[2]->is_frame());
  const mojom::blink::InnerTextFramePtr& child_frame =
      frame->segments[2]->get_frame();
  HTMLIFrameElement* html_iframe =
      DynamicTo<HTMLIFrameElement>(iframe.Unwrap<Node>());
  EXPECT_EQ(html_iframe->contentDocument()->GetFrame()->GetLocalFrameToken(),
            child_frame->token);
  EXPECT_TRUE(frame->segments[3]->is_text());
  EXPECT_EQ("after", frame->segments[3]->get_text());
}

TEST(InnerTextBuilderTest, MultiFrames) {
  frame_test_helpers::WebViewHelper helper;
  std::string base_url("http://internal.test/");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "inner_text_test1.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "subframe-a.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "subframe-b.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "subframe-d.html");
  helper.InitializeAndLoad(base_url + "inner_text_test1.html");

  auto web_iframe_d =
      helper.LocalMainFrame()->GetDocument().GetElementById("d");
  ASSERT_FALSE(web_iframe_d.IsNull());
  HTMLIFrameElement* iframe_d =
      DynamicTo<HTMLIFrameElement>(web_iframe_d.Unwrap<Node>());
  ASSERT_TRUE(iframe_d);
  mojom::blink::InnerTextParams params;
  params.node_id = iframe_d->contentDocument()
                       ->getElementById(AtomicString("bold"))
                       ->GetDomNodeId();

  auto frame =
      InnerTextBuilder::Build(*helper.LocalMainFrame()->GetFrame(), params);
  ASSERT_TRUE(frame);

  ASSERT_EQ(7u, frame->segments.size());
  ASSERT_TRUE(frame->segments[0]->is_text());
  EXPECT_EQ("A", frame->segments[0]->get_text());
  ASSERT_TRUE(frame->segments[1]->is_frame());
  // NOTE: the nesting in this function is intended to indicate a child frame.
  // It is purely for readability.
  {
    const mojom::blink::InnerTextFramePtr& frame_1 =
        frame->segments[1]->get_frame();
    ASSERT_EQ(1u, frame_1->segments.size());
    ASSERT_TRUE(frame_1->segments[0]->is_text());
    EXPECT_EQ("a", frame_1->segments[0]->get_text());
  }
  ASSERT_TRUE(frame->segments[2]->is_text());
  EXPECT_EQ("B C", frame->segments[2]->get_text());
  ASSERT_TRUE(frame->segments[3]->is_frame());
  {
    const mojom::blink::InnerTextFramePtr& frame_2 =
        frame->segments[3]->get_frame();
    ASSERT_EQ(2u, frame_2->segments.size());
    EXPECT_EQ("b ", frame_2->segments[0]->get_text());
    ASSERT_TRUE(frame_2->segments[1]->is_frame());
    {
      const mojom::blink::InnerTextFramePtr& frame_2_1 =
          frame_2->segments[1]->get_frame();
      ASSERT_EQ(1u, frame_2_1->segments.size());
      ASSERT_TRUE(frame_2_1->segments[0]->is_text());
      EXPECT_EQ("a", frame_2_1->segments[0]->get_text());
    }
  }
  ASSERT_TRUE(frame->segments[4]->is_text());
  EXPECT_EQ("D E", frame->segments[4]->get_text());
  ASSERT_TRUE(frame->segments[5]->is_frame());
  {
    const mojom::blink::InnerTextFramePtr& frame_3 =
        frame->segments[5]->get_frame();
    ASSERT_EQ(3u, frame_3->segments.size());
    ASSERT_TRUE(frame_3->segments[0]->is_text());
    EXPECT_EQ("e", frame_3->segments[0]->get_text());
    EXPECT_TRUE(frame_3->segments[1]->is_node_location());
    ASSERT_TRUE(frame_3->segments[2]->is_text());
    EXPECT_EQ("hello", frame_3->segments[2]->get_text());
  }
  ASSERT_TRUE(frame->segments[6]->is_text());
  EXPECT_EQ("F", frame->segments[6]->get_text());
}

TEST(InnerTextBuilderTest, DifferentOrigin) {
  frame_test_helpers::WebViewHelper helper;
  std::string base_url("http://internal.test/");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "inner_text_test2.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8("http://different-host.com/"),
      test::CoreTestDataPath(), "subframe-a.html");
  helper.InitializeAndLoad(base_url + "inner_text_test2.html");

  auto frame =
      InnerTextBuilder::Build(*helper.LocalMainFrame()->GetFrame(), {});

  // The child frame should not be included as it's not same-origin.
  ASSERT_TRUE(frame);
  ASSERT_EQ(1u, frame->segments.size());
  ASSERT_TRUE(frame->segments[0]->is_text());
  EXPECT_EQ("XY", frame->segments[0]->get_text());
}

}  // namespace
}  // namespace blink
