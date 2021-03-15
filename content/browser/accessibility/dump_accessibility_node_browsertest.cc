// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/accessibility_switches.h"

namespace content {

using ui::AXPropertyFilter;
using ui::AXTreeFormatter;

class DumpAccessibilityNodeTest : public DumpAccessibilityTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // kDisableAXMenuList is true on Chrome OS by default. This can cause the
    // calculation of text alternatives from content to fail in blink tests
    // which include a select element descendant.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kDisableAXMenuList, "false");
  }

  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    std::vector<AXPropertyFilter> property_filters;
    property_filters.emplace_back("value='*'", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("value='http*'", AXPropertyFilter::DENY);
    property_filters.emplace_back("layout-guess:*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("select*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("selectedFromFocus=*",
                                  AXPropertyFilter::DENY);
    property_filters.emplace_back("descript*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("check*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("horizontal", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("multiselectable", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("placeholder=*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("*=''", AXPropertyFilter::DENY);
    property_filters.emplace_back("name=*", AXPropertyFilter::ALLOW_EMPTY);
    return property_filters;
  }

  std::vector<std::string> Dump(std::vector<std::string>& unused) override {
    std::unique_ptr<AXTreeFormatter> formatter(CreateFormatter());

    formatter->SetPropertyFilters(scenario_.property_filters,
                                  AXTreeFormatter::kFiltersDefaultSet);

    BrowserAccessibility* test_node = FindNodeByHTMLAttribute("id", "test");
    if (!test_node)
      test_node = FindNodeByHTMLAttribute("class", "test");

    std::string contents =
        test_node ? formatter->FormatNode(test_node) : "Test node not found.";
    return base::SplitString(contents, "\n", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  }

  void RunAriaTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "aria");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath aria_file = test_path.Append(base::FilePath(file_path));
    RunTest(aria_file, "accessibility/aria", FILE_PATH_LITERAL("node"));
  }

  void RunHtmlTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "html");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath html_file = test_path.Append(base::FilePath(file_path));
    RunTest(html_file, "accessibility/html", FILE_PATH_LITERAL("node"));
  }
};

// Parameterize the tests so that each test-pass is run independently.
struct TestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<AXInspectFactory::Type>& i) const {
    return std::string(i.param);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityNodeTest,
    ::testing::ValuesIn(DumpAccessibilityTestHelper::TreeTestPasses()),
    TestPassToString());

// ARIA tests.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityNodeTest, AccessibilityAriaScrollbar) {
  RunAriaTest(FILE_PATH_LITERAL("aria-scrollbar.html"));
}

// HTML tests.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityNodeTest,
                       AccessibilityTableThColHeader) {
  RunHtmlTest(FILE_PATH_LITERAL("table-th-colheader.html"));
}

}  // namespace content
