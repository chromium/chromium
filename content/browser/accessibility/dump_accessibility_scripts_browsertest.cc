// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "build/build_config.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

using ui::AXPropertyFilter;
using ui::AXTreeFormatter;

// See content/test/data/accessibility/readme.md for an overview.
//
// This test loads an HTML file, invokes a script, and then
// compares the script output against an expected baseline.
//
// The flow of the test is as outlined below.
// 1. Load an html file from content/test/data/accessibility.
// 2. Read the expectation.
// 3. Browse to the page, executes scripts and format their output.
// 4. Perform a comparison between actual and expected and fail if they do not
//    exactly match.
class DumpAccessibilityScriptTest : public DumpAccessibilityTestBase {
 public:
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override;
  void AddPropertyFilter(
      std::vector<AXPropertyFilter>* property_filters,
      const std::string& filter,
      AXPropertyFilter::Type type = AXPropertyFilter::ALLOW) {
    property_filters->push_back(AXPropertyFilter(filter, type));
  }

  std::vector<std::string> Dump(std::vector<std::string>& unused) override {
    std::unique_ptr<AXTreeFormatter> formatter(CreateFormatter());

    // Set test provided property filters.
    formatter->SetPropertyFilters(scenario_.property_filters,
                                  AXTreeFormatter::kFiltersDefaultSet);

    // No accessible tree nodes, just run scripts.
    formatter->SetNodeFilters({{"*", "*"}});

    std::string actual_contents =
        formatter->Format(GetRootAccessibilityNode(shell()->web_contents()));
    return base::SplitString(actual_contents, "\n", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  }

  void RunMacTextMarkerTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path =
        GetTestFilePath("accessibility", "mac/textmarker");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath html_file = test_path.Append(base::FilePath(file_path));

    RunTest(html_file, "accessibility/mac/textmarker");
  }
};

std::vector<ui::AXPropertyFilter> DumpAccessibilityScriptTest::DefaultFilters()
    const {
  return {};
}

// Parameterize the tests so that each test-pass is run independently.
struct TestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<AXInspectFactory::Type>& i) const {
    return std::string(i.param);
  }
};

//
// Scripting supported on Mac only.
//

#if defined(OS_MAC)

INSTANTIATE_TEST_SUITE_P(All,
                         DumpAccessibilityScriptTest,
                         ::testing::Values(AXInspectFactory::kMac),
                         TestPassToString());

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXStartTextMarker) {
  RunMacTextMarkerTest(FILE_PATH_LITERAL("ax_start_text_marker.html"));
}

#endif

}  // namespace content
