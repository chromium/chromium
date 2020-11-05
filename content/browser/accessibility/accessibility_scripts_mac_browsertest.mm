// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/strings/sys_string_conversions.h"
#include "content/browser/accessibility/accessibility_tools_utils_mac.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_mac.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/dump_accessibility_test_helper.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"

using content::a11y::LineIndexer;
using content::a11y::AttributeInvoker;
using content::a11y::OptionalNSObject;
using content::a11y::AttributeValueOf;

namespace content {

namespace {

class AccessibilityScriptsMacBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityScriptsMacBrowserTest()
      : test_file_path_(),
        root_(nullptr),
        line_indexer_(nullptr),
        script_output_(),
        formatter_(AccessibilityTreeFormatter::Create()),
        helper_("mac") {
    // Set property filters.
    std::vector<ui::AXPropertyFilter> property_filters;
    formatter_->AddDefaultFilters(&property_filters);
    formatter_->SetPropertyFilters(property_filters);
  }

  void LoadFile(const std::string& file);
  void AssertOutputMatchesExpectations();

 protected:
  AttributeInvoker GetInvokerAndAssertRole(
      const std::string& line_index,
      const std::string& expected_role) const {
    return AttributeInvoker(
        GetNativeNodeAndAssertRole(line_index, expected_role),
        line_indexer_.get());
  }

  OptionalNSObject GetNodeAndAssertRole(
      const std::string& line_index,
      const std::string& expected_role) const {
    return OptionalNSObject::NotNilOrError(
        GetNativeNodeAndAssertRole(line_index, expected_role));
  }

  void Print(const OptionalNSObject& object) {
    script_output_ << object.ToString() << "\n";
  }

  void WaitForEvent(ax::mojom::Event event) const {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete, event);
    waiter.WaitForNotification();
  }

 private:
  BrowserAccessibilityManager* GetManager() const {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

  gfx::NativeViewAccessible GetNativeNodeAndAssertRole(
      const std::string& line_index,
      const std::string& expected_role) const;

  base::FilePath test_file_path_;
  BrowserAccessibility* root_;
  std::unique_ptr<LineIndexer> line_indexer_;
  std::ostringstream script_output_;
  std::unique_ptr<ui::AXTreeFormatter> formatter_;
  DumpAccessibilityTestHelper helper_;
};

gfx::NativeViewAccessible
AccessibilityScriptsMacBrowserTest::GetNativeNodeAndAssertRole(
    const std::string& line_index,
    const std::string& expected_role) const {
  gfx::NativeViewAccessible node = line_indexer_->NodeBy(line_index);
  NSString* role_nsstring =
      AttributeValueOf(node, NSAccessibilityRoleAttribute);
  std::string role_string = base::SysNSStringToUTF8(role_nsstring);
  EXPECT_EQ(role_string, expected_role);
  return node;
}

void AccessibilityScriptsMacBrowserTest::LoadFile(const std::string& file) {
  ASSERT_FALSE(file.empty());
  ASSERT_TRUE(test_file_path_.empty());
  ASSERT_EQ(nullptr, root_);
  ASSERT_EQ(nullptr, line_indexer_.get());

  std::string dir = base::FilePath()
                        .AppendASCII("accessibility")
                        .AppendASCII("scripts")
                        .MaybeAsASCII();

  test_file_path_ = GetTestFilePath(dir.c_str(), file.c_str());

  ASSERT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  ASSERT_TRUE(NavigateToURL(shell(), net::FilePathToFileURL(test_file_path_)));
  waiter.WaitForNotification();

  root_ = GetManager()->GetRoot();
  ASSERT_NE(nullptr, root_);

  BrowserAccessibilityCocoa* cocoa_root = ToBrowserAccessibilityCocoa(root_);
  line_indexer_ = std::make_unique<LineIndexer>(cocoa_root);
}

void AccessibilityScriptsMacBrowserTest::AssertOutputMatchesExpectations() {
  // Collect the tree dump and script output.
  std::ostringstream output;
  {
    std::string contents;
    formatter_->FormatAccessibilityTreeForTesting(root_, &contents);
    output << contents;
    output << script_output_.str();
  }
  std::vector<std::string> lines = base::SplitString(
      output.str(), "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Load expected output lines.
  base::FilePath expectation_file =
      helper_.GetExpectationFilePath(test_file_path_);
  EXPECT_FALSE(expectation_file.empty());
  base::Optional<std::vector<std::string>> expected_lines =
      helper_.LoadExpectationFile(expectation_file);
  EXPECT_TRUE(expected_lines.has_value());

  bool matches_expectation = helper_.ValidateAgainstExpectation(
      test_file_path_, expectation_file, lines, *expected_lines);
  EXPECT_TRUE(matches_expectation);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(AccessibilityScriptsMacBrowserTest,
                       SetSelectedTextMarkerRange_ContentEditable) {
  LoadFile("set-selection-contenteditable.html");
  // Select the 1st word via text marker associated with container AXTextArea.
  {
    AttributeInvoker textarea = GetInvokerAndAssertRole(":2", "AXTextArea");
    OptionalNSObject text_range =
        textarea.GetValue("AXTextMarkerRangeForUIElement",
                          GetNodeAndAssertRole(":2", "AXTextArea"));

    OptionalNSObject marker_0 = a11y::TextMarkerRangeGetStartMarker(text_range);
    OptionalNSObject marker_1 =
        textarea.GetValue("AXNextWordEndTextMarkerForTextMarker", marker_0);
    OptionalNSObject target_selected_marker_range =
        textarea.GetValue("AXTextMarkerRangeForUnorderedTextMarkers",
                          a11y::MakePairArray(marker_0, marker_1));
    textarea.SetValue("AXSelectedTextMarkerRange",
                      target_selected_marker_range);

    WaitForEvent(ax::mojom::Event::kTextSelectionChanged);

    OptionalNSObject selected_text = textarea.GetValue("AXSelectedText");
    Print(selected_text);
  }
  // Select the 2nd word via the marker associated with AXStaticText.
  {
    AttributeInvoker textarea = GetInvokerAndAssertRole(":2", "AXTextArea");
    AttributeInvoker text = GetInvokerAndAssertRole(":4", "AXStaticText");
    OptionalNSObject text_range =
        text.GetValue("AXTextMarkerRangeForUIElement",
                      GetNodeAndAssertRole(":4", "AXStaticText"));

    OptionalNSObject marker_0 = a11y::TextMarkerRangeGetStartMarker(text_range);
    OptionalNSObject marker_1 =
        text.GetValue("AXNextWordEndTextMarkerForTextMarker", marker_0);
    OptionalNSObject marker_2 =
        text.GetValue("AXNextWordEndTextMarkerForTextMarker", marker_1);
    OptionalNSObject marker_3 =
        text.GetValue("AXPreviousWordStartTextMarkerForTextMarker", marker_2);
    OptionalNSObject target_selected_marker_range =
        text.GetValue("AXTextMarkerRangeForUnorderedTextMarkers",
                      a11y::MakePairArray(marker_2, marker_3));
    textarea.SetValue("AXSelectedTextMarkerRange",
                      target_selected_marker_range);

    WaitForEvent(ax::mojom::Event::kTextSelectionChanged);

    OptionalNSObject selected_text = textarea.GetValue("AXSelectedText");
    Print(selected_text);
  }
  // Select text inside a span.
  {
    AttributeInvoker textarea = GetInvokerAndAssertRole(":2", "AXTextArea");
    AttributeInvoker text = GetInvokerAndAssertRole(":8", "AXStaticText");
    OptionalNSObject text_range =
        text.GetValue("AXTextMarkerRangeForUIElement",
                      GetNodeAndAssertRole(":8", "AXStaticText"));

    OptionalNSObject marker_0 = a11y::TextMarkerRangeGetStartMarker(text_range);
    OptionalNSObject marker_1 = a11y::TextMarkerRangeGetEndMarker(text_range);
    OptionalNSObject marker_2 =
        text.GetValue("AXPreviousTextMarkerForTextMarker", marker_1);
    OptionalNSObject target_selected_marker_range =
        text.GetValue("AXTextMarkerRangeForUnorderedTextMarkers",
                      a11y::MakePairArray(marker_0, marker_2));
    textarea.SetValue("AXSelectedTextMarkerRange",
                      target_selected_marker_range);

    WaitForEvent(ax::mojom::Event::kTextSelectionChanged);

    OptionalNSObject selected_text = textarea.GetValue("AXSelectedText");
    Print(selected_text);
  }
  // Select the text inside the block with incorrect grammar.
  {
    AttributeInvoker textarea = GetInvokerAndAssertRole(":2", "AXTextArea");
    AttributeInvoker text = GetInvokerAndAssertRole(":6", "AXStaticText");
    OptionalNSObject text_range =
        text.GetValue("AXTextMarkerRangeForUIElement",
                      GetNodeAndAssertRole(":6", "AXStaticText"));

    OptionalNSObject target_selected_marker_range = text_range;
    textarea.SetValue("AXSelectedTextMarkerRange",
                      target_selected_marker_range);

    WaitForEvent(ax::mojom::Event::kTextSelectionChanged);

    OptionalNSObject selected_text = textarea.GetValue("AXSelectedText");
    Print(selected_text);
  }
  // Select the text after a forced break.
  {
    AttributeInvoker textarea = GetInvokerAndAssertRole(":10", "AXTextArea");
    AttributeInvoker text = GetInvokerAndAssertRole(":14", "AXStaticText");
    OptionalNSObject text_range =
        text.GetValue("AXTextMarkerRangeForUIElement",
                      GetNodeAndAssertRole(":14", "AXStaticText"));

    OptionalNSObject marker_0 = a11y::TextMarkerRangeGetStartMarker(text_range);
    OptionalNSObject marker_1 =
        text.GetValue("AXNextWordEndTextMarkerForTextMarker", marker_0);
    OptionalNSObject target_selected_marker_range =
        text.GetValue("AXTextMarkerRangeForUnorderedTextMarkers",
                      a11y::MakePairArray(marker_0, marker_1));
    textarea.SetValue("AXSelectedTextMarkerRange",
                      target_selected_marker_range);

    WaitForEvent(ax::mojom::Event::kTextSelectionChanged);

    OptionalNSObject selected_text = textarea.GetValue("AXSelectedText");
    Print(selected_text);
  }
  AssertOutputMatchesExpectations();
}

IN_PROC_BROWSER_TEST_F(AccessibilityScriptsMacBrowserTest,
                       SetSelectedTextMarkerRange_TextArea) {
  LoadFile("set-selection-textarea.html");

  // Select the 3rd word.
  {
    AttributeInvoker textarea = GetInvokerAndAssertRole(":3", "AXTextArea");
    OptionalNSObject text_range =
        textarea.GetValue("AXTextMarkerRangeForUIElement",
                          GetNodeAndAssertRole(":3", "AXTextArea"));

    OptionalNSObject marker_0 = a11y::TextMarkerRangeGetStartMarker(text_range);
    OptionalNSObject marker_1 =
        textarea.GetValue("AXNextWordEndTextMarkerForTextMarker", marker_0);
    OptionalNSObject marker_2 =
        textarea.GetValue("AXNextWordEndTextMarkerForTextMarker", marker_1);
    OptionalNSObject marker_3 =
        textarea.GetValue("AXNextWordEndTextMarkerForTextMarker", marker_2);
    OptionalNSObject marker_4 = textarea.GetValue(
        "AXPreviousWordStartTextMarkerForTextMarker", marker_3);
    OptionalNSObject target_selected_marker_range =
        textarea.GetValue("AXTextMarkerRangeForUnorderedTextMarkers",
                          a11y::MakePairArray(marker_3, marker_4));
    textarea.SetValue("AXSelectedTextMarkerRange",
                      target_selected_marker_range);

    WaitForEvent(ax::mojom::Event::kTextSelectionChanged);

    OptionalNSObject selected_text = textarea.GetValue("AXSelectedText");
    Print(selected_text);
  }

  AssertOutputMatchesExpectations();
}

}  // namespace content
