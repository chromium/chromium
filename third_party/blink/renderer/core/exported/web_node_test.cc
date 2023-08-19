// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_node.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class WebNodeTest : public PageTestBase {
 protected:
  void SetInnerHTML(const String& html) {
    GetDocument().documentElement()->setInnerHTML(html);
  }

  WebNode Root() { return WebNode(GetDocument().documentElement()); }
};

TEST_F(WebNodeTest, QuerySelectorMatches) {
  SetInnerHTML("<div id=x><span class=a></span></div>");
  WebElement element = Root().QuerySelector(AtomicString(".a"));
  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(element.HasHTMLTagName("span"));
}

TEST_F(WebNodeTest, QuerySelectorDoesNotMatch) {
  SetInnerHTML("<div id=x><span class=a></span></div>");
  WebElement element = Root().QuerySelector(AtomicString("section"));
  EXPECT_TRUE(element.IsNull());
}

TEST_F(WebNodeTest, QuerySelectorError) {
  SetInnerHTML("<div></div>");
  WebElement element = Root().QuerySelector(AtomicString("@invalid-selector"));
  EXPECT_TRUE(element.IsNull());
}

TEST_F(WebNodeTest, GetElementsByHTMLTagName) {
  SetInnerHTML(
      "<body><LABEL></LABEL><svg "
      "xmlns='http://www.w3.org/2000/svg'><label></label></svg></body>");
  // WebNode::getElementsByHTMLTagName returns only HTML elements.
  WebElementCollection collection = Root().GetElementsByHTMLTagName("label");
  EXPECT_EQ(1u, collection.length());
  EXPECT_TRUE(collection.FirstItem().HasHTMLTagName("label"));
  // The argument should be lower-case.
  collection = Root().GetElementsByHTMLTagName("LABEL");
  EXPECT_EQ(0u, collection.length());
}

class WebNodeSimTest : public SimTest {};

TEST_F(WebNodeSimTest, IsFocused) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  LoadURL("https://example.com/test.html");
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  main_resource.Write(R"HTML(
    <!DOCTYPE html>
    <link rel=stylesheet href=style.css>
    <input id=focusable>
  )HTML");

  css_resource.Start();

  WebNode input_node(GetDocument().getElementById(AtomicString("focusable")));
  EXPECT_FALSE(input_node.IsFocusable());
  EXPECT_FALSE(GetDocument().HaveRenderBlockingStylesheetsLoaded());

  main_resource.Finish();
  css_resource.Complete("dummy {}");
  test::RunPendingTasks();
  EXPECT_TRUE(input_node.IsFocusable());
}

}  // namespace blink
