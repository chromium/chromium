// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/platform/inspect/ax_script_instruction.h"

namespace content {

using ui::AXPropertyFilter;
using ui::AXScriptInstruction;
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

  base::Value EvaluateScript(
      AXTreeFormatter* formatter,
      BrowserAccessibility* root,
      const std::vector<AXScriptInstruction>& instructions,
      size_t start_index,
      size_t end_index) {
    return base::Value(
        formatter->EvaluateScript(root, instructions, start_index, end_index));
  }

  std::vector<std::string> Dump() override {
    std::vector<std::string> dump;
    std::unique_ptr<AXTreeFormatter> formatter(CreateFormatter());
    BrowserAccessibility* root = GetManager()->GetRoot();

    size_t start_index = 0;
    size_t length = scenario_.script_instructions.size();
    while (start_index < length) {
      std::string wait_for;
      size_t index = start_index;
      for (; index < length; index++) {
        if (scenario_.script_instructions[index].IsEvent()) {
          wait_for = scenario_.script_instructions[index].AsEvent();
          break;
        }
      }

      std::string actual_contents;
      if (wait_for.empty()) {
        actual_contents = formatter->EvaluateScript(
            root, scenario_.script_instructions, start_index, index);
      } else {
        auto pair = CaptureEvents(
            base::BindOnce(&DumpAccessibilityScriptTest::EvaluateScript,
                           base::Unretained(this), formatter.get(), root,
                           scenario_.script_instructions, start_index, index));
        actual_contents = pair.first.GetString();
        for (auto event : pair.second) {
          if (base::StartsWith(event, wait_for)) {
            actual_contents += event + '\n';
          }
        }
      }

      auto chunk =
          base::SplitString(actual_contents, "\n", base::KEEP_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
      dump.insert(dump.end(), chunk.begin(), chunk.end());

      start_index = index + 1;
    }
    return dump;
  }

  void RunMacActionTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "mac/action");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath html_file = test_path.Append(base::FilePath(file_path));

    RunTest(html_file, "accessibility/mac/action");
  }

  void RunMacSelectionTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path =
        GetTestFilePath("accessibility", "mac/selection");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath html_file = test_path.Append(base::FilePath(file_path));

    RunTest(html_file, "accessibility/mac/selection");
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

  void RunMacMethodsTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "mac/methods");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath html_file = test_path.Append(base::FilePath(file_path));
    RunTest(html_file, "accessibility/mac/methods");
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXPressButton) {
  RunMacActionTest(FILE_PATH_LITERAL("ax-press-button.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, SelectAllTextarea) {
  RunMacSelectionTest(FILE_PATH_LITERAL("selectall-textarea.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       SetSelectionContenteditable) {
  RunMacSelectionTest(FILE_PATH_LITERAL("set-selection-contenteditable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, SetSelectionTextarea) {
  RunMacSelectionTest(FILE_PATH_LITERAL("set-selection-textarea.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       SetSelectedTextRangeContenteditable) {
  RunMacSelectionTest(
      FILE_PATH_LITERAL("set-selectedtextrange-contenteditable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AXNextWordEndTextMarkerForTextMarker) {
  RunMacTextMarkerTest(
      FILE_PATH_LITERAL("ax-next-word-end-text-marker-for-text-marker.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AXPreviousWordStartTextMarkerForTextMarker) {
  RunMacTextMarkerTest(FILE_PATH_LITERAL(
      "ax-previous-word-start-text-marker-for-text-marker.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXStartTextMarker) {
  RunMacTextMarkerTest(FILE_PATH_LITERAL("ax_start_text_marker.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AXTextMarkerRangeForUIElement) {
  RunMacTextMarkerTest(
      FILE_PATH_LITERAL("ax-text-marker-range-for-ui-element.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AccessibilityPlaceholderValue) {
  RunMacMethodsTest(FILE_PATH_LITERAL("placeholder-value.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AccessibilityTitle) {
  RunMacMethodsTest(FILE_PATH_LITERAL("title.html"));
}

#endif

}  // namespace content
