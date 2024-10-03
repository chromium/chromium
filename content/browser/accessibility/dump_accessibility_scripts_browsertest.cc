// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_script_instruction.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace content {

using ui::AXPropertyFilter;
using ui::AXScriptInstruction;
using ui::AXTreeFormatter;

#if BUILDFLAG(IS_MAC)

constexpr const char kMacAction[]{"mac/action"};
constexpr const char kMacAttributedString[]{"mac/attributed-string"};
constexpr const char kMacAttributes[]{"mac/attributes"};
constexpr const char kMacDescription[]{"mac/description"};
constexpr const char kMacSelection[]{"mac/selection"};
constexpr const char kMacTextMarker[]{"mac/textmarker"};
constexpr const char kMacMethods[]{"mac/methods"};
constexpr const char kMacParameterizedAttributes[]{
    "mac/parameterized-attributes"};

#endif

#if BUILDFLAG(IS_WIN)

constexpr const char kIAccessible[]{"win/ia2/iaccessible"};
constexpr const char kIAccessible2[]{"win/ia2/iaccessible2"};
constexpr const char kIAccessibleTable[]{"win/ia2/iaccessibletable"};
constexpr const char kIAccessibleTextSelectionContainer[]{
    "win/ia2/iaccessibletextselectioncontainer"};

#endif

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
  DumpAccessibilityScriptTest() {
    // Drop 'mac' expectations qualifier both from expectation file names and
    // from scenario directives.
    test_helper_.OverrideExpectationType("content");
  }
  ~DumpAccessibilityScriptTest() = default;
  DumpAccessibilityScriptTest(const DumpAccessibilityScriptTest&) = delete;
  DumpAccessibilityScriptTest& operator=(const DumpAccessibilityScriptTest&) =
      delete;

#if BUILDFLAG(IS_MAC)
  template <const char* type>
  void Migration_RunTypedTest(const base::FilePath::CharType* file_path) {
    if (features::IsMacAccessibilityAPIMigrationEnabled()) {
      RunTypedTest<type>(file_path);
    } else {
      GTEST_SKIP();
    }
  }
#endif

 protected:
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    return {};
  }

  void AddPropertyFilter(
      std::vector<AXPropertyFilter>* property_filters,
      const std::string& filter,
      AXPropertyFilter::Type type = AXPropertyFilter::ALLOW) {
    property_filters->push_back(AXPropertyFilter(filter, type));
  }

  std::vector<std::string> Dump(ui::AXMode mode) override {
    std::vector<std::string> dump;
    std::unique_ptr<AXTreeFormatter> formatter(CreateFormatter());
    ui::BrowserAccessibility* root =
        GetManager()->GetBrowserAccessibilityRoot();

    size_t start_index = 0;
    size_t length = scenario_.script_instructions.size();
    while (start_index < length) {
      std::string wait_for;
      std::string dom_key_string;
      bool printTree = false;
      size_t index = start_index;
      for (; index < length; index++) {
        const AXScriptInstruction& instruction =
            scenario_.script_instructions[index];
        if (instruction.IsEvent()) {
          wait_for = instruction.AsEvent();
          break;
        }
        if (instruction.IsKeyEvent()) {
          dom_key_string = instruction.AsDomKeyString();
          break;
        }
        if (instruction.IsPrintTree()) {
          printTree = true;
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
                           scenario_.script_instructions, start_index, index),
            ui::kAXModeComplete);
        actual_contents = pair.first.ExtractString();
        for (auto event : pair.second) {
          if (base::StartsWith(event, wait_for)) {
            actual_contents += event + '\n';
          }
        }
      }

      if (!dom_key_string.empty()) {
        ui::DomKey dom_key =
            ui::KeycodeConverter::KeyStringToDomKey(dom_key_string);
        if (dom_key != ui::DomKey::NONE) {
          ui::DomCode dom_code =
              ui::KeycodeConverter::CodeStringToDomCode(dom_key_string);
          SimulateKeyPress(GetWebContents(), dom_key, dom_code,
                           ui::DomCodeToUsLayoutKeyboardCode(dom_code),
                           /* control */ false, /* shift */ false,
                           /* alt */ false, /* command */ false);
        }
        actual_contents += "press " + dom_key_string + '\n';
        RunUntilInputProcessed(GetWidgetHost());

        // Input presses could create a11y events. Wait for those to clear
        // before procceding.
        WaitForEndOfTest(mode);
      }
      if (printTree) {
        actual_contents += DumpTreeAsString() + '\n';
      }

      auto chunk =
          base::SplitString(actual_contents, "\n", base::KEEP_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
      dump.insert(dump.end(), chunk.begin(), chunk.end());

      start_index = index + 1;
    }
    return dump;
  }

  EvalJsResult EvaluateScript(
      AXTreeFormatter* formatter,
      ui::BrowserAccessibility* root,
      const std::vector<AXScriptInstruction>& instructions,
      size_t start_index,
      size_t end_index) {
    return EvalJsResult(/*value=*/base::Value(formatter->EvaluateScript(
                            root, instructions, start_index, end_index)),
                        /*error=*/"");
  }

  RenderWidgetHost* GetWidgetHost() {
    return GetWebContents()
        ->GetPrimaryMainFrame()
        ->GetRenderViewHost()
        ->GetWidget();
  }
};

// Parameterize the tests so that each test-pass is run independently.
struct TestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<ui::AXApiType::Type>& i) const {
    return std::string(i.param);
  }
};

//
// Scripting supported on Mac only.
//

#if BUILDFLAG(IS_MAC)

INSTANTIATE_TEST_SUITE_P(All,
                         DumpAccessibilityScriptTest,
                         ::testing::Values(ui::AXApiType::kMac),
                         TestPassToString());

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXAccessKey) {
  RunTypedTest<kMacAttributes>("ax-access-key.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIAAtomic) {
  RunTypedTest<kMacAttributes>("ax-aria-atomic.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIABusy) {
  RunTypedTest<kMacAttributes>("ax-aria-busy.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIAColumnCount) {
  RunTypedTest<kMacAttributes>("ax-aria-column-count.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIAColumnIndex) {
  RunTypedTest<kMacAttributes>("ax-aria-column-index.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIACurrent) {
  RunTypedTest<kMacAttributes>("ax-aria-current.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIALive) {
  RunTypedTest<kMacAttributes>("ax-aria-live.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIASetSize) {
  RunTypedTest<kMacAttributes>("ax-aria-set-size.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIAPosInSet) {
  RunTypedTest<kMacAttributes>("ax-aria-pos-in-set.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIARelevant) {
  RunTypedTest<kMacAttributes>("ax-aria-relevant.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIARowCount) {
  RunTypedTest<kMacAttributes>("ax-aria-row-count.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXARIARowIndex) {
  RunTypedTest<kMacAttributes>("ax-aria-row-index.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXAutocompleteValue) {
  RunTypedTest<kMacAttributes>("ax-autocomplete-value.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXBlockQuoteLevel) {
  RunTypedTest<kMacAttributes>("ax-block-quote-level.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXColumnHeaderUIElements) {
  RunTypedTest<kMacAttributes>("ax-column-header-ui-elements.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXDescription) {
  RunTypedTest<kMacAttributes>("ax-description.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXDetailsElements) {
  RunTypedTest<kMacAttributes>("ax-details-elements.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXDOMClassList) {
  RunTypedTest<kMacAttributes>("ax-dom-class-list.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXDOMIdentifier) {
  RunTypedTest<kMacAttributes>("ax-dom-identifier.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXEditableAncestor) {
  RunTypedTest<kMacAttributes>("ax-editable-ancestor.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXElementBusy) {
  RunTypedTest<kMacAttributes>("ax-element-busy.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXFocusableAncestor) {
  RunTypedTest<kMacAttributes>("ax-focusable-ancestor.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXHasPopup) {
  RunTypedTest<kMacAttributes>("ax-has-popup.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXHelp) {
  RunTypedTest<kMacAttributes>("ax-help.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXHighestEditableAncestor) {
  RunTypedTest<kMacAttributes>("ax-highest-editable-ancestor.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AXInsertionPointLineNumber) {
  RunTypedTest<kMacAttributes>("ax-insertion-point-line-number.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXInvalid) {
  RunTypedTest<kMacAttributes>("ax-invalid.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXIsMultiSelectable) {
  RunTypedTest<kMacAttributes>("ax-is-multi-selectable.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXKeyShortcutsValue) {
  RunTypedTest<kMacAttributes>("ax-key-shortcuts-value.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXLoaded) {
  RunTypedTest<kMacAttributes>("ax-loaded.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXLoadingProgress) {
  RunTypedTest<kMacAttributes>("ax-loading-progress.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathBase) {
  RunTypedTest<kMacAttributes>("ax-math-base.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathFractionDenominator) {
  RunTypedTest<kMacAttributes>("ax-math-fraction-denominator.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathFractionNumerator) {
  RunTypedTest<kMacAttributes>("ax-math-fraction-numerator.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathOver) {
  RunTypedTest<kMacAttributes>("ax-math-over.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathPoscripts) {
  RunTypedTest<kMacAttributes>("ax-math-postscripts.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathPrescripts) {
  RunTypedTest<kMacAttributes>("ax-math-prescripts.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathRootIndex) {
  RunTypedTest<kMacAttributes>("ax-math-root-index.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathRootRadicand) {
  RunTypedTest<kMacAttributes>("ax-math-root-radicand.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathSubscript) {
  RunTypedTest<kMacAttributes>("ax-math-subscript.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathSuperscript) {
  RunTypedTest<kMacAttributes>("ax-math-superscript.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXMathUnder) {
  RunTypedTest<kMacAttributes>("ax-math-under.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXOwns) {
  RunTypedTest<kMacAttributes>("ax-owns.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXPopupValue) {
  RunTypedTest<kMacAttributes>("ax-popup-value.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXPressButton) {
  RunTypedTest<kMacAction>("ax-press-button.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXRequired) {
  RunTypedTest<kMacAttributes>("ax-required.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXRows) {
  RunTypedTest<kMacAttributes>("ax-rows.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXSelected) {
  RunTypedTest<kMacAttributes>("ax-selected.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXSelectedChildren) {
  RunTypedTest<kMacAttributes>("ax-selected-children.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXSelectedRows) {
  RunTypedTest<kMacAttributes>("ax-selected-rows.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXSize) {
  RunTypedTest<kMacAttributes>("ax-size.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXTitleUIElement) {
  RunTypedTest<kMacAttributes>("ax-title-ui-element.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXTitle) {
  RunTypedTest<kMacAttributes>("ax-title.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXURL) {
  RunTypedTest<kMacAttributes>("ax-url.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXVisited) {
  RunTypedTest<kMacAttributes>("ax-visited.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AttributedStringDeletion) {
  RunTypedTest<kMacAttributedString>("deletion.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AttributedStringInsertion) {
  RunTypedTest<kMacAttributedString>("insertion.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AttributedStringMark) {
  RunTypedTest<kMacAttributedString>("mark.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AttributedStringNestedAnnotation) {
  RunTypedTest<kMacAttributedString>(
      "nested-suggestion-insertion-deletion.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AttributedStringSuggestion) {
  RunTypedTest<kMacAttributedString>("suggestion.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AttributedStringBold) {
  RunTypedTest<kMacAttributedString>("bold.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AttributedStringItalic) {
  RunTypedTest<kMacAttributedString>("italic.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AttributedStringStrikethrough) {
  RunTypedTest<kMacAttributedString>("strikethrough.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AttributedStringUnderline) {
  RunTypedTest<kMacAttributedString>("underline.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, ChromeAXNodeId) {
  RunTypedTest<kMacAttributes>("chrome-ax-node-id.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AriaDescription) {
  RunTypedTest<kMacDescription>("aria-description-in-axcustomcontent.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, SelectAllTextarea) {
  RunTypedTest<kMacSelection>("selectall-textarea.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       SetSelectionContenteditable) {
  RunTypedTest<kMacSelection>("set-selection-contenteditable.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, SetSelectionTextarea) {
  RunTypedTest<kMacSelection>("set-selection-textarea.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       SetSelectedTextRangeContenteditable) {
  RunTypedTest<kMacSelection>("set-selectedtextrange-contenteditable.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AXNextWordEndTextMarkerForTextMarker) {
  RunTypedTest<kMacTextMarker>(
      "ax-next-word-end-text-marker-for-text-marker.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AXPreviousWordStartTextMarkerForTextMarker) {
  RunTypedTest<kMacTextMarker>(
      "ax-previous-word-start-text-marker-for-text-marker.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXStartTextMarker) {
  RunTypedTest<kMacTextMarker>("ax-start-text-marker.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AXTextMarkerRangeForUIElement) {
  RunTypedTest<kMacTextMarker>("ax-text-marker-range-for-ui-element.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AccessibilityColumnHeaderUIElements) {
  RunTypedTest<kMacMethods>("accessibility-column-header-ui-elements.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AccessibilityCustomContent) {
  RunTypedTest<kMacMethods>("accessibility-custom-content.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AccessibilityIsIgnored) {
  RunTypedTest<kMacMethods>("accessibility-is-ignored.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AccessibilityLabel) {
  RunTypedTest<kMacMethods>("accessibility-label.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AccessibilityPlaceholderValue) {
  RunTypedTest<kMacMethods>("accessibility-placeholder-value.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AccessibilityRoleDescription) {
  RunTypedTest<kMacMethods>("accessibility-role-description.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AccessibilityRows) {
  RunTypedTest<kMacMethods>("accessibility-rows.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AccessibilitySelected) {
  RunTypedTest<kMacMethods>("accessibility-selected.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AccessibilitySelectedRows) {
  RunTypedTest<kMacMethods>("accessibility-selected-rows.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AccessibilityTitleUIElement) {
  RunTypedTest<kMacMethods>("accessibility-title-ui-element.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AccessibilityTitle) {
  RunTypedTest<kMacMethods>("accessibility-title.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AccessibilityURL) {
  RunTypedTest<kMacMethods>("accessibility-url.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, IsAccessibilityElement) {
  RunTypedTest<kMacMethods>("is-accessibility-element.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, IsAccessibilityFocused) {
  Migration_RunTypedTest<kMacMethods>("is-accessibility-focused.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, SetAccessibilityFocused) {
  RunTypedTest<kMacMethods>("set-accessibility-focused.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, IsAccessibilityDisclosed) {
  Migration_RunTypedTest<kMacMethods>("is-accessibility-disclosed.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AccessibilityDisclosedByRow) {
  Migration_RunTypedTest<kMacMethods>("accessibility-disclosed-by-row.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AccessibilityDisclosedRows) {
  Migration_RunTypedTest<kMacMethods>("accessibility-disclosed-rows.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AccessibilityDisclosureLevel) {
  Migration_RunTypedTest<kMacMethods>("accessibility-disclosure-level.html");
}

// Parameterized attributes

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AXAttributedStringForRange) {
  RunTypedTest<kMacParameterizedAttributes>(
      "ax-attributed-string-for-range.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       AXAttributedStringForTextMarkerRange) {
  RunTypedTest<kMacParameterizedAttributes>(
      "ax-attributed-string-for-text-marker-range.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, AXStringForRange) {
  RunTypedTest<kMacParameterizedAttributes>("ax-string-for-range.html");
}

#endif

#if BUILDFLAG(IS_WIN)

INSTANTIATE_TEST_SUITE_P(All,
                         DumpAccessibilityScriptTest,
                         ::testing::Values(ui::AXApiType::kWinIA2),
                         TestPassToString());

// IAccessible

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, IAccessibleRole) {
  RunTypedTest<kIAccessible>(L"iaccessible-role.html");
}

// IAccessible2

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest, IAccessible2Role) {
  RunTypedTest<kIAccessible2>(L"iaccessible2-role.html");
}

// IAccessibleTable

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       IAccessibleTableSelectedColumns) {
  RunTypedTest<kIAccessibleTable>(L"iaccessibletable-selected-columns.html");
}

// IAccessibleTextSelectionContainer

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       IAccessibleTextSelectionContainerSelections) {
  RunTypedTest<kIAccessibleTextSelectionContainer>(
      L"iaccessibletextselectioncontainer-selections.html");
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityScriptTest,
                       IAccessibleTextSelectionContainerSetSelections) {
  RunTypedTest<kIAccessibleTextSelectionContainer>(
      L"iaccessibletextselectioncontainer-set-selections.html");
}

#endif

}  // namespace content
