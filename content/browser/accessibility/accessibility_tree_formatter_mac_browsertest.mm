// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/data_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/accessibility/platform/inspect/ax_script_instruction.h"
#include "url/gurl.h"

namespace content {

namespace {

const ui::AXPropertyFilter::Type ALLOW_EMPTY =
    ui::AXPropertyFilter::ALLOW_EMPTY;

class AccessibilityTreeFormatterMacBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityTreeFormatterMacBrowserTest() {}
  ~AccessibilityTreeFormatterMacBrowserTest() override {}

  // Checks the formatted accessible tree for the given data URL.
  void TestFormat(const char* url,
                  const std::vector<ui::AXPropertyFilter>& property_filters,
                  const std::vector<ui::AXNodeFilter>& node_filters,
                  const char* expected) const;

  void TestFormat(const char* url,
                  const std::vector<const char*>& filters,
                  const char* expected) const;

  void TestScript(const char* url,
                  const std::vector<const char*>& scripts,
                  const char* expected) const;

  // Tests wrong parameters for an attribute in a single run
  void TestWrongParameters(const char* url,
                           const std::vector<const char*>& parameters,
                           const char* filter_pattern,
                           const char* expected_pattern) const;

 protected:
  BrowserAccessibilityManager* GetManager() const {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }
};

void AccessibilityTreeFormatterMacBrowserTest::TestFormat(
    const char* url,
    const std::vector<ui::AXPropertyFilter>& property_filters,
    const std::vector<ui::AXNodeFilter>& node_filters,
    const char* expected) const {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  ASSERT_TRUE(NavigateToURL(shell(), GURL(url)));
  waiter.WaitForNotification();

  std::unique_ptr<ui::AXTreeFormatter> formatter =
      AXInspectFactory::CreatePlatformFormatter();

  formatter->SetPropertyFilters(property_filters,
                                ui::AXTreeFormatter::kFiltersDefaultSet);
  formatter->SetNodeFilters(node_filters);

  BrowserAccessibility* root = GetManager()->GetRoot();
  ASSERT_NE(nullptr, root);

  std::string actual = formatter->Format(root);
  EXPECT_EQ(actual, expected);
}

void AccessibilityTreeFormatterMacBrowserTest::TestFormat(
    const char* url,
    const std::vector<const char*>& filters,
    const char* expected) const {
  std::vector<ui::AXPropertyFilter> property_filters;
  for (const char* filter : filters) {
    property_filters.emplace_back(filter, ALLOW_EMPTY);
  }
  TestFormat(url, property_filters, {}, expected);
}

void AccessibilityTreeFormatterMacBrowserTest::TestScript(
    const char* url,
    const std::vector<const char*>& scripts,
    const char* expected) const {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  ASSERT_TRUE(NavigateToURL(shell(), GURL(url)));
  waiter.WaitForNotification();

  std::unique_ptr<ui::AXTreeFormatter> formatter =
      AXInspectFactory::CreatePlatformFormatter();

  BrowserAccessibility* root = GetManager()->GetRoot();
  ASSERT_NE(nullptr, root);

  std::vector<ui::AXScriptInstruction> instructions;
  for (const char* script : scripts) {
    instructions.emplace_back(script);
  }

  std::string actual =
      formatter->EvaluateScript(root, instructions, 0, instructions.size());
  EXPECT_EQ(actual, expected);
}

void AccessibilityTreeFormatterMacBrowserTest::TestWrongParameters(
    const char* url,
    const std::vector<const char*>& parameters,
    const char* filter_pattern,
    const char* expected_pattern) const {
  std::string placeholder("Argument");
  size_t expected_pos = std::string(expected_pattern).find(placeholder);
  ASSERT_FALSE(expected_pos == std::string::npos);

  size_t filter_pos = std::string(filter_pattern).find(placeholder);
  ASSERT_FALSE(filter_pos == std::string::npos);

  for (const char* parameter : parameters) {
    std::string expected(expected_pattern);
    expected.replace(expected_pos, placeholder.length(), parameter);

    std::string filter(filter_pattern);
    filter.replace(filter_pos, placeholder.length(), parameter);

    TestFormat(url, {filter.c_str()}, expected.c_str());
  }
}

}  // namespace

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       DefaultAttributes) {
  TestFormat(R"~~(data:text/html,
                    <input aria-label='input'>)~~",
             {},
             R"~~(AXWebArea
++AXGroup
++++AXTextField AXDescription='input'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Filters_NoWildcardProperty) {
  TestFormat(R"~~(data:text/html,
                    <input class='classolasso'>)~~",
             {"AXDOMClassList"},
             R"~~(AXWebArea AXDOMClassList=[]
++AXGroup AXDOMClassList=[]
++++AXTextField AXDOMClassList=['classolasso']
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Filters_LineIndex) {
  TestFormat(R"~~(data:text/html,
                    <input class='input_at_3rd_line'>
                    <input class='input_at_4th_line'>
                    <input class='input_at_5th_line'>)~~",
             {":3,:5;AXDOMClassList=*"}, R"~~(AXWebArea
++AXGroup
++++AXTextField AXDOMClassList=['input_at_3rd_line']
++++AXTextField
++++AXTextField AXDOMClassList=['input_at_5th_line']
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Serialize_AXTextMarker) {
  TestFormat(R"~~(data:text/html,
                    <p>Paragraph</p>)~~",
             {":3;AXStartTextMarker=*"}, R"~~(AXWebArea
++AXGroup
++++AXStaticText AXStartTextMarker={:1, 0, down} AXValue='Paragraph'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Serialize_AXTextMarkerRange) {
  TestFormat(R"~~(data:text/html,
                    <p id='p'>Paragraph</p>
                    <script>
                      window.getSelection().selectAllChildren(document.getElementById('p'));
                    </script>)~~",
             {":3;AXSelectedTextMarkerRange=*"}, R"~~(AXWebArea
++AXGroup
++++AXStaticText AXSelectedTextMarkerRange={anchor: {:2, -1, down}, focus: {:3, 0, down}} AXValue='Paragraph'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Serialize_NSRange) {
  TestFormat(R"~~(data:text/html,<input id='input' value='alphabet'>
                    <script>
                      let input = document.getElementById('input');
                      input.select();
                    </script>)~~",
             {":3;AXSelectedTextRange=*"}, R"~~(AXWebArea
++AXGroup
++++AXTextField AXSelectedTextRange={loc: 0, len: 8} AXValue='alphabet'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_Int) {
  TestFormat(R"~~(data:text/html,
                    <p contentEditable='true'>Text</p>)~~",
             {":2;AXLineForIndex(0)=*"}, R"~~(AXWebArea
++AXTextArea AXLineForIndex(0)=0 AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_Int_WrongParameters) {
  TestWrongParameters(R"~~(data:text/html,
                           <p contentEditable='true'>Text</p>)~~",
                      {"1, 2", "NaN"}, ":2;AXLineForIndex(Argument)=*",
                      R"~~(AXWebArea
++AXTextArea AXLineForIndex(Argument)=ERROR:FAILED_TO_PARSE AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_IntArray) {
  TestFormat(R"~~(data:text/html,
                    <table role="grid"><tr><td>CELL</td></tr></table>)~~",
             {"AXCellForColumnAndRow([0, 0])=*"}, R"~~(AXWebArea
++AXTable AXCellForColumnAndRow([0, 0])=:4
++++AXRow
++++++AXCell
++++++++AXStaticText AXValue='CELL'
++++AXColumn
++++++AXCell
++++++++AXStaticText AXValue='CELL'
++++AXGroup
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_IntArray_NilValue) {
  TestFormat(R"~~(data:text/html,
                    <table role="grid"></table>)~~",
             {"AXCellForColumnAndRow([0, 0])=*"}, R"~~(AXWebArea
++AXTable AXCellForColumnAndRow([0, 0])=NULL
++++AXGroup
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_IntArray_WrongParameters) {
  TestWrongParameters(R"~~(data:text/html,
                           <table role="grid"><tr><td>CELL</td></tr></table>)~~",
                      {"0, 0", "{1, 2}", "[1, NaN]", "[NaN, 1]"},
                      "AXCellForColumnAndRow(Argument)=*", R"~~(AXWebArea
++AXTable AXCellForColumnAndRow(Argument)=ERROR:FAILED_TO_PARSE
++++AXRow
++++++AXCell
++++++++AXStaticText AXValue='CELL'
++++AXColumn
++++++AXCell
++++++++AXStaticText AXValue='CELL'
++++AXGroup
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_TextMarkerArray) {
  TestScript(
      R"~~(data:text/html,
                    <textbox id="textbox">Text</textbox>)~~",
      {"text_range:= textbox.AXTextMarkerRangeForUIElement(textbox)",
       "textbox.AXTextMarkerRangeForUnorderedTextMarkers([text_range."
       "anchor, text_range.focus])"},
      R"~~(text_range={anchor: {:3, 0, down}, focus: {:3, 4, down}}
textbox.AXTextMarkerRangeForUnorderedTextMarkers([text_range.anchor, text_range.focus])={anchor: {:3, 0, down}, focus: {:3, 4, down}}
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_NSRange) {
  TestFormat(R"~~(data:text/html,
                    <p contentEditable='true'>Text</p>)~~",
             {":2;AXStringForRange({loc: 1, len: 2})=*"}, R"~~(AXWebArea
++AXTextArea AXStringForRange({loc: 1, len: 2})='ex' AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_NSRange_WrongParameters) {
  TestWrongParameters(R"~~(data:text/html,
                           <p contentEditable='true'>Text</p>)~~",
                      {"1, 2", "[]", "{loc: 1, leno: 2}", "{loco: 1, len: 2}",
                       "{loc: NaN, len: 2}", "{loc: 2, len: NaN}"},
                      ":2;AXStringForRange(Argument)=*", R"~~(AXWebArea
++AXTextArea AXStringForRange(Argument)=ERROR:FAILED_TO_PARSE AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_UIElement) {
  TestFormat(R"~~(data:text/html,
                    <p contentEditable='true'>Text</p>)~~",
             {":2;AXIndexForChildUIElement(:3)=*"}, R"~~(AXWebArea
++AXTextArea AXIndexForChildUIElement(:3)=0 AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_UIElement_WrongParameters) {
  TestWrongParameters(R"~~(data:text/html,
                           <p contentEditable='true'>Text</p>)~~",
                      {"1, 2", "2", ":4"},
                      ":2;AXIndexForChildUIElement(Argument)=*",
                      R"~~(AXWebArea
++AXTextArea AXIndexForChildUIElement(Argument)=ERROR:FAILED_TO_PARSE AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_TextMarker) {
  TestFormat(R"~~(data:text/html,
                    <p>Text</p>)~~",
             {":1;AXIndexForTextMarker({:2, 1, down})=*"},
             R"~~(AXWebArea AXIndexForTextMarker({:2, 1, down})=1
++AXGroup
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_TextMarker_WrongParameters) {
  TestWrongParameters(
      R"~~(data:text/html,
                           <p>Text</p>)~~",
      {"1, 2", "2", "{2, 1, down}", "{:2, NaN, down}", "{:2, 1, hoho}"},
      ":1;AXIndexForTextMarker(Argument)=*",
      R"~~(AXWebArea AXIndexForTextMarker(Argument)=ERROR:FAILED_TO_PARSE
++AXGroup
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_TextMarkerRange) {
  TestFormat(R"~~(data:text/html,
                    <p>Text</p>)~~",
             {":2;AXStringForTextMarkerRange({anchor: {:2, 1, down}, focus: "
              "{:2, 3, down}})=*"},
             R"~~(AXWebArea
++AXGroup AXStringForTextMarkerRange({anchor: {:2, 1, down}, focus: {:2, 3, down}})='ex'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(
    AccessibilityTreeFormatterMacBrowserTest,
    ParameterizedAttributes_TextMarkerRange_WrongParameters) {
  TestWrongParameters(
      R"~~(data:text/html,
                           <p>Text</p>)~~",
      {"1, 2", "2", "{focus: {:2, 1, down}}", "{anchor: {:2, 1, down}}",
       "{anchor: {2, 1, down}, focus: {2, 1, down}}"},
      ":1;AXStringForTextMarkerRange(Argument)=*",
      R"~~(AXWebArea AXStringForTextMarkerRange(Argument)=ERROR:FAILED_TO_PARSE
++AXGroup
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest, NestedCalls) {
  TestFormat(R"~~(data:text/html,
                    <p>Text</p>)~~",
             {":1;AXIndexForTextMarker(AXTextMarkerForIndex(0))"},
             R"~~(AXWebArea AXIndexForTextMarker(AXTextMarkerForIndex(0))=0
++AXGroup
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest, Script) {
  TestScript(R"~~(data:text/html,
                    <input aria-label='input'>)~~",
             {":3.AXRole"},
             R"~~(:3.AXRole='AXTextField'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Document) {
  TestScript(R"~~(data:text/html,
                    <input id='textbox' aria-label='input'>)~~",
             {"document.AXRole"},
             R"~~(document.AXRole='AXWebArea'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_ByDOMId) {
  TestScript(R"~~(data:text/html,
                    <input id='textbox' aria-label='input'>)~~",
             {"textbox.AXRole"},
             R"~~(textbox.AXRole='AXTextField'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_ByDOMId_WrongDOMId) {
  TestScript(R"~~(data:text/html,
                    <input id='textbox' aria-label='input'>)~~",
             {"textbo.AXRole"},
             R"~~(textbo.AXRole=ERROR:FAILED_TO_PARSE
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_UnrecognizedAttribute) {
  TestScript(R"~~(data:text/html,
                    <input id='textbox' aria-label='input'>)~~",
             {"textbox.AXRolio"},
             R"~~(textbox.AXRolio=ERROR:FAILED_TO_PARSE
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_NotApplicableAttribute) {
  TestScript(R"~~(data:text/html,
                    <input id='textbox'>)~~",
             {"textbox.AXARIABusy"},
             R"~~(textbox.AXARIABusy=n/a
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_NullValue) {
  TestScript(R"~~(data:text/html,
                    <input id='input'>)~~",
             {"input.AXTitleUIElement"},
             R"~~(input.AXTitleUIElement=NULL
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Comment) {
  TestScript(R"~~(data:text/html,
                    <input id='textbox' aria-label='input'>)~~",
             {"// textbox.AXRolio"},
             R"~~(// textbox.AXRolio
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Object_IntArray) {
  TestScript("data:text/html,", {"var:= [3, 4]"},
             R"~~(var=[3, 4]
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Object_NSRange) {
  TestScript("data:text/html,", {"var:= {loc: 3, len: 2}"},
             R"~~(var={loc: 3, len: 2}
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Object_TextMarker) {
  TestScript(R"~~(data:text/html,
                    <textarea id="textarea">Text</textarea>)~~",
             {"var:= {:2, 2, down}"},
             R"~~(var={:2, 2, down}
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Object_TextMarkerArray) {
  TestScript(R"~~(data:text/html,
                    <textarea id="textarea">Text</textarea>)~~",
             {"var:= [{:2, 2, down}, {:1, 1, up}]"},
             R"~~(var=[{:2, 2, down}, {:1, 1, up}]
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Object_TextMarkerRange) {
  TestScript(R"~~(data:text/html,
                    <textarea id="textarea">Text</textarea>)~~",
             {"var:= {anchor: {:3, 0, down}, focus: {:3, 4, down}}"},
             R"~~(var={anchor: {:3, 0, down}, focus: {:3, 4, down}}
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest, Script_Chain) {
  TestScript(R"~~(data:text/html,
                    <input id='input' aria-label='input'>)~~",
             {"input.AXFocusableAncestor.AXRole"},
             R"~~(input.AXFocusableAncestor.AXRole='AXTextField'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Chain_Array) {
  TestScript(R"~~(data:text/html,
                    <p id='p'>Paragraph</p>)~~",
             {"p.AXChildren[0].AXRole"},
             R"~~(p.AXChildren[0].AXRole='AXStaticText'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Chain_Array_OutOfRange) {
  TestScript(R"~~(data:text/html,
                    <p id='p'>Paragraph</p>)~~",
             {"p.AXChildren[9999].AXRole"},
             R"~~(p.AXChildren[9999].AXRole=ERROR:FAILED_TO_PARSE
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Chain_Array_Count) {
  TestScript(R"~~(data:text/html,
                    <p id='p'><span>1</span><span>2</span><span>3</span></p>)~~",
             {"p.AXChildren.count"},
             R"~~(p.AXChildren.count=3
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Chain_TextRange_Anchor) {
  TestScript(R"~~(data:text/html,
                    <p id='p'>Paragraph</p>)~~",
             {"p.AXTextMarkerRangeForUIElement(p).anchor"},
             R"~~(p.AXTextMarkerRangeForUIElement(p).anchor={:2, 0, down}
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Chain_TextRange_Focus) {
  TestScript(R"~~(data:text/html,
                    <p id='p'>Paragraph</p>)~~",
             {"p.AXTextMarkerRangeForUIElement(p).focus"},
             R"~~(p.AXTextMarkerRangeForUIElement(p).focus={:2, 9, down}
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Chain_TextRange_Error) {
  TestScript(R"~~(data:text/html,
                    <p id='p'>Paragraph</p>)~~",
             {"p.AXTextMarkerRangeForUIElement(p).haha"},
             R"~~(p.AXTextMarkerRangeForUIElement(p).haha=ERROR:FAILED_TO_PARSE
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Variables_AXElement) {
  TestScript(R"~~(data:text/html,
                    <p id='p'>Paragraph</p>)~~",
             {"text:= p.AXChildren[0]", "text.AXRole"},
             R"~~(text=:3
text.AXRole='AXStaticText'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_Variables_Null) {
  TestScript(R"~~(data:text/html,
                    <p id='p'>Paragraph</p>)~~",
             {"var:= p.AXTitleUIElement", "var"},
             R"~~(var=NULL
var=NULL
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_ActionNames) {
  TestScript(
      R"~~(data:text/html,
                    <button id='button'>Press me</button>)~~",
      {"button.AXActionNames"},
      R"~~(button.AXActionNames=['AXPress', 'AXShowMenu', 'AXScrollToVisible']
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_PerformAction) {
  TestScript(R"~~(data:text/html,
                    <button id='button'>Press me</button>)~~",
             {"button.AXPerformAction(AXPress)"}, R"~~()~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_SetAttribute) {
  TestScript(
      R"~~(data:text/html,
                    <textarea id="textarea">Text</textarea>)~~",
      {"textarea.AXSelectedTextMarkerRange = "
       "textarea.AXTextMarkerRangeForUIElement(textarea)"},
      R"~~(textarea.AXSelectedTextMarkerRange={anchor: {:3, 0, down}, focus: {:3, 4, down}}
)~~");
}

}  // namespace content
