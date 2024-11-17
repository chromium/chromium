// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_node.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_dom_event.h"
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

  void AddScript(String js) {
    GetDocument().GetSettings()->SetScriptEnabled(true);
    Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
    script->setInnerHTML(js);
    GetDocument().body()->AppendChild(script);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
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

TEST_F(WebNodeTest, CannotFindTextInElementThatIsNotAContainer) {
  SetInnerHTML(R"HTML(
    <div><br class="not-a-container"/> Hello world! </div>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".not-a-container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(element
                  .FindTextInElementWith("Hello world",
                                         [](const WebString&) { return true; })
                  .IsEmpty());
}

TEST_F(WebNodeTest, CannotFindTextNodesThatAreNotContainers) {
  SetInnerHTML(R"HTML(
    <div><br class="not-a-container"/> Hello world! </div>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".not-a-container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(element.FindAllTextNodesMatchingRegex(".*").empty());
}

TEST_F(WebNodeTest, CanFindTextInElementThatIsAContainer) {
  SetInnerHTML(R"HTML(
    <body class="container"><div> Hello world! </div></body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_EQ(WebString(" Hello world! "),
            element.FindTextInElementWith(
                "Hello world", [](const WebString&) { return true; }));
}

TEST_F(WebNodeTest, CanFindTextNodesThatAreContainers) {
  SetInnerHTML(R"HTML(
    <body class="container"><div id="id"> Hello world! </div></body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());

  WebVector<WebNode> nodes =
      element.FindAllTextNodesMatchingRegex("^ Hello world! $");
  ASSERT_EQ(nodes.size(), 1U);
  EXPECT_EQ(element.GetDocument().GetElementById("id").FirstChild(), nodes[0]);
}

TEST_F(WebNodeTest, CanFindCaseInsensitiveTextInElement) {
  SetInnerHTML(R"HTML(
    <body class="container"><div> HeLLo WoRLd! </div></body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_EQ(WebString(" HeLLo WoRLd! "),
            element.FindTextInElementWith(
                "hello world", [](const WebString&) { return true; }));
}

TEST_F(WebNodeTest, CannotFindTextInElementIfValidatorRejectsIt) {
  SetInnerHTML(R"HTML(
    <body class="container"><div> Hello world! </div></body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(element
                  .FindTextInElementWith("Hello world",
                                         [](const WebString&) { return false; })
                  .IsEmpty());
}

TEST_F(WebNodeTest, CannotFindTextNodesIfMatcherRejectsIt) {
  SetInnerHTML(R"HTML(
    <body class="container"><div> Hello world! </div></body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(element.FindAllTextNodesMatchingRegex("(?!.*)").empty());
}

TEST_F(WebNodeTest, CanFindTextInReadonlyTextInputElement) {
  SetInnerHTML(R"HTML(
    <body class="container">
      <input type="text" readonly="" value=" HeLLo WoRLd! ">
    </body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_EQ(WebString(" HeLLo WoRLd! "),
            element.FindTextInElementWith(
                "hello world", [](const WebString&) { return true; }));
}

TEST_F(WebNodeTest, CannotFindTextInNonTextInputElement) {
  SetInnerHTML(R"HTML(
    <body class="container">
      <input type="url" readonly="" value=" HeLLo WoRLd! ">
    </body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(element
                  .FindTextInElementWith("hello world",
                                         [](const WebString&) { return true; })
                  .IsEmpty());
}

TEST_F(WebNodeTest, CannotFindTextNodesInNonTextInputElement) {
  SetInnerHTML(R"HTML(
    <body class="container">
      <input type="url" readonly="" value=" HeLLo WoRLd! ">
    </body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(
      element.FindAllTextNodesMatchingRegex("^ HeLLo WoRLd! $").empty());
}

TEST_F(WebNodeTest, CannotFindTextInNonReadonlyTextInputElement) {
  SetInnerHTML(R"HTML(
    <body class="container">
      <input type="text" value=" HeLLo WoRLd! ">
    </body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(element
                  .FindTextInElementWith("hello world",
                                         [](const WebString&) { return true; })
                  .IsEmpty());
}

TEST_F(WebNodeTest, CannotFindTextNodesInNonReadonlyTextInputElement) {
  SetInnerHTML(R"HTML(
    <body class="container">
      <input type="text" value=" HeLLo WoRLd! ">
    </body>
  )HTML");
  WebElement element = Root().QuerySelector(AtomicString(".container"));

  EXPECT_FALSE(element.IsNull());
  EXPECT_TRUE(
      element.FindAllTextNodesMatchingRegex("^ HeLLo WoRLd! $").empty());
}

// Tests that AddEventListener() registers and deregisters a listener.
TEST_F(WebNodeTest, AddEventListener) {
  testing::MockFunction<void(std::string_view)> checkpoint;
  base::MockRepeatingCallback<void(blink::WebDOMEvent)> handler;
  {
    testing::InSequence seq;
    EXPECT_CALL(checkpoint, Call("focus"));
    EXPECT_CALL(checkpoint, Call("set_caret 1"));
    EXPECT_CALL(handler, Run);
    EXPECT_CALL(checkpoint, Call("set_caret 2"));
    EXPECT_CALL(handler, Run);
    EXPECT_CALL(checkpoint, Call("set_caret 3"));
  }

  SetInnerHTML("<textarea id=field>0123456789</textarea>");

  // Focuses the textarea.
  auto focus = [&]() {
    checkpoint.Call("focus");
    AddScript(String("document.getElementById('field').focus()"));
    task_environment().RunUntilIdle();
  };

  // Moves the caret in the field and fires a selectionchange event.
  auto set_caret = [&](int caret_position) {
    checkpoint.Call(base::StringPrintf("set_caret %d", caret_position));
    AddScript(String(base::StringPrintf(
        "document.getElementById('field').setSelectionRange(%d, %d)",
        caret_position, caret_position)));
    task_environment().RunUntilIdle();
  };

  focus();
  {
    auto remove_listener = Root().AddEventListener(
        WebNode::EventType::kSelectionchange, handler.Get());
    set_caret(1);
    set_caret(2);
    // The listener is removed by `remove_listener`'s destructor.
  }
  set_caret(3);
}

}  // namespace blink
