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
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class WebElementTest : public PageTestBase {
 protected:
  void InsertHTML(String html);
  void AddScript(String script);
  WebElement TestElement();
};

void WebElementTest::InsertHTML(String html) {
  GetDocument().documentElement()->setInnerHTML(html);
}

void WebElementTest::AddScript(String js) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(js);
  GetDocument().body()->AppendChild(script);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
}

WebElement WebElementTest::TestElement() {
  Element* element = GetDocument().getElementById(AtomicString("testElement"));
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

// Tests SelectedText() and ContainsFrameSelection() with divs, including a
// contenteditable.
TEST_F(WebElementTest, SelectedTextOfContentEditable) {
  InsertHTML(
      R"(<div>Foo</div>
         <div id=testElement contenteditable>Some <b>rich text</b> here.</div>
         <div>Bar</div>)");
  auto* element = GetDocument().getElementById(AtomicString("testElement"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Selection().SelectSubString(*element, 2, 15);
  ASSERT_EQ(Selection().SelectedText(), String("me rich text he"));
  EXPECT_TRUE(TestElement().ContainsFrameSelection());
  EXPECT_EQ(TestElement().SelectedText().Utf8(), "me rich text he");

  Selection().SelectSubString(*element, 10, 7);
  ASSERT_EQ(Selection().SelectedText(), String("text he"));
  EXPECT_TRUE(TestElement().ContainsFrameSelection());
  EXPECT_EQ(TestElement().SelectedText().Utf8(), "text he");

  Selection().SelectSubString(*element->firstElementChild(), 0, 9);
  ASSERT_EQ(Selection().SelectedText(), String("rich text"));
  EXPECT_TRUE(TestElement().ContainsFrameSelection());
  EXPECT_EQ(TestElement().SelectedText().Utf8(), "rich text");

  Selection().SelectSubString(*element->parentElement(), 0, 8);
  ASSERT_EQ(Selection().SelectedText(), String("Foo\nSome"));
  EXPECT_FALSE(TestElement().ContainsFrameSelection());
  EXPECT_EQ(TestElement().SelectedText().Utf8(), "");

  Selection().SelectSubString(*element->parentElement(), 19, 9);
  ASSERT_EQ(Selection().SelectedText(), String("here.\nBar"));
  EXPECT_TRUE(TestElement().ContainsFrameSelection());
  // This is not ideal behavior: it'd be preferable if SelectedText() truncated
  // the selection at the end of `TestElement()`.
  EXPECT_EQ(TestElement().SelectedText().Utf8(), "here.\nBar");
}

// Tests SelectedText() and ContainsFrameSelection() with a textarea.
TEST_F(WebElementTest, SelectedTextOfTextArea) {
  InsertHTML(
      R"(<div>Foo</div>
         <textarea id=testElement>Some plain text here.</textarea>
         <div>Bar</div>)");
  auto* element = blink::To<HTMLTextAreaElement>(
      GetDocument().getElementById(AtomicString("testElement")));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  element->Focus();

  element->SetSelectionRange(2, 18);
  EXPECT_TRUE(TestElement().ContainsFrameSelection());
  EXPECT_EQ(TestElement().SelectedText().Utf8(), "me plain text he");

  element->SetSelectionRange(11, 18);
  EXPECT_TRUE(TestElement().ContainsFrameSelection());
  EXPECT_EQ(TestElement().SelectedText().Utf8(), "text he");

  element->SetSelectionRange(5, 15);
  EXPECT_TRUE(TestElement().ContainsFrameSelection());
  EXPECT_EQ(TestElement().SelectedText().Utf8(), "plain text");
}

// Tests SelectedText() and ContainsFrameSelection() with a document
// with no root, on a form control element.
TEST_F(WebElementTest, SelectedTextEmptyDocument) {
  InsertHTML(R"(<input type=text id=testElement></div>)");
  WebElement test_element = TestElement();
  GetDocument().documentElement()->remove();

  EXPECT_FALSE(test_element.ContainsFrameSelection());
  EXPECT_EQ(test_element.SelectedText().Utf8(), "");
}

// Tests SelectText() with a textarea.
TEST_F(WebElementTest, SelectTextOfTextArea) {
  InsertHTML(
      R"(<div>Foo</div>
      <textarea id=testElement>Some plain text here.</textarea>
      <div>Bar</div>)");

  TestElement().SelectText(/*select_all=*/false);
  EXPECT_EQ(Selection().SelectedText(), "");

  TestElement().SelectText(/*select_all=*/true);
  EXPECT_EQ(Selection().SelectedText(), "Some plain text here.");
}

// Tests SelectText() with a contenteditable.
TEST_F(WebElementTest, SelectTextOfContentEditable) {
  InsertHTML(
      R"(<div>Foo</div>
      <div id=testElement contenteditable>Some <b>rich text</b> here.</div>
      <textarea>Some plain text here.</textarea>)");

  TestElement().SelectText(/*select_all=*/false);
  EXPECT_EQ(Selection().SelectedText(), "");

  TestElement().SelectText(/*select_all=*/true);
  EXPECT_EQ(Selection().SelectedText(), "Some rich text here.");
}

TEST_F(WebElementTest, PasteTextIntoContentEditable) {
  InsertHTML(
      "<div id=testElement contenteditable>Some <b>rich text</b> here.</div>"
      "<textarea>Some plain text here.</textarea>");
  auto* element = GetDocument().getElementById(AtomicString("testElement"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Selection().SelectSubString(*element->firstElementChild(), 0, 9);
  ASSERT_EQ(Selection().SelectedText(), String("rich text"));
  // Paste and replace selection.
  TestElement().PasteText("fancy text", /*replace_all=*/false);
  EXPECT_EQ(element->innerHTML(), "Some <b>fancy text</b>&nbsp;here.");
  // Paste and replace all.
  TestElement().PasteText("Hello", /*replace_all=*/true);
  EXPECT_EQ(element->innerHTML(), "Hello");
  // Paste into an unfocused element.
  element->nextElementSibling()->Focus();
  TestElement().PasteText("world", /*replace_all=*/false);
  EXPECT_EQ(element->innerHTML(), "Hello&nbsp;world");
}

TEST_F(WebElementTest, PasteTextIntoTextArea) {
  InsertHTML(
      "<div contenteditable>Some <b>rich text</b> here.</div>"
      "<textarea id=testElement>Some plain text here.</textarea>");
  auto* element = blink::To<HTMLTextAreaElement>(
      GetDocument().getElementById(AtomicString("testElement")));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  element->Focus();
  element->setSelectionStart(5);
  element->setSelectionEnd(15);
  ASSERT_EQ(element->Value().Substring(
                element->selectionStart(),
                element->selectionEnd() - element->selectionStart()),
            String("plain text"));
  // Paste and replace selection.
  TestElement().PasteText("boring text", /*replace_all=*/false);
  EXPECT_EQ(element->Value(), "Some boring text here.");
  // Paste and replace all.
  TestElement().PasteText("Hello", /*replace_all=*/true);
  EXPECT_EQ(element->Value(), "Hello");
  // Paste into an unfocused element.
  element->previousElementSibling()->Focus();
  TestElement().PasteText("world", /*replace_all=*/false);
  EXPECT_EQ(element->Value(), "Hello world");
}

// Tests that PasteText() aborts when the JavaScript handler of the 'paste'
// event prevents the default handling.
TEST_F(WebElementTest, PasteTextIsNoOpWhenPasteIsCancelled) {
  InsertHTML(
      "<div id=testElement contenteditable>Some <b>rich text</b> here.</div>");
  AddScript(R"(
      document.getElementById('testElement').addEventListener('paste', e => {
        e.target.textContent = 'UPPERCASE TEXT';
        e.preventDefault();
      }))");
  auto* element = GetDocument().getElementById(AtomicString("testElement"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Selection().SelectSubString(*element->firstElementChild(), 0, 9);
  ASSERT_EQ(Selection().SelectedText(), String("rich text"));
  // Paste and replace selection.
  TestElement().PasteText("fancy text", /*replace_all=*/false);
  EXPECT_EQ(element->innerHTML(), "Some <b>UPPERCASE TEXT</b> here.");
}

// Tests that PasteText() aborts when the JavaScript handler of the
// 'beforeinput' event prevents the default handling.
TEST_F(WebElementTest, PasteTextIsNoOpWhenBeforeInputIsCancelled) {
  InsertHTML(
      "<div id=testElement contenteditable>Some <b>rich text</b> here.</div>");
  AddScript(R"(
      document.getElementById('testElement').addEventListener('beforeinput',
                                                              e => {
        e.target.textContent = 'UPPERCASE TEXT';
        e.preventDefault();
      }))");
  auto* element = GetDocument().getElementById(AtomicString("testElement"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Selection().SelectSubString(*element->firstElementChild(), 0, 9);
  ASSERT_EQ(Selection().SelectedText(), String("rich text"));
  // Paste and replace selection.
  TestElement().PasteText("fancy text", /*replace_all=*/false);
  EXPECT_EQ(element->innerHTML(), "Some <b>UPPERCASE TEXT</b> here.");
}

TEST_F(WebElementTest, ShadowRoot) {
  InsertHTML("<input id=testElement>");
  EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
      << "ShadowRoot() should not return a UA ShadowRoot.";

  {
    InsertHTML("<span id=testElement></span>");
    EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
        << "No ShadowRoot initially.";
    auto* element = GetDocument().getElementById(AtomicString("testElement"));
    element->AttachShadowRootForTesting(ShadowRootMode::kOpen);
    EXPECT_FALSE(TestElement().ShadowRoot().IsNull())
        << "Should return V1 open ShadowRoot.";
  }

  {
    InsertHTML("<p id=testElement></p>");
    EXPECT_TRUE(TestElement().ShadowRoot().IsNull())
        << "No ShadowRoot initially.";
    auto* element = GetDocument().getElementById(AtomicString("testElement"));
    element->AttachShadowRootForTesting(ShadowRootMode::kClosed);
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
