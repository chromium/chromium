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
#include "url/gurl.h"

namespace content {

namespace {

const ui::AXPropertyFilter::Type ALLOW_EMPTY =
    ui::AXPropertyFilter::ALLOW_EMPTY;
const ui::AXPropertyFilter::Type SCRIPT = ui::AXPropertyFilter::SCRIPT;

class AccessibilityTreeFormatterMacBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityTreeFormatterMacBrowserTest() {}
  ~AccessibilityTreeFormatterMacBrowserTest() override {}

  // Checks the formatted accessible tree for the given data URL.
  void TestAndCheck(const char* url,
                    const std::vector<ui::AXPropertyFilter>& property_filters,
                    const std::vector<ui::AXNodeFilter>& node_filters,
                    const char* expected) const;

  void TestAndCheck(const char* url,
                    const std::vector<const char*>& filters,
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

void AccessibilityTreeFormatterMacBrowserTest::TestAndCheck(
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

void AccessibilityTreeFormatterMacBrowserTest::TestAndCheck(
    const char* url,
    const std::vector<const char*>& filters,
    const char* expected) const {
  std::vector<ui::AXPropertyFilter> property_filters;
  for (const char* filter : filters) {
    property_filters.emplace_back(filter, ALLOW_EMPTY);
  }
  TestAndCheck(url, property_filters, {}, expected);
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

    TestAndCheck(url, {filter.c_str()}, expected.c_str());
  }
}

}  // namespace

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       DefaultAttributes) {
  TestAndCheck(R"~~(data:text/html,
                    <input aria-label='input'>)~~",
               {},
               R"~~(AXWebArea
++AXGroup
++++AXTextField AXDescription='input'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Filters_NoWildcardProperty) {
  TestAndCheck(R"~~(data:text/html,
                    <input class='classolasso'>)~~",
               {"AXDOMClassList"},
               R"~~(AXWebArea AXDOMClassList=[]
++AXGroup AXDOMClassList=[]
++++AXTextField AXDOMClassList=['classolasso']
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Filters_LineIndex) {
  TestAndCheck(R"~~(data:text/html,
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
  TestAndCheck(R"~~(data:text/html,
                    <p>Paragraph</p>)~~",
               {":3;AXStartTextMarker=*"}, R"~~(AXWebArea
++AXGroup
++++AXStaticText AXStartTextMarker={:1, 0, down} AXValue='Paragraph'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Serialize_AXTextMarkerRange) {
  TestAndCheck(R"~~(data:text/html,
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
                       ParameterizedAttributes_Int) {
  TestAndCheck(R"~~(data:text/html,
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
  TestAndCheck(R"~~(data:text/html,
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
  TestAndCheck(R"~~(data:text/html,
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
                       ParameterizedAttributes_NSRange) {
  TestAndCheck(R"~~(data:text/html,
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
  TestAndCheck(R"~~(data:text/html,
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
  TestAndCheck(R"~~(data:text/html,
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
  TestAndCheck(R"~~(data:text/html,
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
  TestAndCheck(R"~~(data:text/html,
                    <p>Text</p>)~~",
               {":1;AXIndexForTextMarker(AXTextMarkerForIndex(0))"},
               R"~~(AXWebArea AXIndexForTextMarker(AXTextMarkerForIndex(0))=0
++AXGroup
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest, Script) {
  TestAndCheck(R"~~(data:text/html,
                    <input aria-label='input'>)~~",
               {{":3.AXRole", SCRIPT}}, {{"*", "*"}},
               R"~~(:3.AXRole='AXTextField'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_ByDOMId) {
  TestAndCheck(R"~~(data:text/html,
                    <input id='textbox' aria-label='input'>)~~",
               {{"textbox.AXRole", SCRIPT}}, {{"*", "*"}},
               R"~~(textbox.AXRole='AXTextField'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Script_ByDOMId_WrongDOMId) {
  TestAndCheck(R"~~(data:text/html,
                    <input id='textbox' aria-label='input'>)~~",
               {{"textbo.AXRole", SCRIPT}}, {{"*", "*"}},
               R"~~(textbo.AXRole=ERROR:FAILED_TO_PARSE
)~~");
}

}  // namespace content
