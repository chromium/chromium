// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_element.h"

#include <memory>
#include <vector>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_label_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class WebElementTest : public PageTestBase {
 protected:
  void InsertHTML(String html);
  WebElement TestElement();
};

void WebElementTest::InsertHTML(String html) {
  GetDocument().documentElement()->setInnerHTML(html);
}

WebElement WebElementTest::TestElement() {
  Element* element = GetDocument().getElementById("testElement");
  DCHECK(element);
  return WebElement(element);
}

TEST_F(WebElementTest, IsEditable) {
  InsertHTML("<div id=testElement></div>");
  EXPECT_FALSE(TestElement().IsEditable());

  InsertHTML("<div id=testElement contenteditable=true></div>");
  EXPECT_TRUE(TestElement().IsEditable());

  InsertHTML(R"HTML(
    <div style='-webkit-user-modify: read-write'>
      <div id=testElement></div>
    </div>
  )HTML");
  EXPECT_TRUE(TestElement().IsEditable());

  InsertHTML(R"HTML(
    <div style='-webkit-user-modify: read-write'>
      <div id=testElement style='-webkit-user-modify: read-only'></div>
    </div>
  )HTML");
  EXPECT_FALSE(TestElement().IsEditable());

  InsertHTML("<input id=testElement>");
  EXPECT_TRUE(TestElement().IsEditable());

  InsertHTML("<input id=testElement readonly>");
  EXPECT_FALSE(TestElement().IsEditable());

  InsertHTML("<input id=testElement disabled>");
  EXPECT_FALSE(TestElement().IsEditable());

  InsertHTML("<fieldset disabled><div><input id=testElement></div></fieldset>");
  EXPECT_FALSE(TestElement().IsEditable());
}

TEST_F(WebElementTest, IsAutonomousCustomElement) {
  InsertHTML("<x-undefined id=testElement></x-undefined>");
  EXPECT_FALSE(TestElement().IsAutonomousCustomElement());
  InsertHTML("<div id=testElement></div>");
  EXPECT_FALSE(TestElement().IsAutonomousCustomElement());

  GetDocument().GetSettings()->SetScriptEnabled(true);
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    customElements.define('v1-custom', class extends HTMLElement {});
    document.body.appendChild(document.createElement('v1-custom'));
    customElements.define('v1-builtin',
                          class extends HTMLButtonElement {},
                          { extends:'button' });
    document.body.appendChild(
        document.createElement('button', { is: 'v1-builtin' }));
  )JS");
  GetDocument().body()->appendChild(script);
  auto* v1builtin = GetDocument().body()->lastChild();
  EXPECT_FALSE(WebElement(To<Element>(v1builtin)).IsAutonomousCustomElement());
  auto* v1autonomous = v1builtin->previousSibling();
  EXPECT_TRUE(
      WebElement(To<Element>(v1autonomous)).IsAutonomousCustomElement());
}

TEST_F(WebElementTest, ShadowRoot) {
  InsertHTML("<input id=testElement>");
  EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
      << "ShadowRoot() should not return a UA ShadowRoot.";

  {
    InsertHTML("<span id=testElement></span>");
    EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
        << "No ShadowRoot initially.";
    auto* element = GetDocument().getElementById("testElement");
    element->AttachShadowRootInternal(ShadowRootType::kOpen);
    EXPECT_FALSE(TestElement().ShadowRoot().IsNull())
        << "Should return V1 open ShadowRoot.";
  }

  {
    InsertHTML("<p id=testElement></p>");
    EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
        << "No ShadowRoot initially.";
    auto* element = GetDocument().getElementById("testElement");
    element->AttachShadowRootInternal(ShadowRootType::kClosed);
    EXPECT_FALSE(TestElement().ShadowRoot().IsNull())
        << "Should return V1 closed ShadowRoot.";
  }
}

TEST_F(WebElementTest, ComputedStyleProperties) {
  InsertHTML(R"HTML(
    <body>
      <div id=testElement></div>
    </body>
  )HTML");

  WebElement element = TestElement();
  element.GetDocument().InsertStyleSheet(
      "body { font-size: 16px; text-decoration: underline; color: blue;}");
  // font-size
  {
    EXPECT_EQ(element.GetComputedValue("font-size"), "16px");
    element.SetAttribute("style", "font-size: 3em");
    EXPECT_EQ(element.GetComputedValue("font-size"), "48px");
  }

  // text-decoration
  {
    EXPECT_EQ(element.GetComputedValue("text-decoration"),
              "none solid rgb(0, 0, 255)");
    element.SetAttribute("style", "text-decoration: line-through");
    EXPECT_EQ(element.GetComputedValue("text-decoration-line"), "line-through");
    EXPECT_EQ(element.GetComputedValue("-Webkit-text-decorations-in-effect"),
              "underline line-through");
  }

  // font-weight
  {
    EXPECT_EQ(element.GetComputedValue("font-weight"), "400");
    element.SetAttribute("style", "font-weight: bold");
    EXPECT_EQ(element.GetComputedValue("font-weight"), "700");
  }

  // color
  {
    EXPECT_EQ(element.GetComputedValue("color"), "rgb(0, 0, 255)");
    element.SetAttribute("style", "color: red");
    EXPECT_EQ(element.GetComputedValue("color"), "rgb(255, 0, 0)");
  }
}

TEST_F(WebElementTest, Labels) {
  auto ExpectLabelIdsEqual = [&](const std::vector<WebString>& expected_ids) {
    std::vector<WebString> label_ids;
    for (const WebLabelElement& label : TestElement().Labels())
      label_ids.push_back(label.GetIdAttribute());
    EXPECT_THAT(label_ids, ::testing::UnorderedElementsAreArray(expected_ids));
  };

  // No label.
  InsertHTML("<input id=testElement>");
  ExpectLabelIdsEqual({});

  // A single label.
  InsertHTML(R"HTML(
    <label id=testLabel for=testElement>Label</label>
    <input id=testElement>
  )HTML");
  ExpectLabelIdsEqual({"testLabel"});

  // Multiple labels.
  InsertHTML(R"HTML(
    <label id=testLabel1 for=testElement>Label 1</label>
    <label id=testLabel2 for=testElement>Label 2</label>
    <input id=testElement>
  )HTML");
  ExpectLabelIdsEqual({"testLabel1", "testLabel2"});
}

}  // namespace blink
