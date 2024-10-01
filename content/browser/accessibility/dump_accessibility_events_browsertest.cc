// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter.h"
#if BUILDFLAG(IS_WIN)
#include "ui/accessibility/platform/browser_accessibility_manager_win.h"
#endif

namespace content {

using ui::AXPropertyFilter;
using ui::AXTreeFormatter;

// See content/test/data/accessibility/readme.md for an overview.
//
// Tests that the right platform-specific accessibility events are fired
// in response to things that happen in a web document.
//
// Similar to DumpAccessibilityTree in that each test consists of a
// single HTML file, possibly with a few special directives in comments,
// and then expectation files in text format for each platform.
//
// While DumpAccessibilityTree just loads the document and then
// prints out a text representation of the accessibility tree,
// DumpAccessibilityEvents loads the document, then executes the
// JavaScript function "go()", then it records and dumps all accessibility
// events generated as a result of that "go" function executing.
//
// How each event is dumped is platform-specific, but should be of the
// form:
//
// <event> on <node>
//
// ...where <event> is the name of the event, and <node> is a description
// of the node the event fired on, such as the node's role and name.
//
// As with DumpAccessibilityTree, DumpAccessibilityEvents takes the events
// dumped from that particular html file and compares it to the expectation
// file in the same directory (for example, test-name-expected-win.txt)
// and the test fails if they don't agree.
//
// Currently it's not possible to test for accessibility events that
// don't fire immediately (i.e. within the call scope of the call to "go()");
// the test framework calls "go()" and then sends a sentinel event signaling
// the end of the test; anything received after that is too late.
class DumpAccessibilityEventsTest : public DumpAccessibilityTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "KeyboardFocusableScrollers");
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ShadowRootReferenceTarget");
    DumpAccessibilityTestBase::SetUpCommandLine(command_line);
  }

  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    std::vector<ui::AXPropertyFilter> property_filters;
    // Suppress spurious focus events on the document object.
    property_filters.emplace_back("EVENT_OBJECT_FOCUS*DOCUMENT*",
                                  AXPropertyFilter::DENY);
    property_filters.emplace_back("AutomationFocusChanged*document*",
                                  AXPropertyFilter::DENY);
    // Implementing IRawElementProviderAdviseEvents causes Win7 to fire
    // spurious focus events (regardless of what the implementation does).
    property_filters.emplace_back("AutomationFocusChanged on role=region",
                                  AXPropertyFilter::DENY);
    return property_filters;
  }

  std::vector<std::string> Dump(ui::AXMode mode) override;

  void OnDiffFailed() override;
  void RunEventTest(const base::FilePath::CharType* file_path);

 private:
  std::string initial_tree_;
  std::string final_tree_;
};

std::vector<std::string> DumpAccessibilityEventsTest::Dump(ui::AXMode mode) {
  WebContentsImpl* web_contents = GetWebContents();

  // Save a copy of the accessibility tree (as a text dump); we'll
  // log this for the user later if the test fails.
  initial_tree_ = DumpUnfilteredAccessibilityTreeAsString();

  final_tree_.clear();
  bool run_go_again = false;
  std::vector<std::string> result;
  do {
    // Dump the event logs, running them through any filters specified
    // in the HTML file.
    auto [go_results, event_logs] = CaptureEvents(
        base::BindOnce([](RenderFrameHostImpl* frame,
                          std::string script) { return EvalJs(frame, script); },
                       web_contents->GetPrimaryMainFrame(), "go()"),
        ui::kAXModeComplete);
    run_go_again = go_results == true;
    // Save a copy of the final accessibility tree (as a text dump); we'll
    // log this for the user later if the test fails.
    final_tree_.append(DumpUnfilteredAccessibilityTreeAsString());

    for (auto& event_log : event_logs) {
      if (AXTreeFormatter::MatchesPropertyFilters(scenario_.property_filters,
                                                  event_log, true)) {
        result.push_back(base::EscapeNonASCII(event_log));
      }
    }

    if (run_go_again) {
      final_tree_.append("=== Start Continuation ===\n");
      result.emplace_back("=== Start Continuation ===");
    }
  } while (run_go_again);

  return result;
}

void DumpAccessibilityEventsTest::OnDiffFailed() {
  printf("\n");
  printf("Initial accessibility tree (after load complete):\n");
  printf("%s\n", initial_tree_.c_str());
  printf("\n");
  printf("Final accessibility tree after events fired:\n");
  printf("%s\n", final_tree_.c_str());
  printf("\n");
}

void DumpAccessibilityEventsTest::RunEventTest(
    const base::FilePath::CharType* file_path) {
  base::FilePath test_path = GetTestFilePath("accessibility", "event");

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
  }

  base::FilePath event_file = test_path.Append(base::FilePath(file_path));
  RunTest(event_file, "accessibility/event");
}

class DumpAccessibilityEventsTestExceptUIA
    : public DumpAccessibilityEventsTest {};

// Parameterize the tests so that each test-pass is run independently.
struct DumpAccessibilityEventsTestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<ui::AXApiType::Type>& i) const {
    return std::string(i.param);
  }
};

// UIA is excluded due to flakiness. See https://crbug.com/1459215
INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityEventsTest,
    ::testing::ValuesIn(DumpAccessibilityTestBase::EventTestPasses()),
    DumpAccessibilityEventsTestPassToString());

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityEventsTestExceptUIA,
    ::testing::ValuesIn(DumpAccessibilityTestBase::EventTestPassesExceptUIA()),
    DumpAccessibilityEventsTestPassToString());

class DumpAccessibilityEventsWithExperimentalWebFeaturesTest
    : public DumpAccessibilityEventsTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

// TODO(crbug.com/40841326): disabled on UIA.
INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityEventsWithExperimentalWebFeaturesTest,
    ::testing::ValuesIn(DumpAccessibilityTestBase::EventTestPassesExceptUIA()),
    DumpAccessibilityEventsTestPassToString());

// This test suite is empty on some OSes.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    DumpAccessibilityEventsWithExperimentalWebFeaturesTest);

// This test suite is empty on some OSes.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DumpAccessibilityEventsTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    DumpAccessibilityEventsTestExceptUIA);

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaAtomicChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-atomic-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaAtomicChanged2) {
  RunEventTest(FILE_PATH_LITERAL("aria-atomic-changed2.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaBusyChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-busy-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaButtonExpand) {
  RunEventTest(FILE_PATH_LITERAL("aria-button-expand.html"));
}

// crbug.com/1142637: disabled due to missing invalidation causing flakiness.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsAriaComboBoxCollapse) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-collapse.html"));
}

// TODO(crbug.com/40844027): Flaky on win
#if BUILDFLAG(IS_WIN)
#define MAYBE_AccessibilityEventsAriaComboBoxExpand \
  DISABLED_AccessibilityEventsAriaComboBoxExpand
#else
#define MAYBE_AccessibilityEventsAriaComboBoxExpand \
  AccessibilityEventsAriaComboBoxExpand
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsAriaComboBoxExpand) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-expand.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaControlsChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-controls-changed.html"));
}

// TODO(nektar): Re-enable this test after kValueChanged is removed from Blink.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsAriaComboBoxUneditable) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-uneditable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaCurrentChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-current-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaDisabledChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-disabled-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaHasPopupChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-haspopup-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaHiddenChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-hidden-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaLevelChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-level-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaLiveChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-live-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaMenuItemFocus) {
  RunEventTest(FILE_PATH_LITERAL("aria-menuitem-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaMultilineChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-multiline-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaPosinsetChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-posinset-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaPressedChangesButtonRole) {
  RunEventTest(FILE_PATH_LITERAL("aria-pressed-changes-button-role.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaReadonlyChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-readonly-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaRelevantChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-relevant-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaRelevantChanged2) {
  RunEventTest(FILE_PATH_LITERAL("aria-relevant-changed2.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSetSizeChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-setsize-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSortChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-sort-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTextboxChildrenChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-textbox-children-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTextboxEditabilityChanges) {
  RunEventTest(FILE_PATH_LITERAL("aria-textbox-editability-changes.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTextboxWithFocusableChildren) {
  RunEventTest(FILE_PATH_LITERAL("aria-textbox-with-focusable-children.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTreeCollapse) {
  RunEventTest(FILE_PATH_LITERAL("aria-tree-collapse.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTreeExpand) {
  RunEventTest(FILE_PATH_LITERAL("aria-tree-expand.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTreeItemFocus) {
  RunEventTest(FILE_PATH_LITERAL("aria-treeitem-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaTreeItemFocusReferenceTarget) {
  RunEventTest(FILE_PATH_LITERAL("aria-treeitem-focus-reference-target.html"));
}

// TODO(crbug.com/40844027): Flaky on win & mac
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_AccessibilityEventsAriaComboBoxFocus \
  DISABLED_AccessibilityEventsAriaComboBoxFocus
#else
#define MAYBE_AccessibilityEventsAriaComboBoxFocus \
  AccessibilityEventsAriaComboBoxFocus
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsAriaComboBoxFocus) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-focus.html"));
}

// TODO(crbug.com/41384724): Fails on Windows.
// TODO(crbug.com/41448628): Flaky on Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_AccessibilityEventsAriaComboBoxDelayAddList \
  DISABLED_AccessibilityEventsAriaComboBoxDelayAddList
#else
#define MAYBE_AccessibilityEventsAriaComboBoxDelayAddList \
  AccessibilityEventsAriaComboBoxDelayAddList
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsAriaComboBoxDelayAddList) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-delay-add-list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaComboBoxDelayShowList) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-delay-show-list.html"));
}

// TODO(crbug.com/40844027): Flaky on win
#if BUILDFLAG(IS_WIN)
#define MAYBE_AccessibilityEventsAriaComboBoxNext \
  DISABLED_AccessibilityEventsAriaComboBoxNext
#else
#define MAYBE_AccessibilityEventsAriaComboBoxNext \
  AccessibilityEventsAriaComboBoxNext
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsAriaComboBoxNext) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-next.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSliderValueBothChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-slider-value-both-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSliderValueChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-slider-value-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSliderValueTextChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-slider-valuetext-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSpinButtonValueBothChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-spinbutton-value-both-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSpinButtonValueChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-spinbutton-value-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSpinButtonValueTextChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-spinbutton-valuetext-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddAlert) {
  RunEventTest(FILE_PATH_LITERAL("add-alert.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddAlertWithRoleChange) {
  RunEventTest(FILE_PATH_LITERAL("add-alert-with-role-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddAlertContent) {
  RunEventTest(FILE_PATH_LITERAL("add-alert-content.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddChild) {
  RunEventTest(FILE_PATH_LITERAL("add-child.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddChildOfBody) {
  RunEventTest(FILE_PATH_LITERAL("add-child-of-body.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddDialog) {
  RunEventTest(FILE_PATH_LITERAL("add-dialog.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddDialogDescribedBy) {
  RunEventTest(FILE_PATH_LITERAL("add-dialog-described-by.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddDialogNoInfo) {
  RunEventTest(FILE_PATH_LITERAL("add-dialog-no-info.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddHiddenAttribute) {
  RunEventTest(FILE_PATH_LITERAL("add-hidden-attribute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddHiddenAttributeSubtree) {
  RunEventTest(FILE_PATH_LITERAL("add-hidden-attribute-subtree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddSubtree) {
  RunEventTest(FILE_PATH_LITERAL("add-subtree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAnonymousBlockChildrenChanged) {
  RunEventTest(FILE_PATH_LITERAL("anonymous-block-children-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsChildrenChangedOnlyOnAncestor) {
  RunEventTest(FILE_PATH_LITERAL("children-changed-only-on-ancestor.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCheckedStateChanged) {
  RunEventTest(FILE_PATH_LITERAL("checked-state-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCheckedMixedChanged) {
  RunEventTest(FILE_PATH_LITERAL("checked-mixed-changed.html"));
}

// http:/crbug.com/889013
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsCaretHide) {
  RunEventTest(FILE_PATH_LITERAL("caret-hide.html"));
}

// http:/crbug.com/889013
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsCaretMove) {
  RunEventTest(FILE_PATH_LITERAL("caret-move.html"));
}

// Flaky on Windows, disabled on Linux: https://crbug.com/1186887
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_AccessibilityEventsCaretMoveHiddenInput \
  DISABLED_AccessibilityEventsCaretMoveHiddenInput
#else
#define MAYBE_AccessibilityEventsCaretMoveHiddenInput \
  AccessibilityEventsCaretMoveHiddenInput
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsCaretMoveHiddenInput) {
  RunEventTest(FILE_PATH_LITERAL("caret-move-hidden-input.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCheckboxValidity) {
  RunEventTest(FILE_PATH_LITERAL("checkbox-validity.html"));
}

// Flaky on TSAN, see https://crbug.com/1066702
#if defined(THREAD_SANITIZER)
#define MAYBE_AccessibilityEventsCaretBrowsingEnabled \
  DISABLED_AccessibilityEventsCaretBrowsingEnabled
#else
#define MAYBE_AccessibilityEventsCaretBrowsingEnabled \
  AccessibilityEventsCaretBrowsingEnabled
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsCaretBrowsingEnabled) {
  // This actually enables caret browsing without setting the pref.
  GetWebContents()->GetMutableRendererPrefs()->caret_browsing_enabled = true;
  // This notifies accessibility that caret browsing is on so that it sends
  // accessibility events when the caret moves.
  ui::AXPlatform::GetInstance().SetCaretBrowsingState(true);

  RunEventTest(FILE_PATH_LITERAL("caret-browsing-enabled.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCaretBrowsingDisabled) {
  // Make sure command line switch that forces caret browsing on is not set.
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCaretBrowsing));

  RunEventTest(FILE_PATH_LITERAL("caret-browsing-disabled.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSDisplay) {
  RunEventTest(FILE_PATH_LITERAL("css-display.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaExpandedAndCollapsed) {
  RunEventTest(FILE_PATH_LITERAL("aria-expanded-and-collapsed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaExpandedAndCollapsedReparenting) {
  RunEventTest(
      FILE_PATH_LITERAL("aria-expanded-and-collapsed-reparenting.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaHiddenDescendants) {
  RunEventTest(FILE_PATH_LITERAL("aria-hidden-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaHiddenSingleDescendant) {
  RunEventTest(FILE_PATH_LITERAL("aria-hidden-single-descendant.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityEventsTest,
    AccessibilityEventsAriaHiddenSingleDescendantDisplayNone) {
  RunEventTest(
      FILE_PATH_LITERAL("aria-hidden-single-descendant-display-none.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityEventsTest,
    AccessibilityEventsAriaHiddenSingleDescendantVisibilityHidden) {
  RunEventTest(FILE_PATH_LITERAL(
      "aria-hidden-single-descendant-visibility-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaHiddenDescendantsAlreadyIgnored) {
  RunEventTest(
      FILE_PATH_LITERAL("aria-hidden-descendants-already-ignored.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSDisplayDescendants) {
  RunEventTest(FILE_PATH_LITERAL("css-display-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSFlexTextUpdate) {
  RunEventTest(FILE_PATH_LITERAL("css-flex-text-update.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSVisibility) {
  RunEventTest(FILE_PATH_LITERAL("css-visibility.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSVisibilityDescendants) {
  RunEventTest(FILE_PATH_LITERAL("css-visibility-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSCollapse) {
  RunEventTest(FILE_PATH_LITERAL("css-visibility-collapse.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsDescriptionChange) {
  RunEventTest(FILE_PATH_LITERAL("description-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsDescriptionChangeIndirect) {
  RunEventTest(FILE_PATH_LITERAL("description-change-indirect.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsDescriptionChangeNoRelation) {
  RunEventTest(FILE_PATH_LITERAL("description-change-no-relation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsDisabledStateChanged) {
  RunEventTest(FILE_PATH_LITERAL("disabled-state-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsExpandedChanged) {
  RunEventTest(FILE_PATH_LITERAL("expanded-changed.html"));
}

// TODO(crbug.com/40897549): disabled on UIA.
// TODO(crbug.com/40897744): Failing on Mac.
// TODO(crbug.com/40897744): Actually failing everywhere. Disabled.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTestExceptUIA,
                       DISABLED_AccessibilityEventsPopoverExpandedChanged) {
  RunEventTest(FILE_PATH_LITERAL("popover-expanded-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsFormRequiredChanged) {
  RunEventTest(FILE_PATH_LITERAL("form-required-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsFocusListbox) {
  RunEventTest(FILE_PATH_LITERAL("focus-listbox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsFocusListboxMultiselect) {
  RunEventTest(FILE_PATH_LITERAL("focus-listbox-multiselect.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsIframeSrcChanged) {
  RunEventTest(FILE_PATH_LITERAL("iframe-src-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsIndividualNodesBecomeIgnored) {
  RunEventTest(FILE_PATH_LITERAL("individual-nodes-become-ignored.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityEventsTest,
    AccessibilityEventsIndividualNodesBecomeIgnoredButIncluded) {
  RunEventTest(
      FILE_PATH_LITERAL("individual-nodes-become-ignored-but-included.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsInnerHtmlChange) {
  RunEventTest(FILE_PATH_LITERAL("inner-html-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsInputTypeTextValueChanged) {
  RunEventTest(FILE_PATH_LITERAL("input-type-text-value-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsListboxFocus) {
  RunEventTest(FILE_PATH_LITERAL("listbox-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsListboxNext) {
  RunEventTest(FILE_PATH_LITERAL("listbox-next.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionAdd) {
  RunEventTest(FILE_PATH_LITERAL("live-region-add.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionAddLiveAttribute) {
  RunEventTest(FILE_PATH_LITERAL("live-region-add-live-attribute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionChange) {
  RunEventTest(FILE_PATH_LITERAL("live-region-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionCreate) {
  RunEventTest(FILE_PATH_LITERAL("live-region-create.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionOff) {
  RunEventTest(FILE_PATH_LITERAL("live-region-off.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionElemReparent) {
  RunEventTest(FILE_PATH_LITERAL("live-region-elem-reparent.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionIgnoresClick) {
  RunEventTest(FILE_PATH_LITERAL("live-region-ignores-click.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsLiveRegionRemove) {
  RunEventTest(FILE_PATH_LITERAL("live-region-remove.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityEventsTest,
    AccessibilityEventsLiveRegionChangeOnFreshlyUnignoredNode) {
  RunEventTest(
      FILE_PATH_LITERAL("live-region-change-on-freshly-unignored-node.html"));
}

// TODO(crbug.com/40844027): Flaky on win
#if BUILDFLAG(IS_WIN)
#define MAYBE_AccessibilityEventsMenuListCollapse \
  DISABLED_AccessibilityEventsMenuListCollapse
#else
#define MAYBE_AccessibilityEventsMenuListCollapse \
  AccessibilityEventsMenuListCollapse
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsMenuListCollapse) {
  RunEventTest(FILE_PATH_LITERAL("menulist-collapse.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       // TODO(crbug.com/40924143): Re-enable this test
                       DISABLED_AccessibilityEventsMenuListCollapseNext) {
  RunEventTest(FILE_PATH_LITERAL("menulist-collapse-next.html"));
}

// TODO(crbug.com/40780161): Flaky on Linux and Win.
// TODO(crbug.com/40779330): locks up with popup open, only on Mac
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_AccessibilityEventsMenuListExpand \
  DISABLED_AccessibilityEventsMenuListExpand
#else
#define MAYBE_AccessibilityEventsMenuListExpand \
  AccessibilityEventsMenuListExpand
#endif

// TODO(crbug.com/40779330): locks up with popup open, only on Mac. Default
// action on selected HTML:option doesn't work, so no events are fired, and
// the test times out.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AccessibilityEventsMenuWithOptgroupListNext \
  DISABLED_AccessibilityEventsMenuWithOptgroupListNext
#else
#define MAYBE_AccessibilityEventsMenuWithOptgroupListNext \
  AccessibilityEventsMenuWithOptgroupListNext
#endif

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsMenuListExpand) {
  RunEventTest(FILE_PATH_LITERAL("menulist-expand.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsMenuListFocus) {
  RunEventTest(FILE_PATH_LITERAL("menulist-focus.html"));
}

// TODO(crbug.com/40841326): disabled on UIA
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTestExceptUIA,
                       // TODO(crbug.com/40913066): Re-enable this test
                       DISABLED_AccessibilityEventsMenuListNext) {
  RunEventTest(FILE_PATH_LITERAL("menulist-next.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTestExceptUIA,
                       MAYBE_AccessibilityEventsMenuWithOptgroupListNext) {
  RunEventTest(FILE_PATH_LITERAL("menulist-with-optgroup-next.html"));
}

// ---- Custom menulist tests ----

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsWithExperimentalWebFeaturesTest,
                       AccessibilityEventsMenuListCustomExpandCollapse) {
  RunEventTest(FILE_PATH_LITERAL("menulist-custom-expand-collapse.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsWithExperimentalWebFeaturesTest,
                       AccessibilityEventsMenuListCustomFocus) {
  RunEventTest(FILE_PATH_LITERAL("menulist-custom-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsWithExperimentalWebFeaturesTest,
                       AccessibilityEventsMenuListCustomNext) {
  RunEventTest(FILE_PATH_LITERAL("menulist-custom-next.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsMultipleAriaPropertiesChanged) {
  RunEventTest(FILE_PATH_LITERAL("multiple-aria-properties-changed.html"));
}

// Flaky on Windows: https://crbug.com/1078490.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AccessibilityEventsNameChange \
  DISABLED_AccessibilityEventsNameChange
#else
#define MAYBE_AccessibilityEventsNameChange AccessibilityEventsNameChange
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsNameChange) {
  RunEventTest(FILE_PATH_LITERAL("name-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsNameChangeIndirect) {
  RunEventTest(FILE_PATH_LITERAL("name-change-indirect.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsDocumentTitleChange) {
  RunEventTest(FILE_PATH_LITERAL("document-title-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsWithExperimentalWebFeaturesTest,
                       AccessibilityEventsNavigationApi) {
  RunEventTest(FILE_PATH_LITERAL("navigation-api.html"));
}

// TODO(crbug.com/40869309): Failing on linux/mac/win multiple builders.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_AccessibilityEventsImmediateRefresh \
  DISABLED_AccessibilityEventsImmediateRefresh
#else
#define MAYBE_AccessibilityEventsImmediateRefresh \
  AccessibilityEventsImmediateRefresh
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsWithExperimentalWebFeaturesTest,
                       MAYBE_AccessibilityEventsImmediateRefresh) {
  RunEventTest(FILE_PATH_LITERAL("immediate-refresh.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveChild) {
  RunEventTest(FILE_PATH_LITERAL("remove-child.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsReparentElementWithActiveDescendant) {
  RunEventTest(
      FILE_PATH_LITERAL("reparent-element-with-active-descendant.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveHiddenAttribute) {
  RunEventTest(FILE_PATH_LITERAL("remove-hidden-attribute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsRoleChanged) {
  RunEventTest(FILE_PATH_LITERAL("role-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsReportValidityInvalidField) {
  RunEventTest(FILE_PATH_LITERAL("report-validity-invalid-field.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveHiddenAttributeSubtree) {
  RunEventTest(FILE_PATH_LITERAL("remove-hidden-attribute-subtree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsSamePageLinkNavigation) {
#if BUILDFLAG(IS_WIN)
  if (!ui::BrowserAccessibilityManagerWin::
          IsUiaActiveTextPositionChangedEventSupported()) {
    return;
  }
#endif
  RunEventTest(FILE_PATH_LITERAL("same-page-link-navigation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsScrollHorizontalScrollPercentChange) {
  RunEventTest(
      FILE_PATH_LITERAL("scroll-horizontal-scroll-percent-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsScrollVerticalScrollPercentChange) {
  RunEventTest(FILE_PATH_LITERAL("scroll-vertical-scroll-percent-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsStyleChanged) {
  RunEventTest(FILE_PATH_LITERAL("style-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsSubtreeReparentedIgnoredChanged) {
  RunEventTest(FILE_PATH_LITERAL("subtree-reparented-ignored-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsSubtreeReparentedViaAppendChild) {
  RunEventTest(FILE_PATH_LITERAL("subtree-reparented-via-append-child.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsSubtreeReparentedViaAriaOwns) {
  RunEventTest(FILE_PATH_LITERAL("subtree-reparented-via-aria-owns.html"));
}
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsSubtreeReparentedViaAriaOwns2) {
  RunEventTest(FILE_PATH_LITERAL("subtree-reparented-via-aria-owns-2.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTabindexAddedOnPlainDiv) {
  RunEventTest(FILE_PATH_LITERAL("tabindex-added-on-plain-div.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTabindexAddedOnAriaHidden) {
  RunEventTest(FILE_PATH_LITERAL("tabindex-added-on-aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTabindexRemovedOnPlainDiv) {
  RunEventTest(FILE_PATH_LITERAL("tabindex-removed-on-plain-div.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTabindexRemovedOnAriaHidden) {
  RunEventTest(FILE_PATH_LITERAL("tabindex-removed-on-aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveSubtree) {
  RunEventTest(FILE_PATH_LITERAL("remove-subtree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextAlignChanged) {
  RunEventTest(FILE_PATH_LITERAL("text-align-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextChanged) {
  RunEventTest(FILE_PATH_LITERAL("text-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextChangedContentEditable) {
  RunEventTest(FILE_PATH_LITERAL("text-changed-contenteditable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextIndentChanged) {
  RunEventTest(FILE_PATH_LITERAL("text-indent-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextSelectionChanged) {
  RunEventTest(FILE_PATH_LITERAL("text-selection-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextSelectionInsideHiddenElement) {
  RunEventTest(FILE_PATH_LITERAL("text-selection-inside-hidden-element.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextSelectionInsideVideo) {
  RunEventTest(FILE_PATH_LITERAL("text-selection-inside-video.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaCheckedChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-checked-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaMultiselectableChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-multiselectable-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaRequiredChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-required-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaPressedChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-pressed-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTheadFocus) {
  RunEventTest(FILE_PATH_LITERAL("thead-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTfootFocus) {
  RunEventTest(FILE_PATH_LITERAL("tfoot-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsTbodyFocus) {
  RunEventTest(FILE_PATH_LITERAL("tbody-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsVisibilityHiddenChanged) {
  RunEventTest(FILE_PATH_LITERAL("visibility-hidden-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSelectedChanged) {
  RunEventTest(FILE_PATH_LITERAL("aria-selected-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaSelectedChangedNewSubtree) {
  RunEventTest(FILE_PATH_LITERAL("aria-selected-changed-new-subtree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsButtonClick) {
  RunEventTest(FILE_PATH_LITERAL("button-click.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsButtonRemoveChildren) {
  RunEventTest(FILE_PATH_LITERAL("button-remove-children.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       RangeValueIsReadonlyChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-is-readonly-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, RangeValueMaximumChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-maximum-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, RangeValueMinimumChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-minimum-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, RangeValueStepChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-step-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, RangeValueValueChanged) {
  RunEventTest(FILE_PATH_LITERAL("range-value-value-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, ValueIsReadOnlyChanged) {
  RunEventTest(FILE_PATH_LITERAL("value-is-readonly-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, ValueValueChanged) {
  RunEventTest(FILE_PATH_LITERAL("value-value-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsMenuOpenedClosed) {
  RunEventTest(FILE_PATH_LITERAL("menu-opened-closed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsMenubarShowHideMenus) {
  RunEventTest(FILE_PATH_LITERAL("menubar-show-hide-menus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaFlowToChange) {
  RunEventTest(FILE_PATH_LITERAL("aria-flow-to.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest,
                       AccessibilityEventsSelectAddRemove) {
  RunEventTest(FILE_PATH_LITERAL("select-selected-add-remove.html"));
}

// Test is flaky on Linux. See crbug.com/990847 for more details.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DeleteSubtree DISABLED_DeleteSubtree
#else
#define MAYBE_DeleteSubtree DeleteSubtree
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, MAYBE_DeleteSubtree) {
  RunEventTest(FILE_PATH_LITERAL("delete-subtree.html"));
}

}  // namespace content
