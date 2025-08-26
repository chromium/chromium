// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_BROWSERTEST_H_

#include "base/command_line.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"

namespace content {

constexpr const char kAccName[]{"accname"};
constexpr const char kAria[]{"aria"};
constexpr const char kCSS[]{"css"};
constexpr const char kCrash[]{"crash"};
constexpr const char kFormControls[]{"form-controls"};
constexpr const char kHtml[]{"html"};
constexpr const char kMathML[]{"mathml"};
constexpr const char kDisplayLocking[]{"display-locking"};
constexpr const char kRelations[]{"relations"};
constexpr const char kRegression[]{"regression"};
constexpr const char kTestHarness[]{"test-harness"};
inline constexpr const char kMaterialDesign[]{"material-design"};

// See content/test/data/accessibility/readme.md for an overview.
//
// Use tools/accessibility/rebase_dump_accessibility_tree_tests.py to
// update test expectations.
//
// This test takes a snapshot of the platform BrowserAccessibility tree and
// tests it against an expected baseline.
//
// The flow of the test is as outlined below.
// 1. Load an html file from content/test/data/accessibility.
// 2. Read the expectation.
// 3. Browse to the page, wait for the accessibility tree to load, optionally
//    wait for certain conditions (such as @WAIT-FOR) and serialize the platform
//    specific tree into a human readable string.
// 4. Perform a comparison between actual and expected and fail if they do not
//    exactly match.
class DumpAccessibilityTreeTest : public DumpAccessibilityTestBase {
 public:
  DumpAccessibilityTreeTest();
  ~DumpAccessibilityTreeTest() override;


  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetUpOnMainThread() override;

  ukm::TestUkmRecorder& recorder() { return *ukm_recorder_; }

  std::vector<std::string> Dump() override;

// Convenience macro to define test types without special treatment.
#define TEST_TYPE(type)                                             \
  void Run##type##Test(const base::FilePath::CharType* file_path) { \
    RunTypedTest<k##type>(file_path);                               \
  }
  TEST_TYPE(AccName)
  TEST_TYPE(Aria)
  TEST_TYPE(CSS)
  TEST_TYPE(Crash)
  TEST_TYPE(Html)
  TEST_TYPE(MathML)
  TEST_TYPE(MaterialDesign)
  TEST_TYPE(DisplayLocking)
  TEST_TYPE(Relations)
  TEST_TYPE(Regression)
  TEST_TYPE(TestHarness)

  void RunCrashTest(const base::FilePath::CharType* file_path,
                    ui::AXMode mode) {
    RunTypedTest<kCrash>(file_path, mode);
  }

  void RunFormControlsTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kFormControls>(file_path, ui::kAXModeFormControls);
  }

  void RunNoScreenReaderDisplayLockingTest(
      const base::FilePath::CharType* file_path) {
    RunTypedTest<kDisplayLocking>(file_path, ui::kAXModeComplete,
                                  FILE_PATH_LITERAL("no-screen-reader"));
  }

  void RunOnScreenTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kHtml>(file_path, ui::kAXModeOnScreen);
  }

  // TODO(accessibility): Replace all tests using RunPopoverHintTest to just
  // RunHtmlTest when `interestfor` is enabled by default.
  void RunPopoverHintTest(const base::FilePath::CharType* file_path) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "HTMLInterestForAttribute");
    RunTypedTest<kHtml>(file_path);
  }

 protected:
  // Override from DumpAccessibilityTestBase.
  void ChooseFeatures(
      std::vector<base::test::FeatureRef>* enabled_features,
      std::vector<base::test::FeatureRef>* disabled_features) override;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_BROWSERTEST_H_
