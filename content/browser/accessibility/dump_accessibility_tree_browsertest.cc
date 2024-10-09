// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_tree_browsertest.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

// Tests that use @DEFAULT_ACTION-ON to open a popup for an <input> (such as
// color or date/time pickers) never complete on Android, where native pickers
// are used. Because that UI is native, it is not necessary to pass those tests.
#if BUILDFLAG(IS_ANDROID)
#define NOT_ANDROID(x) DISABLED_##x
#else
#define NOT_ANDROID(x) x
#endif

namespace content {

using ui::AXPropertyFilter;
using ui::AXTreeFormatter;

std::vector<ui::AXPropertyFilter> DumpAccessibilityTreeTest::DefaultFilters()
    const {
  std::vector<AXPropertyFilter> property_filters;
  if (GetParam() == ui::AXApiType::kMac) {
    return property_filters;
  }

  property_filters.emplace_back("value='*'", AXPropertyFilter::ALLOW);
  // The value attribute on the document object contains the URL of the
  // current page which will not be the same every time the test is run.
  property_filters.emplace_back("value='http*'", AXPropertyFilter::DENY);
  // Object attributes.value
  property_filters.emplace_back("layout-guess:*", AXPropertyFilter::ALLOW);

  property_filters.emplace_back("select*", AXPropertyFilter::ALLOW);
  property_filters.emplace_back("selectedFromFocus=*", AXPropertyFilter::DENY);
  property_filters.emplace_back("descript*", AXPropertyFilter::ALLOW);
  property_filters.emplace_back("check*", AXPropertyFilter::ALLOW);
  property_filters.emplace_back("horizontal", AXPropertyFilter::ALLOW);
  property_filters.emplace_back("multiselectable", AXPropertyFilter::ALLOW);
  property_filters.emplace_back("placeholder=*", AXPropertyFilter::ALLOW);
  property_filters.emplace_back("ispopup*", AXPropertyFilter::ALLOW);

  // Deny most empty values.
  property_filters.emplace_back("*=''", AXPropertyFilter::DENY);
  // After denying empty values, we need to add the following filter because we
  // want to allow name=''.
  property_filters.emplace_back("name=*", AXPropertyFilter::ALLOW_EMPTY);
  return property_filters;
}

void DumpAccessibilityTreeTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  DumpAccessibilityTestBase::SetUpCommandLine(command_line);
  // Enable KeyboardFocusableScrollers, used by AccessibilityScrollableOverflow.
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "KeyboardFocusableScrollers");
  // Enable AccessibilityAriaVirtualContent.
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "AccessibilityAriaVirtualContent");
  // Enable ComputedAccessibilityInfo.
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "ComputedAccessibilityInfo");
  // Enable accessibility object model, used in other tests.
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "AccessibilityObjectModel");
  // Enable aria-colindextext and aria-rowindextext.
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "AriaRowColIndexText");
  // Enable CSSReadingFlow, used by AccessibilityCSSReadingFlow.
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "CSSReadingFlow");
  // Enable CSSVisibilityInert, used by AccessibilityCSSVisibilityInert
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "CSSVisibilityInert");
  // Enable reference target
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "ShadowRootReferenceTarget");
}

std::vector<std::string> DumpAccessibilityTreeTest::Dump(ui::AXMode mode) {
  WaitForFinalTreeContents(mode);

  return base::SplitString(DumpTreeAsString(), "\n", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

void DumpAccessibilityTreeTest::ChooseFeatures(
    std::vector<base::test::FeatureRef>* enabled_features,
    std::vector<base::test::FeatureRef>* disabled_features) {
  // http://crbug.com/1063155 - temporary until this is enabled
  // everywhere.
  enabled_features->emplace_back(
      features::kEnableAccessibilityAriaVirtualContent);
  // crbug.com/339418716 - temporary until enabled by default
  enabled_features->emplace_back(blink::features::kPermissionElement);
  DumpAccessibilityTestBase::ChooseFeatures(enabled_features,
                                            disabled_features);
}

class DumpAccessibilityTreeTestExceptUIA : public DumpAccessibilityTreeTest {};

// Parameterize the tests so that each test-pass is run independently.
struct DumpAccessibilityTreeTestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<ui::AXApiType::Type>& i) const {
    return std::string(i.param);
  }
};

class YieldingParserDumpAccessibilityTreeTest
    : public DumpAccessibilityTreeTest {
 protected:
  YieldingParserDumpAccessibilityTreeTest() {
    feature_list_.InitWithFeatures(
        {{blink::features::kHTMLParserYieldAndDelayOftenForTesting}},
        {/* disabled_features */});
  }

  ~YieldingParserDumpAccessibilityTreeTest() override {
    // Ensure that the feature lists are destroyed in the same order they
    // were created in.
    scoped_feature_list_.Reset();
    feature_list_.Reset();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SummaryAsHeadingDumpAccessibilityTreeTest
    : public DumpAccessibilityTreeTest {
 protected:
  SummaryAsHeadingDumpAccessibilityTreeTest() {
    feature_list_.InitWithFeatures(
        {{features::kAccessibilityExposeSummaryAsHeading}},
        {/* disabled_features */});
  }

  ~SummaryAsHeadingDumpAccessibilityTreeTest() override {
    // Ensure that the feature lists are destroyed in the same order they
    // were created in.
    scoped_feature_list_.Reset();
    feature_list_.Reset();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/40925629): We need to create a way to incrementally
// enable and create UIA tests.
INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityTreeTest,
    ::testing::ValuesIn(DumpAccessibilityTestBase::TreeTestPasses()),
    DumpAccessibilityTreeTestPassToString());

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityTreeTestExceptUIA,
    ::testing::ValuesIn(DumpAccessibilityTestBase::TreeTestPassesExceptUIA()),
    DumpAccessibilityTreeTestPassToString());

INSTANTIATE_TEST_SUITE_P(
    All,
    YieldingParserDumpAccessibilityTreeTest,
    ::testing::ValuesIn(DumpAccessibilityTestBase::TreeTestPassesExceptUIA()),
    DumpAccessibilityTreeTestPassToString());

INSTANTIATE_TEST_SUITE_P(
    All,
    SummaryAsHeadingDumpAccessibilityTreeTest,
    ::testing::ValuesIn(DumpAccessibilityTestBase::TreeTestPasses()),
    DumpAccessibilityTreeTestPassToString());

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSAltText) {
  RunCSSTest(FILE_PATH_LITERAL("alt-text.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityCSSAltText) {
  RunCSSTest(FILE_PATH_LITERAL("alt-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAnchorPositioning) {
  RunCSSTest(FILE_PATH_LITERAL("anchor-positioning.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAnchorPositioningChanged) {
  RunCSSTest(FILE_PATH_LITERAL("anchor-positioning-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAnchorPositioningChangedSimple) {
  RunCSSTest(FILE_PATH_LITERAL("anchor-positioning-changed-simple.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAnchorPositioningMultipleAnchors) {
  RunCSSTest(FILE_PATH_LITERAL("anchor-positioning-multiple-anchors.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAnchorPositioningPositionFallback) {
  RunCSSTest(FILE_PATH_LITERAL("anchor-positioning-position-fallback.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAnchorPositioningWithoutDetails) {
  RunCSSTest(FILE_PATH_LITERAL("anchor-positioning-without-details.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSBackgroundColorTransparent) {
  RunCSSTest(FILE_PATH_LITERAL("background-color-transparent.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSBeforeAfterBlock) {
  RunCSSTest(FILE_PATH_LITERAL("before-after-block.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSBeforeAfterCode) {
  RunCSSTest(FILE_PATH_LITERAL("before-after-code.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSColor) {
  RunCSSTest(FILE_PATH_LITERAL("color.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSContentVisibilityAutoCrash) {
  RunCSSTest(FILE_PATH_LITERAL("content-visibility-auto-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSContentVisibilityAutoAriaHidden) {
  RunCSSTest(FILE_PATH_LITERAL("content-visibility-auto-aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityCSSContentVisibilityAutoAriaHidden) {
  RunCSSTest(FILE_PATH_LITERAL("content-visibility-auto-aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSContentVisibilityHiddenCheckFailure) {
  RunCSSTest(FILE_PATH_LITERAL("content-visibility-hidden-check-failure.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSContentVisibilityToHidden) {
  RunCSSTest(FILE_PATH_LITERAL("content-visibility-to-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSCounterText) {
  RunCSSTest(FILE_PATH_LITERAL("counter-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSDisplayContents) {
  RunCSSTest(FILE_PATH_LITERAL("display-contents.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityCSSDisplayContents) {
  RunCSSTest(FILE_PATH_LITERAL("display-contents.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSFontStyle) {
  RunCSSTest(FILE_PATH_LITERAL("font-style.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSFontFamily) {
  RunCSSTest(FILE_PATH_LITERAL("font-family.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSFontSize) {
  RunCSSTest(FILE_PATH_LITERAL("font-size.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSHeadStyleScriptDisplayBlock) {
  RunCSSTest(FILE_PATH_LITERAL("head-style-script-display-block.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSHeadStyleScriptContentVisibilityHidden) {
  RunCSSTest(
      FILE_PATH_LITERAL("head-style-script-content-visibility-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityCSSHeadStyleScriptContentVisibilityHidden) {
  RunCSSTest(
      FILE_PATH_LITERAL("head-style-script-content-visibility-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSDisplayListItem) {
  RunCSSTest(FILE_PATH_LITERAL("display-list-item.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSDisplayNone) {
  RunCSSTest(FILE_PATH_LITERAL("display-none.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSDisplayTablePseudoElements) {
  RunCSSTest(FILE_PATH_LITERAL("display-table-pseudo-elements.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSDisplayToNone) {
  RunCSSTest(FILE_PATH_LITERAL("display-to-none.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSDisplayToInline) {
  RunCSSTest(FILE_PATH_LITERAL("display-to-inline.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSDisplayToBlock) {
  RunCSSTest(FILE_PATH_LITERAL("display-to-block.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSMarkerHyphens) {
  RunCSSTest(FILE_PATH_LITERAL("marker-hyphens.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSMarkerCrash) {
  RunCSSTest(FILE_PATH_LITERAL("marker-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSTextOverflowEllipsis) {
  RunCSSTest(FILE_PATH_LITERAL("text-overflow-ellipsis.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSFirstLetter) {
  RunCSSTest(FILE_PATH_LITERAL("first-letter.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSInlinePositionRelative) {
  RunCSSTest(FILE_PATH_LITERAL("inline-position-relative.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityCSSInlinePositionRelative) {
  RunCSSTest(FILE_PATH_LITERAL("inline-position-relative.html"));
}
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSIframeHidden) {
  RunCSSTest(FILE_PATH_LITERAL("iframe-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSLanguage) {
  RunCSSTest(FILE_PATH_LITERAL("language.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSPseudoElements) {
  RunCSSTest(FILE_PATH_LITERAL("pseudo-elements.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSPseudoElementsSeparatedByWhitespace) {
  RunCSSTest(FILE_PATH_LITERAL("pseudo-elements-separated-by-whitespace.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityCSSPseudoElementsSeparatedByWhitespace) {
  RunCSSTest(FILE_PATH_LITERAL("pseudo-elements-separated-by-whitespace.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSPseudoElementsIgnoredParent) {
  RunCSSTest(FILE_PATH_LITERAL("pseudo-elements-ignored-parent.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSPseudoElementAlternativeText) {
  RunCSSTest(FILE_PATH_LITERAL("pseudo-element-alternative-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSPseudoElementHighlight) {
  RunCSSTest(FILE_PATH_LITERAL("pseudo-element-highlight.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSPseudoElementPositioned) {
  RunCSSTest(FILE_PATH_LITERAL("pseudo-element-positioned.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSDOMElements) {
  RunCSSTest(FILE_PATH_LITERAL("dom-element-css-alternative-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSTableIncomplete) {
  RunCSSTest(FILE_PATH_LITERAL("table-incomplete.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSTableCellBadParent) {
  RunCSSTest(FILE_PATH_LITERAL("table-cell-bad-parent.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSTableDisplay) {
  RunCSSTest(FILE_PATH_LITERAL("table-display.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSTableDisplayOther) {
  RunCSSTest(FILE_PATH_LITERAL("table-display-other.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSDataTableDisplayOther) {
  RunCSSTest(FILE_PATH_LITERAL("table-data-display-other.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomRowElement) {
  RunCSSTest(FILE_PATH_LITERAL("table-custom-row-element.html"));
}

// TODO(crbug.com/334802046): Enable the test after the issue is fixed.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityCSSTransform) {
  RunCSSTest(FILE_PATH_LITERAL("transform.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSVisibility) {
  RunCSSTest(FILE_PATH_LITERAL("visibility.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSVisibilityToHidden) {
  RunCSSTest(FILE_PATH_LITERAL("visibility-to-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSVisibilityToCollapsed) {
  RunCSSTest(FILE_PATH_LITERAL("visibility-to-collapsed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSVisibilityToVisible) {
  RunCSSTest(FILE_PATH_LITERAL("visibility-to-visible.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSReadingFlow) {
  RunCSSTest(FILE_PATH_LITERAL("reading-flow.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    AccessibilityCSSReadingFlowDisplayContentsMixedSiblings) {
  RunCSSTest(
      FILE_PATH_LITERAL("reading-flow-display-contents-mixed-siblings.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    AccessibilityCSSReadingFlowDisplayContentsUniformSiblings) {
  RunCSSTest(
      FILE_PATH_LITERAL("reading-flow-display-contents-uniform-siblings.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSReadingFlowDisplayContentsShadowHost) {
  RunCSSTest(
      FILE_PATH_LITERAL("reading-flow-display-contents-shadow-host.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSReadingFlowPseudoElements) {
  RunCSSTest(FILE_PATH_LITERAL("reading-flow-pseudo-elements.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSReadingFlowShadowDomSlot) {
  RunCSSTest(FILE_PATH_LITERAL("reading-flow-shadow-dom-slot.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSReadingFlowOutOfFlowPosition) {
  RunCSSTest(FILE_PATH_LITERAL("reading-flow-out-of-flow-position.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityA) {
  RunHtmlTest(FILE_PATH_LITERAL("a.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAbbr) {
  RunHtmlTest(FILE_PATH_LITERAL("abbr.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAbsoluteOffscreen) {
  RunHtmlTest(FILE_PATH_LITERAL("absolute-offscreen.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAccordion) {
  RunHtmlTest(FILE_PATH_LITERAL("accordion.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityActionVerbs) {
  RunHtmlTest(FILE_PATH_LITERAL("action-verbs.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityActions) {
  RunHtmlTest(FILE_PATH_LITERAL("actions.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAddress) {
  RunHtmlTest(FILE_PATH_LITERAL("address.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAnnotationRoles) {
  RunAriaTest(FILE_PATH_LITERAL("annotation-roles.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityArea) {
  RunHtmlTest(FILE_PATH_LITERAL("area.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAreaAlone) {
  RunHtmlTest(FILE_PATH_LITERAL("area-alone.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAreaCrash) {
  RunHtmlTest(FILE_PATH_LITERAL("area-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAreaSerializationCrash) {
  RunHtmlTest(FILE_PATH_LITERAL("area-serialization-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAreaWithAriaOwns) {
  RunHtmlTest(FILE_PATH_LITERAL("area-with-aria-owns.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAHrefUpdate) {
  RunHtmlTest(FILE_PATH_LITERAL("a-href-update.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAName) {
  RunHtmlTest(FILE_PATH_LITERAL("a-name.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityANameCalc) {
  RunHtmlTest(FILE_PATH_LITERAL("a-name-calc.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityANoText) {
  RunHtmlTest(FILE_PATH_LITERAL("a-no-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAOnclick) {
  RunHtmlTest(FILE_PATH_LITERAL("a-onclick.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityANestedStructure) {
  RunHtmlTest(FILE_PATH_LITERAL("a-nested-structure.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAIsInteresting) {
  RunHtmlTest(FILE_PATH_LITERAL("isInteresting.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityClickableAncestor) {
  RunHtmlTest(FILE_PATH_LITERAL("clickable-ancestor.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityFocusedButtonWithAriaOwns) {
  RunHtmlTest(FILE_PATH_LITERAL("focused-button-with-aria-owns.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityComboboxOptgroup) {
  RunHtmlTest(FILE_PATH_LITERAL("combobox-optgroup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityComboboxWithRedundantAriaRole) {
  RunHtmlTest(FILE_PATH_LITERAL("combobox-with-redundant-aria-role.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySlotDisplayContents) {
  RunHtmlTest(FILE_PATH_LITERAL("slot-display-contents.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgStyleElement) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-style-element.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaActivedescendant) {
  RunAriaTest(FILE_PATH_LITERAL("aria-activedescendant.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaActivedescendantTargetIdChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-activedescendant-target-id-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaActivedescendantReferenceTarget) {
  RunAriaTest(FILE_PATH_LITERAL("aria-activedescendant-reference-target.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    AccessibilityAriaActivedescendantReferenceTargetDynamic) {
  RunAriaTest(
      FILE_PATH_LITERAL("aria-activedescendant-reference-target-dynamic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaAlert) {
  RunAriaTest(FILE_PATH_LITERAL("aria-alert.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaAlertDialog) {
  RunAriaTest(FILE_PATH_LITERAL("aria-alertdialog.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaAnyUnignored) {
  RunAriaTest(FILE_PATH_LITERAL("aria-any-unignored.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaApplication) {
  RunAriaTest(FILE_PATH_LITERAL("aria-application.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaArticle) {
  RunAriaTest(FILE_PATH_LITERAL("aria-article.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaAtomic) {
  RunAriaTest(FILE_PATH_LITERAL("aria-atomic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaAutocomplete) {
  RunAriaTest(FILE_PATH_LITERAL("aria-autocomplete.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaBanner) {
  RunAriaTest(FILE_PATH_LITERAL("aria-banner.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaBlockquote) {
  RunAriaTest(FILE_PATH_LITERAL("aria-blockquote.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaBrailleLabel) {
  RunAriaTest(FILE_PATH_LITERAL("aria-braillelabel.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaBrailleRoleDescription) {
  RunAriaTest(FILE_PATH_LITERAL("aria-brailleroledescription.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaBusy) {
  RunAriaTest(FILE_PATH_LITERAL("aria-busy.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaButton) {
  RunAriaTest(FILE_PATH_LITERAL("aria-button.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaCaption) {
  RunAriaTest(FILE_PATH_LITERAL("aria-caption.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaCell) {
  RunAriaTest(FILE_PATH_LITERAL("aria-cell.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaCheckBox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-checkbox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaChecked) {
  RunAriaTest(FILE_PATH_LITERAL("aria-checked.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaCode) {
  RunAriaTest(FILE_PATH_LITERAL("aria-code.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaColAttr) {
  RunAriaTest(FILE_PATH_LITERAL("aria-col-attr.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaColRowIndex) {
  RunAriaTest(FILE_PATH_LITERAL("aria-col-row-index.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaColRowIndexUndefined) {
  RunAriaTest(FILE_PATH_LITERAL("aria-col-row-index-undefined.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaColRowIndexText) {
  RunAriaTest(FILE_PATH_LITERAL("aria-col-row-index-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaColumnHeader) {
  RunAriaTest(FILE_PATH_LITERAL("aria-columnheader.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaCombobox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-combobox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaComboboxDynamic) {
  RunAriaTest(FILE_PATH_LITERAL("aria-combobox-dynamic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaComboboxImplicitHasPopup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-combobox-implicit-haspopup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityComboboxAriaHaspopupChanged) {
  RunRegressionTest(FILE_PATH_LITERAL("combobox-aria-haspopup-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaComboboxUneditable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-combobox-uneditable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOnePointOneCombobox) {
  RunAriaTest(FILE_PATH_LITERAL("aria1.1-combobox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaComment) {
  RunAriaTest(FILE_PATH_LITERAL("aria-comment.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaComplementary) {
  RunAriaTest(FILE_PATH_LITERAL("aria-complementary.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaContentInfo) {
  RunAriaTest(FILE_PATH_LITERAL("aria-contentinfo.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityContinuations) {
  RunHtmlTest(FILE_PATH_LITERAL("continuations.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContinuationsParserSplitsMarkup) {
  RunHtmlTest(FILE_PATH_LITERAL("continuations-parser-splits-markup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaControls) {
  RunAriaTest(FILE_PATH_LITERAL("aria-controls.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaControlsTargetIdChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-controls-target-id-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaControlsReferenceTarget) {
  RunAriaTest(FILE_PATH_LITERAL("aria-controls-reference-target.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaControlsManyParagraphsBetween) {
  RunAriaTest(FILE_PATH_LITERAL("aria-controls-many-paragraphs-between.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaCurrent) {
  RunAriaTest(FILE_PATH_LITERAL("aria-current.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDefinition) {
  RunAriaTest(FILE_PATH_LITERAL("aria-definition.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaDescribedBy) {
  RunAriaTest(FILE_PATH_LITERAL("aria-describedby.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaDescribedByTargetIdChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-describedby-target-id-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaDescribedByReferenceTarget) {
  RunAriaTest(FILE_PATH_LITERAL("aria-describedby-reference-target.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaDescribedByReferenceTargetDynamic) {
  RunAriaTest(
      FILE_PATH_LITERAL("aria-describedby-reference-target-dynamic.html"));
}

// TODO(crbug.com/40853220): disabled on UIA
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTestExceptUIA,
                       AccessibilityAriaDescribedByUpdates) {
  RunAriaTest(FILE_PATH_LITERAL("aria-describedby-updates.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaDescription) {
  RunAriaTest(FILE_PATH_LITERAL("aria-description.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDetails) {
  RunAriaTest(FILE_PATH_LITERAL("aria-details.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaDetailsTargetIdChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-details-target-id-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaDetailsReferenceTarget) {
  RunAriaTest(FILE_PATH_LITERAL("aria-details-reference-target.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaDetailsMultiple) {
  RunAriaTest(FILE_PATH_LITERAL("aria-details-multiple.html"));
}

// TODO(crbug.com/40842838): disabled on UIA
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTestExceptUIA,
                       AccessibilityAriaDetailsRoles) {
  RunAriaTest(FILE_PATH_LITERAL("aria-details-roles.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDialog) {
  RunAriaTest(FILE_PATH_LITERAL("aria-dialog.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDirectory) {
  RunAriaTest(FILE_PATH_LITERAL("aria-directory.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaDirectoryChildren) {
  RunAriaTest(FILE_PATH_LITERAL("aria-directory-children.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDisabled) {
  RunAriaTest(FILE_PATH_LITERAL("aria-disabled.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDocument) {
  RunAriaTest(FILE_PATH_LITERAL("aria-document.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaEmphasis) {
  RunAriaTest(FILE_PATH_LITERAL("aria-emphasis.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaErrorMessage) {
  RunAriaTest(FILE_PATH_LITERAL("aria-errormessage.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaErrorMessageTargetIdChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-errormessage-target-id-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaErrorMessageReferenceTarget) {
  RunAriaTest(FILE_PATH_LITERAL("aria-errormessage-reference-target.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaExpanded) {
  RunAriaTest(FILE_PATH_LITERAL("aria-expanded.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaExpandedRolesSupported) {
  RunAriaTest(FILE_PATH_LITERAL("aria-expanded-roles-supported.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaFeed) {
  RunAriaTest(FILE_PATH_LITERAL("aria-feed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaFigure) {
  RunAriaTest(FILE_PATH_LITERAL("aria-figure.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaFocusableSubwidgetNotEditable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-focusable-subwidget-not-editable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaHasPopup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-haspopup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaHeading) {
  RunAriaTest(FILE_PATH_LITERAL("aria-heading.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaHidden) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenOnBody) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-body.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenChanged) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenChangedOnButtons) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-changed-on-buttons.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenDescendants) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenOnHTML) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-html.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenLineBreakingObjects) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-line-breaking-objects.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenSingleDescendant) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-single-descendant.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenSingleDescendantDisplayNone) {
  RunAriaTest(
      FILE_PATH_LITERAL("aria-hidden-single-descendant-display-none.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    AccessibilityAriaHiddenSingleDescendantVisibilityHidden) {
  RunAriaTest(FILE_PATH_LITERAL(
      "aria-hidden-single-descendant-visibility-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenDescendantTabindexChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-descendant-tabindex-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenDescribedBy) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-described-by.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenFocusCorrections) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-focus-corrections.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenFocusedButton) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-focused-button.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenFocusedInput) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-focused-input.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenFocusedSpan) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-focused-span.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenLabelledBy) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-labelled-by.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenIframeBody) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-iframe-body.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenIframeBody) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-iframe-body.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenIframe) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-iframe.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaFlowto) {
  RunAriaTest(FILE_PATH_LITERAL("aria-flowto.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaFlowtoTargetIdChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-flowto-target-id-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaFlowtoReferenceTarget) {
  RunAriaTest(FILE_PATH_LITERAL("aria-flowto-reference-target.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaFlowtoMultiple) {
  RunAriaTest(FILE_PATH_LITERAL("aria-flowto-multiple.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaForm) {
  RunAriaTest(FILE_PATH_LITERAL("aria-form.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaGeneric) {
  RunAriaTest(FILE_PATH_LITERAL("aria-generic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaGlobal) {
  RunAriaTest(FILE_PATH_LITERAL("aria-global.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaGrid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaGridAriaSelected) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid-aria-selected.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaGridDynamicAddRow) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid-dynamic-add-row.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaGridExtraWrapElems) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid-extra-wrap-elems.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaGridCell) {
  RunAriaTest(FILE_PATH_LITERAL("aria-gridcell.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaGridCellFocusedOnly) {
  RunAriaTest(FILE_PATH_LITERAL("aria-gridcell-focused-only.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaGroup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-group.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaIllegalVal) {
  RunAriaTest(FILE_PATH_LITERAL("aria-illegal-val.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaImg) {
  RunAriaTest(FILE_PATH_LITERAL("aria-img.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaImgChild) {
  RunAriaTest(FILE_PATH_LITERAL("aria-img-child.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaInsertionDeletion) {
  RunAriaTest(FILE_PATH_LITERAL("aria-insertion-deletion.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaKeyShortcuts) {
  RunAriaTest(FILE_PATH_LITERAL("aria-keyshortcuts.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaLabel) {
  RunAriaTest(FILE_PATH_LITERAL("aria-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelAugmentInnerText) {
  RunAriaTest(FILE_PATH_LITERAL("aria-label-augment-inner-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelWithTabIndex) {
  RunAriaTest(FILE_PATH_LITERAL("aria-label-with-tabindex.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByRefersToSelf) {
  RunAriaTest(FILE_PATH_LITERAL("aria-labelledby-refers-to-self.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByHeading) {
  RunAriaTest(FILE_PATH_LITERAL("aria-labelledby-heading.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByTargetIdChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-labelledby-target-id-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByUpdates) {
  RunAriaTest(FILE_PATH_LITERAL("aria-labelledby-updates.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLeafInEditable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-leaf-in-editable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaLevel) {
  RunAriaTest(FILE_PATH_LITERAL("aria-level.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaLink) {
  RunAriaTest(FILE_PATH_LITERAL("aria-link.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaList) {
  RunAriaTest(FILE_PATH_LITERAL("aria-list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaListBox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxDisabled) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox-disabled.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxActiveDescendant) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox-activedescendant.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxAriaSelected) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox-aria-selected.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxChildFocus) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox-childfocus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaListItem) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listitem.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaLive) {
  RunAriaTest(FILE_PATH_LITERAL("aria-live.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaLiveNested) {
  RunAriaTest(FILE_PATH_LITERAL("aria-live-nested.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLiveWithContent) {
  RunAriaTest(FILE_PATH_LITERAL("aria-live-with-content.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaLog) {
  RunAriaTest(FILE_PATH_LITERAL("aria-log.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMain) {
  RunAriaTest(FILE_PATH_LITERAL("aria-main.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMarquee) {
  RunAriaTest(FILE_PATH_LITERAL("aria-marquee.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMenu) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menu.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMenuBar) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menubar.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMenuItem) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menuitem.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaMenuItemInGroup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menuitem-in-group.html"));
}
// crbug.com/442278 will stop creating new text elements representing title.
// Re-baseline after the Blink change goes in
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaMenuItemCheckBox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menuitemcheckbox.html"));
}

// crbug.com/442278 will stop creating new text elements representing title.
// Re-baseline after the Blink change goes in
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaMenuItemRadio) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menuitemradio.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMeter) {
  RunAriaTest(FILE_PATH_LITERAL("aria-meter.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaMismatchedTableAttr) {
  RunAriaTest(FILE_PATH_LITERAL("aria-mismatched-table-attr.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaModal) {
  RunAriaTest(FILE_PATH_LITERAL("aria-modal.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaModalFocusableDialog) {
  RunAriaTest(FILE_PATH_LITERAL("aria-modal-focusable-dialog.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaModalLayered) {
  RunAriaTest(FILE_PATH_LITERAL("aria-modal-layered.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaModalMoveFocus) {
  RunAriaTest(FILE_PATH_LITERAL("aria-modal-move-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaModalRemoveParentContainer) {
  RunAriaTest(FILE_PATH_LITERAL("aria-modal-remove-parent-container.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaModalUnhidden) {
  RunAriaTest(FILE_PATH_LITERAL("aria-modal-unhidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMultiline) {
  RunAriaTest(FILE_PATH_LITERAL("aria-multiline.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaMultiselectable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-multiselectable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaNavigation) {
  RunAriaTest(FILE_PATH_LITERAL("aria-navigation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaNote) {
  RunAriaTest(FILE_PATH_LITERAL("aria-note.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOrientation) {
  RunAriaTest(FILE_PATH_LITERAL("aria-orientation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaOwns) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaOwnsCrash) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaOwnsCrash2) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-crash-2.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsTargetIdChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-target-id-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsChildAndHiddenDescendantCrash) {
  RunFormControlsTest(
      FILE_PATH_LITERAL("aria-owns-child-and-hidden-descendant-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsFromTextarea) {
  RunRegressionTest(FILE_PATH_LITERAL("aria-owns-from-textarea.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaOwnsGrid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-grid.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTwoOwnersRemoveOne) {
  RunAriaTest(FILE_PATH_LITERAL("aria-two-owners-remove-one.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityAriaTwoOwnersRemoveOne) {
  RunAriaTest(FILE_PATH_LITERAL("aria-two-owners-remove-one.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsReparentAboveOtherChangeCrash) {
  RunRegressionTest(
      FILE_PATH_LITERAL("aria-owns-reparent-above-other-change-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsIgnored) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-ignored.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsIncludedInTree) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-included-in-tree.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsFromDisplayNone) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-from-display-none.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsFromDisplayNone) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-from-display-none.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsIllegalMultipleOwner) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-illegal-multiple-owner.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaOwnsList) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOwnsWithRoleChange) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-with-role-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMath) {
  RunAriaTest(FILE_PATH_LITERAL("aria-math.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaNone) {
  RunAriaTest(FILE_PATH_LITERAL("aria-none.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaOption) {
  RunAriaTest(FILE_PATH_LITERAL("aria-option.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaOptionComplexChildren) {
  RunAriaTest(FILE_PATH_LITERAL("aria-option-complex-children.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaParagraph) {
  RunAriaTest(FILE_PATH_LITERAL("aria-paragraph.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaPosinset) {
  RunAriaTest(FILE_PATH_LITERAL("aria-posinset.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaArticlePosInSetSetSize) {
  RunAriaTest(FILE_PATH_LITERAL("aria-article-posinset-setsize.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaPresentation) {
  RunAriaTest(FILE_PATH_LITERAL("aria-presentation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaPresentationInList) {
  RunAriaTest(FILE_PATH_LITERAL("aria-presentation-in-list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaPressed) {
  RunAriaTest(FILE_PATH_LITERAL("aria-pressed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaProgressbar) {
  RunAriaTest(FILE_PATH_LITERAL("aria-progressbar.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRadio) {
  RunAriaTest(FILE_PATH_LITERAL("aria-radio.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRadiogroup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-radiogroup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaRadioInShadowRoot) {
  RunAriaTest(FILE_PATH_LITERAL("aria-radio-in-shadow-root.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaReadonly) {
  RunAriaTest(FILE_PATH_LITERAL("aria-readonly.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRegion) {
  RunAriaTest(FILE_PATH_LITERAL("aria-region.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRelevant) {
  RunAriaTest(FILE_PATH_LITERAL("aria-relevant.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRequired) {
  RunAriaTest(FILE_PATH_LITERAL("aria-required.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaRequiredEmptyInput) {
  RunAriaTest(FILE_PATH_LITERAL("aria-required-empty-input.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaRoleDescription) {
  RunAriaTest(FILE_PATH_LITERAL("aria-roledescription.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRow) {
  RunAriaTest(FILE_PATH_LITERAL("aria-row.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRowAttr) {
  RunAriaTest(FILE_PATH_LITERAL("aria-row-attr.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRowGroup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-rowgroup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRowHeader) {
  RunAriaTest(FILE_PATH_LITERAL("aria-rowheader.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaRowText) {
  // RunAriaTestMinusHtmlMode ensures that the correct object attributes are
  // set even if html mode is not set.
  RunAriaTestMinusHtmlMode(FILE_PATH_LITERAL("aria-rowtext.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaScrollbar) {
  RunAriaTest(FILE_PATH_LITERAL("aria-scrollbar.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSearch) {
  RunAriaTest(FILE_PATH_LITERAL("aria-search.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSearchbox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-searchbox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaSearchboxWithSelection) {
  RunAriaTest(FILE_PATH_LITERAL("aria-searchbox-with-selection.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSelected) {
  RunAriaTest(FILE_PATH_LITERAL("aria-selected.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSeparator) {
  RunAriaTest(FILE_PATH_LITERAL("aria-separator.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSetsize) {
  RunAriaTest(FILE_PATH_LITERAL("aria-setsize.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaSetCountsWithHiddenItems) {
  RunAriaTest(FILE_PATH_LITERAL("aria-set-counts-with-hidden-items.html"));
}
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSlider) {
  RunAriaTest(FILE_PATH_LITERAL("aria-slider.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaSortOnAriaGrid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-sort-aria-grid.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaSetCountsWithTreeLevels) {
  RunAriaTest(FILE_PATH_LITERAL("aria-set-counts-with-tree-levels.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaSortOnHtmlTable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-sort-html-table.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSpinButton) {
  RunAriaTest(FILE_PATH_LITERAL("aria-spinbutton.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaStatus) {
  RunAriaTest(FILE_PATH_LITERAL("aria-status.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaStrong) {
  RunAriaTest(FILE_PATH_LITERAL("aria-strong.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSubscript) {
  RunAriaTest(FILE_PATH_LITERAL("aria-subscript.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaSuperscript) {
  RunAriaTest(FILE_PATH_LITERAL("aria-superscript.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSwitch) {
  RunAriaTest(FILE_PATH_LITERAL("aria-switch.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTab) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tab.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-table.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTabNestedInLists) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tab-nested-in-lists.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTabList) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tablist.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTabListAriaLevel) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tablist-aria-level.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTabPanel) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tabpanel.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTerm) {
  RunAriaTest(FILE_PATH_LITERAL("aria-term.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTextbox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-textbox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTextboxWithAriaTextboxChild) {
  RunAriaTest(FILE_PATH_LITERAL("aria-textbox-with-aria-textbox-child.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTextboxWithNonTextChildren) {
  RunAriaTest(FILE_PATH_LITERAL("aria-textbox-with-non-text-children.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTextboxWithRichText) {
  RunAriaTest(FILE_PATH_LITERAL("aria-textbox-with-rich-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTextboxWithSelection) {
  RunAriaTest(FILE_PATH_LITERAL("aria-textbox-with-selection.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTime) {
  RunAriaTest(FILE_PATH_LITERAL("aria-time.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTimer) {
  RunAriaTest(FILE_PATH_LITERAL("aria-timer.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaToggleButton) {
  RunAriaTest(FILE_PATH_LITERAL("aria-togglebutton.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaToolbar) {
  RunAriaTest(FILE_PATH_LITERAL("aria-toolbar.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTooltip) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tooltip.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTree) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTreeGrid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-treegrid.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTreeDiscontinuous) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tree-discontinuous.html"));
}

// https://crbug.com/367650908: flaky on Linux
#if BUILDFLAG(IS_LINUX)
#define MAYBE_AccessibilityAriaTreeitemNestedInLists \
  DISABLED_AccessibilityAriaTreeitemNestedInLists
#else
#define MAYBE_AccessibilityAriaTreeitemNestedInLists \
  AccessibilityAriaTreeitemNestedInLists
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaTreeitemNestedInLists) {
  RunAriaTest(FILE_PATH_LITERAL("aria-treeitem-nested-in-lists.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityDisabledAriaComboboxHasValue) {
  RunAriaTest(FILE_PATH_LITERAL("disabled-aria-combobox-has-value.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaUndefined) {
  RunAriaTest(FILE_PATH_LITERAL("aria-undefined.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaUndefinedLiteral) {
  RunAriaTest(FILE_PATH_LITERAL("aria-undefined-literal.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaEmptyString) {
  RunAriaTest(FILE_PATH_LITERAL("aria-empty-string.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaValueMin) {
  RunAriaTest(FILE_PATH_LITERAL("aria-valuemin.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaValueMax) {
  RunAriaTest(FILE_PATH_LITERAL("aria-valuemax.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaValueNow) {
  RunAriaTest(FILE_PATH_LITERAL("aria-valuenow.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaValueText) {
  RunAriaTest(FILE_PATH_LITERAL("aria-valuetext.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaVirtualContent) {
  RunAriaTest(FILE_PATH_LITERAL("aria-virtualcontent.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputTextARIAPlaceholder) {
  RunAriaTest(FILE_PATH_LITERAL("input-text-aria-placeholder.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableCanvasFallback) {
  RunHtmlTest(FILE_PATH_LITERAL("table-canvas-fallback.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityTableCanvasFallback) {
  RunHtmlTest(FILE_PATH_LITERAL("table-canvas-fallback.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTabletumnHidden) {
  RunAriaTest(FILE_PATH_LITERAL("table-column-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableColumnRemove) {
  RunHtmlTest(FILE_PATH_LITERAL("table-column-remove.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityLabelWithSelectedAriaOption) {
  RunAriaTest(FILE_PATH_LITERAL("label-with-selected-option.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityArticle) {
  RunHtmlTest(FILE_PATH_LITERAL("article.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAside) {
  RunHtmlTest(FILE_PATH_LITERAL("aside.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAsideInsideOtherSection) {
  RunHtmlTest(FILE_PATH_LITERAL("aside-inside-other-section.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAsideInsideSectionRoleGeneric) {
  RunHtmlTest(FILE_PATH_LITERAL("aside-inside-section-role-generic.html"));
}

// TODO(crbug.com/40943250): Fix failure on android
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_AccessibilityAudio DISABLED_AccessibilityAudio
#else
#define MAYBE_AccessibilityAudio AccessibilityAudio
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, MAYBE_AccessibilityAudio) {
  RunHtmlTest(FILE_PATH_LITERAL("audio.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAWithBefore) {
  RunHtmlTest(FILE_PATH_LITERAL("a-with-before.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAWithImg) {
  RunHtmlTest(FILE_PATH_LITERAL("a-with-img.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityB) {
  RunHtmlTest(FILE_PATH_LITERAL("b.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBase) {
  RunHtmlTest(FILE_PATH_LITERAL("base.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBdo) {
  RunHtmlTest(FILE_PATH_LITERAL("bdo.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityBdoTableCellFormControls) {
  RunFormControlsTest(FILE_PATH_LITERAL("bdo-table-cell.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBlockquote) {
  RunHtmlTest(FILE_PATH_LITERAL("blockquote.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityBlockquoteLevels) {
  RunHtmlTest(FILE_PATH_LITERAL("blockquote-levels.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBody) {
  RunHtmlTest(FILE_PATH_LITERAL("body.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBodyTabIndex) {
  RunHtmlTest(FILE_PATH_LITERAL("body-tabindex.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBoundsInherits) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-inherits.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBoundsClips) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-clips.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBoundsAbsolute) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-absolute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBoundsFixed) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-fixed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityBoundsFixedScrolling) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-fixed-scrolling.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBR) {
  RunHtmlTest(FILE_PATH_LITERAL("br.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBRWithChild) {
  RunHtmlTest(FILE_PATH_LITERAL("br-with-child.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityButton) {
  RunHtmlTest(FILE_PATH_LITERAL("button.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityButtonSubmit) {
  RunHtmlTest(FILE_PATH_LITERAL("button-submit.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityButtonAltChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("button-alt-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityButtonContentChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("button-content-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityButtonNameCalc) {
  RunHtmlTest(FILE_PATH_LITERAL("button-name-calc.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCanvas) {
  RunHtmlTest(FILE_PATH_LITERAL("canvas.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCanvasFallback) {
  RunHtmlTest(FILE_PATH_LITERAL("canvas-fallback.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityCanvasFallback) {
  RunHtmlTest(FILE_PATH_LITERAL("canvas-fallback.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCaption) {
  RunHtmlTest(FILE_PATH_LITERAL("caption.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCharacterLocations) {
  RunHtmlTest(FILE_PATH_LITERAL("character-locations.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCheckboxNameCalc) {
  RunHtmlTest(FILE_PATH_LITERAL("checkbox-name-calc.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCite) {
  RunHtmlTest(FILE_PATH_LITERAL("cite.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCode) {
  RunHtmlTest(FILE_PATH_LITERAL("code.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCol) {
  RunHtmlTest(FILE_PATH_LITERAL("col.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityColgroup) {
  RunHtmlTest(FILE_PATH_LITERAL("colgroup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomSelectSimple) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  RunHtmlTest(FILE_PATH_LITERAL("custom-select-simple.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomSelectSimpleOpen) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  RunHtmlTest(FILE_PATH_LITERAL("custom-select-simple-open.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCustomSelect) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  RunHtmlTest(FILE_PATH_LITERAL("custom-select.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityCustomSelect) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  RunHtmlTest(FILE_PATH_LITERAL("custom-select.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomSelectOpen) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  RunHtmlTest(FILE_PATH_LITERAL("custom-select-open.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDd) {
  RunHtmlTest(FILE_PATH_LITERAL("dd.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDel) {
  RunHtmlTest(FILE_PATH_LITERAL("del.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDesignMode) {
  RunHtmlTest(FILE_PATH_LITERAL("design-mode.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDetails) {
  RunHtmlTest(FILE_PATH_LITERAL("details.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDetailsName) {
  RunHtmlTest(FILE_PATH_LITERAL("details-name.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDfn) {
  RunHtmlTest(FILE_PATH_LITERAL("dfn.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDialog) {
  RunHtmlTest(FILE_PATH_LITERAL("dialog.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDisabled) {
  RunHtmlTest(FILE_PATH_LITERAL("disabled.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityDisabledWithSubtree) {
  RunHtmlTest(FILE_PATH_LITERAL("disabled-with-subtree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDiv) {
  RunHtmlTest(FILE_PATH_LITERAL("div.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDl) {
  RunHtmlTest(FILE_PATH_LITERAL("dl.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDt) {
  RunHtmlTest(FILE_PATH_LITERAL("dt.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDpubRoles) {
  RunAriaTest(FILE_PATH_LITERAL("dpub-roles.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityDpubRolesComputed) {
  RunAriaTest(FILE_PATH_LITERAL("dpub-roles-computed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityGraphicsRoles) {
  RunAriaTest(FILE_PATH_LITERAL("graphics-roles.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableBr) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-br.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableFontSize) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-font-size.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableDescendants) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableDescendantsFormControls) {
  RunFormControlsTest(FILE_PATH_LITERAL("contenteditable-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableDocsLi) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-docs-li.html"));
}

// TODO(b/324376803): Re-enable flaky test.
IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       DISABLED_AccessibilityContenteditableDocsLi) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-docs-li.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableLiContainsPresentation) {
  RunHtmlTest(
      FILE_PATH_LITERAL("contenteditable-li-contains-presentation.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityContenteditableLiContainsPresentation) {
  RunHtmlTest(
      FILE_PATH_LITERAL("contenteditable-li-contains-presentation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableSpans) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-spans.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityElementClassIdAttr) {
  RunHtmlTest(FILE_PATH_LITERAL("element-class-id-attr.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableDescendantsWithSelection) {
  RunHtmlTest(
      FILE_PATH_LITERAL("contenteditable-descendants-with-selection.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityContenteditableDescendantsWithSelection) {
  RunHtmlTest(
      FILE_PATH_LITERAL("contenteditable-descendants-with-selection.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    AccessibilityContenteditableWithEmbeddedContenteditables) {
  RunHtmlTest(
      FILE_PATH_LITERAL("contenteditable-with-embedded-contenteditables.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableWithNoDescendants) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-with-no-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditablePlaintextWithRole) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-plaintext-with-role.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableOnDisallowedElement) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-on-disallowed-element.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCustomElement) {
  RunHtmlTest(FILE_PATH_LITERAL("custom-element.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomElementEmptySlot) {
  RunHtmlTest(FILE_PATH_LITERAL("custom-element-empty-slot.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomElementHidden) {
  RunHtmlTest(FILE_PATH_LITERAL("custom-element-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomElementNestedSlots) {
  RunHtmlTest(FILE_PATH_LITERAL("custom-element-nested-slots.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomElementRemoveNodes) {
  RunHtmlTest(FILE_PATH_LITERAL("custom-element-remove-nodes.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomElementWithAriaOwnsOutside) {
  RunHtmlTest(FILE_PATH_LITERAL("custom-element-with-aria-owns-outside.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityCustomElementWithAriaOwnsOutside) {
  RunHtmlTest(FILE_PATH_LITERAL("custom-element-with-aria-owns-outside.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomElementWithAriaOwnsInside) {
  RunHtmlTest(FILE_PATH_LITERAL("custom-element-with-aria-owns-inside.html"));
}
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCustomElementWithAriaOwnsInsideSlot) {
  RunHtmlTest(
      FILE_PATH_LITERAL("custom-element-with-aria-owns-inside-slot.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityEm) {
  RunHtmlTest(FILE_PATH_LITERAL("em.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityEmbed) {
  RunHtmlTest(FILE_PATH_LITERAL("embed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityEmbedImageError) {
  RunHtmlTest(FILE_PATH_LITERAL("embed-image-error.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityFieldset) {
  RunHtmlTest(FILE_PATH_LITERAL("fieldset.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityFigcaption) {
  RunHtmlTest(FILE_PATH_LITERAL("figcaption.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityFigcaption) {
  RunHtmlTest(FILE_PATH_LITERAL("figcaption.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityFigcaptionNested) {
  RunHtmlTest(FILE_PATH_LITERAL("figcaption-nested.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityFigure) {
  RunHtmlTest(FILE_PATH_LITERAL("figure.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityFooter) {
  RunHtmlTest(FILE_PATH_LITERAL("footer.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityFooterInsideOtherSection) {
  RunHtmlTest(FILE_PATH_LITERAL("footer-inside-other-section.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityFooterInsideSectionRoleGeneric) {
  RunRegressionTest(
      FILE_PATH_LITERAL("footer-inside-section-role-generic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityForm) {
  RunHtmlTest(FILE_PATH_LITERAL("form.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityFormValidationMessage) {
  RunHtmlTest(FILE_PATH_LITERAL("form-validation-message.html"));
}

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    AccessibilityFormValidationMessageRemovedAfterErrorCorrected) {
  RunHtmlTest(FILE_PATH_LITERAL(
      "form-validation-message-removed-after-error-corrected.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityFrameset) {
  RunHtmlTest(FILE_PATH_LITERAL("frameset.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityFramesetPostEnable) {
  enable_accessibility_after_navigating_ = true;
  RunHtmlTest(FILE_PATH_LITERAL("frameset.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityGeneratedContentAfterHiddenInput) {
  RunHtmlTest(FILE_PATH_LITERAL("generated-content-after-hidden-input.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityGeneratedContentInEmptyPage) {
  RunHtmlTest(FILE_PATH_LITERAL("generated-content-in-empty-page.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityHead) {
  RunHtmlTest(FILE_PATH_LITERAL("head.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityHeader) {
  RunHtmlTest(FILE_PATH_LITERAL("header.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityHGroup) {
  RunHtmlTest(FILE_PATH_LITERAL("hgroup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityHeaderInsideOtherSection) {
  RunHtmlTest(FILE_PATH_LITERAL("header-inside-other-section.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityHeaderInsideSectionRoleGeneric) {
  RunRegressionTest(
      FILE_PATH_LITERAL("header-inside-section-role-generic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityHeading) {
  RunHtmlTest(FILE_PATH_LITERAL("heading.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityHeadingWithTabIndex) {
  RunHtmlTest(FILE_PATH_LITERAL("heading-with-tabIndex.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityHidden) {
  RunAriaTest(FILE_PATH_LITERAL("hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityHiddenDescribedBy) {
  RunAriaTest(FILE_PATH_LITERAL("hidden-described-by.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityHiddenLabeledBy) {
  RunAriaTest(FILE_PATH_LITERAL("hidden-labelled-by.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityHR) {
  RunHtmlTest(FILE_PATH_LITERAL("hr.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityHTML) {
  RunHtmlTest(FILE_PATH_LITERAL("html.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityHTMLAttributesAndTagNames) {
  RunHtmlTest(FILE_PATH_LITERAL("html-attributes-and-tag-names.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityI) {
  RunHtmlTest(FILE_PATH_LITERAL("i.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityIframe) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityIframeEmpty) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-empty.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeAriaHidden) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityIframeCreate) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-create.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeCreateEmpty) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-create-empty.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeEmptyPositioned) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-empty-positioned.html"));
}

class DumpAccessibilityTreeFencedFrameTest : public DumpAccessibilityTreeTest {
 protected:
  DumpAccessibilityTreeFencedFrameTest() {
    feature_list_.InitWithFeatures(
        {{blink::features::kFencedFrames},
         {features::kPrivacySandboxAdsAPIsOverride},
         {blink::features::kFencedFramesAPIChanges},
         {blink::features::kFencedFramesDefaultMode}},
        {/* disabled_features */});

    UseHttpsTestServer();
  }

  ~DumpAccessibilityTreeFencedFrameTest() override {
    // Ensure that the feature lists are destroyed in the same order they
    // were created in.
    scoped_feature_list_.Reset();
    feature_list_.Reset();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityTreeFencedFrameTest,
    ::testing::ValuesIn(DumpAccessibilityTestBase::TreeTestPasses()),
    DumpAccessibilityTreeTestPassToString());

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeFencedFrameTest,
                       AccessibilityFencedFrameScrollable) {
  RunHtmlTest(FILE_PATH_LITERAL("fencedframe-scrollable-mparch.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeScrollable) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-scrollable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeSrcdocChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-srcdoc-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframePostEnable) {
  enable_accessibility_after_navigating_ = true;
  RunHtmlTest(FILE_PATH_LITERAL("iframe.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeCrossProcess) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-cross-process.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeCoordinates) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-coordinates.html"));
}

// https://crbug.com/956990
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityIframeCoordinatesCrossProcess) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-coordinates-cross-process.html"));
}
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityIframePadding) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-padding.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframePresentational) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-presentational.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransform) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransformCrossProcess) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform-cross-process.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransformNested) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform-nested.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransformNestedCrossProcess) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform-nested-cross-process.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransformScrolled) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform-scrolled.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeWithInvalidChildren) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-with-invalid-children.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeWithInvalidChildrenAdded) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-with-invalid-children-added.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeWithRegionRole) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-with-region-role.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityImg) {
  RunHtmlTest(FILE_PATH_LITERAL("img.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityImgBroken) {
  RunHtmlTest(FILE_PATH_LITERAL("img-broken.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityImgEmptyAlt) {
  RunHtmlTest(FILE_PATH_LITERAL("img-empty-alt.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityImgLinkEmptyAlt) {
  RunHtmlTest(FILE_PATH_LITERAL("img-link-empty-alt.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityImgLinkEmptyAltSet) {
  RunHtmlTest(FILE_PATH_LITERAL("img-link-empty-alt-set.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityImgMimeType) {
  RunHtmlTest(FILE_PATH_LITERAL("img-mime-type.png"));  // Open an image file.
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInPageLinks) {
  RunHtmlTest(FILE_PATH_LITERAL("in-page-links.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, InertAttribute) {
  RunHtmlTest(FILE_PATH_LITERAL("inert-attribute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputButton) {
  RunHtmlTest(FILE_PATH_LITERAL("input-button.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputCheckBox) {
  RunHtmlTest(FILE_PATH_LITERAL("input-checkbox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputCheckBoxInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-checkbox-in-menu.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityInputCheckBoxInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-checkbox-in-menu.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputCheckBoxLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("input-checkbox-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputColor) {
  RunHtmlTest(FILE_PATH_LITERAL("input-color.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       NOT_ANDROID(AccessibilityInputColorWithPopupOpen)) {
  RunHtmlTest(FILE_PATH_LITERAL("input-color-with-popup-open.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputDate) {
  RunHtmlTest(FILE_PATH_LITERAL("input-date.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputDateDisabled) {
  RunHtmlTest(FILE_PATH_LITERAL("input-date-disabled.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputColorDisabled) {
  RunHtmlTest(FILE_PATH_LITERAL("input-color-disabled.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       NOT_ANDROID(AccessibilityInputDateWithPopupOpen)) {
  RunHtmlTest(FILE_PATH_LITERAL("input-date-with-popup-open.html"));
}

// The /blink test pass is different when run on Windows vs other OSs.
// So separate into two different tests: <input type="datetime-local"> has a
// ", " inserted between fields on Windows.
#if BUILDFLAG(IS_WIN)
#define AccessibilityInputDateWithPopupOpenMultiple_TestFile \
  FILE_PATH_LITERAL("input-date-with-popup-open-multiple-for-win.html")
#else
#define AccessibilityInputDateWithPopupOpenMultiple_TestFile \
  FILE_PATH_LITERAL("input-date-with-popup-open-multiple.html")
#endif

IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    NOT_ANDROID(AccessibilityInputDateWithPopupOpenMultiple)) {
  RunHtmlTest(AccessibilityInputDateWithPopupOpenMultiple_TestFile);
}

IN_PROC_BROWSER_TEST_P(
    YieldingParserDumpAccessibilityTreeTest,
    NOT_ANDROID(AccessibilityInputDateWithPopupOpenMultiple)) {
  RunHtmlTest(AccessibilityInputDateWithPopupOpenMultiple_TestFile);
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       NOT_ANDROID(AccessibilityInputTimeWithPopupOpen)) {
  RunHtmlTest(FILE_PATH_LITERAL("input-time-with-popup-open.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       NOT_ANDROID(AccessibilityInputTimeWithPopupOpen)) {
  RunHtmlTest(FILE_PATH_LITERAL("input-time-with-popup-open.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputDateTime) {
  RunHtmlTest(FILE_PATH_LITERAL("input-datetime.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputDateTimeLocal) {
  RunHtmlTest(FILE_PATH_LITERAL("input-datetime-local.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputEmail) {
  RunHtmlTest(FILE_PATH_LITERAL("input-email.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputFile) {
  RunHtmlTest(FILE_PATH_LITERAL("input-file.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputHidden) {
  RunHtmlTest(FILE_PATH_LITERAL("input-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputImage) {
  RunHtmlTest(FILE_PATH_LITERAL("input-image.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputList) {
  RunHtmlTest(FILE_PATH_LITERAL("input-list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputMonth) {
  RunHtmlTest(FILE_PATH_LITERAL("input-month.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputNumber) {
  RunHtmlTest(FILE_PATH_LITERAL("input-number.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputPassword) {
  RunHtmlTest(FILE_PATH_LITERAL("input-password.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputPasswordObscured) {
  RunHtmlTest(FILE_PATH_LITERAL("input-password-obscured.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputRadio) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputRadioCheckboxLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio-checkbox-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputRadioChunked) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio-chunked.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputRadioInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio-in-menu.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputRadioUnhidden) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio-unhidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputRadioWrappedLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio-wrapped-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputRange) {
  RunHtmlTest(FILE_PATH_LITERAL("input-range.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputReset) {
  RunHtmlTest(FILE_PATH_LITERAL("input-reset.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputSearch) {
  RunHtmlTest(FILE_PATH_LITERAL("input-search.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInsertBefore) {
  RunHtmlTest(FILE_PATH_LITERAL("insert-before.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityInsertBeforeWithWhitespace) {
  RunHtmlTest(FILE_PATH_LITERAL("insert-before-with-whitespace.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInsertBeforeWithWhitespace) {
  RunHtmlTest(FILE_PATH_LITERAL("insert-before-with-whitespace.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityInsertBefore) {
  RunHtmlTest(FILE_PATH_LITERAL("insert-before.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityScrollableOverflow) {
  RunHtmlTest(FILE_PATH_LITERAL("scrollable-overflow.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityReentrantAddChildren) {
  RunRegressionTest(FILE_PATH_LITERAL("reentrant-add-children.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityScrollableTextarea) {
  RunHtmlTest(FILE_PATH_LITERAL("scrollable-textarea.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityScrollable) {
  RunHtmlTest(FILE_PATH_LITERAL("scrollable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySmall) {
  RunHtmlTest(FILE_PATH_LITERAL("small.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputSubmit) {
  RunHtmlTest(FILE_PATH_LITERAL("input-submit.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputSuggestionsSourceElement) {
  RunHtmlTest(FILE_PATH_LITERAL("input-suggestions-source-element.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputTel) {
  RunHtmlTest(FILE_PATH_LITERAL("input-tel.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputText) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputTextReadOnly) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-read-only.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputTextNameCalc) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-name-calc.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputTextValue) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-value.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputTextValueChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-value-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputTextWithSelection) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-with-selection.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputTime) {
  RunHtmlTest(FILE_PATH_LITERAL("input-time.html"));
}

// The /blink test pass is different when run on Windows vs other OSs.
// So separate into two different tests.
#if BUILDFLAG(IS_WIN)
#define AccessibilityInputTypes_TestFile \
  FILE_PATH_LITERAL("input-types-for-win.html")
#else
#define AccessibilityInputTypes_TestFile FILE_PATH_LITERAL("input-types.html")
#endif

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputTypes) {
  RunHtmlTest(AccessibilityInputTypes_TestFile);
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputTypesWithValue) {
  RunHtmlTest(FILE_PATH_LITERAL("input-types-with-value.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputTypesWithPlaceholder) {
  RunHtmlTest(FILE_PATH_LITERAL("input-types-with-placeholder.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputTypesWithValueAndPlaceholder) {
  RunHtmlTest(FILE_PATH_LITERAL("input-types-with-value-and-placeholder.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputUrl) {
  RunHtmlTest(FILE_PATH_LITERAL("input-url.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputWeek) {
  RunHtmlTest(FILE_PATH_LITERAL("input-week.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityId) {
  RunHtmlTest(FILE_PATH_LITERAL("id.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityImgFormFormControls) {
  RunFormControlsTest(FILE_PATH_LITERAL("img-form.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityImgMapPseudoFormControls) {
  RunFormControlsTest(FILE_PATH_LITERAL("img-map-pseudo.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityIns) {
  RunHtmlTest(FILE_PATH_LITERAL("ins.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInteractiveControlsWithLabels) {
  RunHtmlTest(FILE_PATH_LITERAL("interactive-controls-with-labels.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityLabelForHiddenInput) {
  RunHtmlTest(FILE_PATH_LITERAL("label-for-hidden-input.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLabelUpdates) {
  RunHtmlTest(FILE_PATH_LITERAL("label-updates.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLandmark) {
  RunHtmlTest(FILE_PATH_LITERAL("landmark.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityLayoutTableInButton) {
  RunHtmlTest(FILE_PATH_LITERAL("layout-table-in-button.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLegend) {
  RunHtmlTest(FILE_PATH_LITERAL("legend.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLi) {
  RunHtmlTest(FILE_PATH_LITERAL("li.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLink) {
  RunHtmlTest(FILE_PATH_LITERAL("link.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLinkTarget) {
  RunHtmlTest(FILE_PATH_LITERAL("link-target.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityLinkInsideHeading) {
  RunHtmlTest(FILE_PATH_LITERAL("link-inside-heading.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityList) {
  RunHtmlTest(FILE_PATH_LITERAL("list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityListText) {
  RunHtmlTest(FILE_PATH_LITERAL("list-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityListTextAddition) {
  RunHtmlTest(FILE_PATH_LITERAL("list-text-addition.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityListTextRemoval) {
  RunHtmlTest(FILE_PATH_LITERAL("list-text-removal.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityListItemLevel) {
  RunHtmlTest(FILE_PATH_LITERAL("list-item-level.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityListItemNestedDiv) {
  RunHtmlTest(FILE_PATH_LITERAL("list-item-nested-div.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityListItemAriaSetsizeUnknown) {
  RunHtmlTest(FILE_PATH_LITERAL("list-item-aria-setsize-unknown.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityListItemAriaSetsizeUnknownFlattened) {
  RunHtmlTest(
      FILE_PATH_LITERAL("list-item-aria-setsize-unknown-flattened.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityListMarkerStyles) {
  RunHtmlTest(FILE_PATH_LITERAL("list-marker-styles.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityListMarkers) {
  RunHtmlTest(FILE_PATH_LITERAL("list-markers.html"));
}

// Explicitly enables 'speak-as' descriptor for CSS @counter-style rule to test
// accessibility tree with custom counter styles.
// TODO(xiaochengh): Remove this class after shipping 'speak-as'.
class DumpAccessibilityTreeWithSpeakAsDescriptorTest
    : public DumpAccessibilityTreeTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DumpAccessibilityTreeTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "CSSAtRuleCounterStyleSpeakAsDescriptor");
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityTreeWithSpeakAsDescriptorTest,
    ::testing::ValuesIn(DumpAccessibilityTestBase::TreeTestPasses()),
    DumpAccessibilityTreeTestPassToString());

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeWithSpeakAsDescriptorTest,
                       AccessibilityListMarkerStylesCustom) {
  RunCSSTest(FILE_PATH_LITERAL("list-marker-styles-custom.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLongText) {
  RunHtmlTest(FILE_PATH_LITERAL("long-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMain) {
  RunHtmlTest(FILE_PATH_LITERAL("main.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMapAnyContents) {
  RunHtmlTest(FILE_PATH_LITERAL("map-any-contents.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityMapInsideMapFormControls) {
  RunFormControlsTest(FILE_PATH_LITERAL("map-inside-map.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMapUnused) {
  RunHtmlTest(FILE_PATH_LITERAL("map-unused.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMapWithRole) {
  RunHtmlTest(FILE_PATH_LITERAL("map-with-role.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityMapWithAriaOwns) {
  RunHtmlTest(FILE_PATH_LITERAL("map-with-aria-owns.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMark) {
  RunHtmlTest(FILE_PATH_LITERAL("mark.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMath) {
  RunHtmlTest(FILE_PATH_LITERAL("math.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("menu.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMeta) {
  RunHtmlTest(FILE_PATH_LITERAL("meta.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMeter) {
  RunHtmlTest(FILE_PATH_LITERAL("meter.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityMicroClearfixHack) {
  RunHtmlTest(FILE_PATH_LITERAL("micro-clearfix-hack.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMinRole) {
  RunHtmlTest(FILE_PATH_LITERAL("min-role.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityMinRoleTabbableGroup) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableBlinkFeatures, "KeyboardFocusableScrollers");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableBlinkFeatures, "AccessibilityMinRoleTabbable");
  RunHtmlTest(FILE_PATH_LITERAL("min-role-tabbable-group.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMinRoleInGrid) {
  RunHtmlTest(FILE_PATH_LITERAL("min-role-in-grid.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityMissingRelationTargetsAddedLater) {
  RunAriaTest(FILE_PATH_LITERAL("missing-relation-targets-added-later.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogClosed) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-closed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogOpened) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-opened.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogInIframeOpened) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-opened.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityModalDialogInIframeOpened) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-opened.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogAndIframes) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-and-iframes.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogStack) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-stack.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityMoveChildHypertext) {
  RunHtmlTest(FILE_PATH_LITERAL("move-child-hypertext.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityMoveChildHypertext2) {
  RunHtmlTest(FILE_PATH_LITERAL("move-child-hypertext-2.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityNavigation) {
  RunHtmlTest(FILE_PATH_LITERAL("navigation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityNameCalcFocusable) {
  RunHtmlTest(FILE_PATH_LITERAL("name-calc-focusable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityNextOnLineCheckFailure) {
  RunCSSTest(FILE_PATH_LITERAL("next-on-line-check-failure.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityNoscript) {
  RunHtmlTest(FILE_PATH_LITERAL("noscript.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityOl) {
  RunHtmlTest(FILE_PATH_LITERAL("ol.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityObject) {
  RunHtmlTest(FILE_PATH_LITERAL("object.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityObjectImage) {
  RunHtmlTest(FILE_PATH_LITERAL("object-image.html"));
}

#if BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityObjectImageError) {
  RunHtmlTest(FILE_PATH_LITERAL("object-image-error.html"));
}
#endif

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityOffscreen) {
  RunHtmlTest(FILE_PATH_LITERAL("offscreen.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityOffscreenIframe) {
  RunHtmlTest(FILE_PATH_LITERAL("offscreen-iframe.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityOffscreenScroll) {
  RunHtmlTest(FILE_PATH_LITERAL("offscreen-scroll.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityOffscreenSelect) {
  RunHtmlTest(FILE_PATH_LITERAL("offscreen-select.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityOptgroup) {
  RunHtmlTest(FILE_PATH_LITERAL("optgroup.html"));
}

#if BUILDFLAG(IS_MAC)
// On a Mac, <select> uses native widget, and the a11y tree does not contain
// <select> contents ( unless it's a select that uses appearance: base-select
// which never uses a native widget on any platform)
#define MAYBE_AccessibilityOptgroupMenulist \
  DISABLED_AccessibilityOptgroupMenulist
#else
#define MAYBE_AccessibilityOptgroupMenulist AccessibilityOptgroupMenulist
#endif  // BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityOptgroupMenulist) {
  RunHtmlTest(FILE_PATH_LITERAL("optgroup-menulist.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityOptgroupCustomMenulist) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  RunHtmlTest(FILE_PATH_LITERAL("optgroup-custom-menulist.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityOpenModal) {
  RunHtmlTest(FILE_PATH_LITERAL("open-modal.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityOptionindatalist) {
  RunHtmlTest(FILE_PATH_LITERAL("option-in-datalist.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityOutput) {
  RunHtmlTest(FILE_PATH_LITERAL("output.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityOverflowActions) {
  RunHtmlTest(FILE_PATH_LITERAL("overflow-actions.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityOverflowScroller) {
  RunHtmlTest(FILE_PATH_LITERAL("overflow-scroller.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityP) {
  RunHtmlTest(FILE_PATH_LITERAL("p.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityParam) {
  RunHtmlTest(FILE_PATH_LITERAL("param.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityPermission) {
  RunHtmlTest(FILE_PATH_LITERAL("permission.html"));
}

class DumpAccessibilityTreeWithProhibitedNamesTest
    : public YieldingParserDumpAccessibilityTreeTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DumpAccessibilityTreeTest::SetUpCommandLine(command_line);
    // Explicitly enable feature that repairs accessible names on roles where it
    // prohibited, moving to description.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "AccessibilityProhibitedNames");
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityTreeWithProhibitedNamesTest,
    ::testing::ValuesIn(DumpAccessibilityTestBase::TreeTestPasses()),
    DumpAccessibilityTreeTestPassToString());

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeWithProhibitedNamesTest,
                       AccessibilityProhibitedName) {
  RunHtmlTest(FILE_PATH_LITERAL("prohibited-name.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityPopoverApi) {
  RunHtmlTest(FILE_PATH_LITERAL("popover-api.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityPopoverApi) {
  RunHtmlTest(FILE_PATH_LITERAL("popover-api.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityPopoverExpanded) {
  RunHtmlTest(FILE_PATH_LITERAL("popover-expanded.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityPopoverCollapsed) {
  RunHtmlTest(FILE_PATH_LITERAL("popover-collapsed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityPopoverHint) {
  RunPopoverHintTest(FILE_PATH_LITERAL("popover-hint.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityPopoverHint) {
  RunPopoverHintTest(FILE_PATH_LITERAL("popover-hint.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityPre) {
  RunHtmlTest(FILE_PATH_LITERAL("pre.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityPresentational) {
  RunAriaTest(FILE_PATH_LITERAL("presentational.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityPresentationalMenu) {
  RunAriaTest(FILE_PATH_LITERAL("presentational-menu.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityPresentationalOL) {
  RunAriaTest(FILE_PATH_LITERAL("presentational-ol.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityPresentationalUL) {
  RunAriaTest(FILE_PATH_LITERAL("presentational-ul.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityProgress) {
  RunHtmlTest(FILE_PATH_LITERAL("progress.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, PseudoElementListMarker) {
  RunHtmlTest(FILE_PATH_LITERAL("pseudo-element-list-marker.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityProgressWithBackgroundExposesValues) {
  RunRegressionTest(
      FILE_PATH_LITERAL("progress-with-background-exposes-values.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityProgressMeterCrash) {
  RunFormControlsTest(FILE_PATH_LITERAL("progress-meter-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityQ) {
  RunHtmlTest(FILE_PATH_LITERAL("q.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityRelevantSpace) {
  RunHtmlTest(FILE_PATH_LITERAL("relevant-space.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityReparentCrash) {
  RunHtmlTest(FILE_PATH_LITERAL("reparent-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityReplaceData) {
  RunHtmlTest(FILE_PATH_LITERAL("replace-data.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityRoleChange) {
  RunAriaTest(FILE_PATH_LITERAL("role-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityRoleChangeDelay) {
  RunAriaTest(FILE_PATH_LITERAL("role-change-delay.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityRoleGroupFormControls) {
  RunFormControlsTest(FILE_PATH_LITERAL("role-group.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityRoleGroupFormControlsWithInitialFullA11y) {
  // First turn on full a11y, including screen reader mode.
  WebContentsImpl* web_contents = GetWebContents();
  static_cast<WebContentsImpl*>(web_contents)
      ->AddAccessibilityModeForTesting(ui::kAXModeComplete);
  BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::kAXModeComplete);

  // Ensure that a form controls test can still set form controls mode even
  // if incompatible modes were set previously.
  RunFormControlsTest(FILE_PATH_LITERAL("role-group.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityRuby) {
  RunHtmlTest(FILE_PATH_LITERAL("ruby.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityS) {
  RunHtmlTest(FILE_PATH_LITERAL("s.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySamp) {
  RunHtmlTest(FILE_PATH_LITERAL("samp.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityScript) {
  RunHtmlTest(FILE_PATH_LITERAL("script.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySearch) {
  RunHtmlTest(FILE_PATH_LITERAL("search.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySection) {
  RunHtmlTest(FILE_PATH_LITERAL("section.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySelectionContainer) {
  RunHtmlTest(FILE_PATH_LITERAL("selection-container.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySelect) {
  RunHtmlTest(FILE_PATH_LITERAL("select.html"));
}

// The test times out on Mac because it cannot open the native select.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AccessibilitySelectOpen DISABLED_AccessibilitySelectOpen
#else
#define MAYBE_AccessibilitySelectOpen AccessibilitySelectOpen
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilitySelectOpen) {
  RunHtmlTest(FILE_PATH_LITERAL("select-open.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySelectInCanvas) {
  RunHtmlTest(FILE_PATH_LITERAL("select-in-canvas.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilitySelectInCanvas) {
  RunHtmlTest(FILE_PATH_LITERAL("select-in-canvas.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySelectFollowsFocus) {
  RunHtmlTest(FILE_PATH_LITERAL("select-follows-focus.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySelectFollowsFocusAriaSelectedFalse) {
  RunHtmlTest(
      FILE_PATH_LITERAL("select-follows-focus-aria-selected-false.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySelectFollowsFocusMultiselect) {
  RunHtmlTest(FILE_PATH_LITERAL("select-follows-focus-multiselect.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySpan) {
  RunHtmlTest(FILE_PATH_LITERAL("span.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySpanLineBreak) {
  RunHtmlTest(FILE_PATH_LITERAL("span-line-break.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilitySpanLineBreak) {
  RunHtmlTest(FILE_PATH_LITERAL("span-line-break.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityStaticList) {
  RunHtmlTest(FILE_PATH_LITERAL("static-list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityStrong) {
  RunHtmlTest(FILE_PATH_LITERAL("strong.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityStyle) {
  RunHtmlTest(FILE_PATH_LITERAL("style.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySub) {
  RunHtmlTest(FILE_PATH_LITERAL("sub.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilitySub) {
  RunHtmlTest(FILE_PATH_LITERAL("sub.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySup) {
  RunHtmlTest(FILE_PATH_LITERAL("sup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySummary) {
  RunHtmlTest(FILE_PATH_LITERAL("summary.html"));
}

IN_PROC_BROWSER_TEST_P(SummaryAsHeadingDumpAccessibilityTreeTest,
                       AccessibilitySummaryAsHeading) {
  RunHtmlTest(FILE_PATH_LITERAL("summary-as-heading.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySvg) {
  RunHtmlTest(FILE_PATH_LITERAL("svg.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgAsEmbedSource) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-as-embed-source.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgAsObjectSource) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-as-object-source.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgChildOfButton) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-child-of-button.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySvgChildOfSvg) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-child-of-svg.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySvgDescInGroup) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-desc-in-group.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgElementsNotMapped) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-elements-not-mapped.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilitySvgElementsNotMapped) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-elements-not-mapped.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgTextAlternativeComputation) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-text-alternative-computation.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilitySvgTextAlternativeComputation) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-text-alternative-computation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgTitleInGroup) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-title-in-group.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgWithClickableRect) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-with-clickable-rect.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgWithForeignObject) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-with-foreign-object.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgWithLinkToDocument) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-with-link-to-document.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgWithNonLinkAnchors) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-with-non-link-anchors.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgSymbolWithRole) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-symbol-with-role.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySvgG) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-g.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTabindexExposeChildren) {
  RunHtmlTest(FILE_PATH_LITERAL("tabindex-expose-children.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTableRowAdd) {
  RunHtmlTest(FILE_PATH_LITERAL("table-row-add.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTableSimple) {
  RunHtmlTest(FILE_PATH_LITERAL("table-simple.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTableLayout) {
  RunHtmlTest(FILE_PATH_LITERAL("table-layout.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityTableLayout) {
  RunHtmlTest(FILE_PATH_LITERAL("table-layout.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTablePresentation) {
  RunHtmlTest(FILE_PATH_LITERAL("table-presentation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableThColHeader) {
  RunHtmlTest(FILE_PATH_LITERAL("table-th-colheader.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableThRowHeader) {
  RunHtmlTest(FILE_PATH_LITERAL("table-th-rowheader.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableTbodyTfoot) {
  RunHtmlTest(FILE_PATH_LITERAL("table-thead-tbody-tfoot.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableFocusableSections) {
  RunHtmlTest(FILE_PATH_LITERAL("table-focusable-sections.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTableSpans) {
  RunHtmlTest(FILE_PATH_LITERAL("table-spans.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableHeadersEmptyFirstCell) {
  RunHtmlTest(FILE_PATH_LITERAL("table-headers-empty-first-cell.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableHeadersOnAllSides) {
  RunHtmlTest(FILE_PATH_LITERAL("table-headers-on-all-sides.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableHeadersRowRoleDynamic) {
  RunHtmlTest(FILE_PATH_LITERAL("table-headers-row-role-dynamic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableMultipleRowAndColumnHeaders) {
  RunHtmlTest(FILE_PATH_LITERAL("table-multiple-row-and-column-headers.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AccessibilityTableMultipleRowAndColumnHeaders) {
  RunHtmlTest(FILE_PATH_LITERAL("table-multiple-row-and-column-headers.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTabPanel) {
  RunHtmlTest(FILE_PATH_LITERAL("tab-panel.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTextAlign) {
  RunHtmlTest(FILE_PATH_LITERAL("text-align.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTextColorsAndStyles) {
  RunHtmlTest(FILE_PATH_LITERAL("text-colors-and-styles.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTextDecorationStyles) {
  RunHtmlTest(FILE_PATH_LITERAL("text-decoration-styles.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTextIndent) {
  RunHtmlTest(FILE_PATH_LITERAL("text-indent.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTextarea) {
  RunHtmlTest(FILE_PATH_LITERAL("textarea.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTextareaChanges) {
  RunHtmlTest(FILE_PATH_LITERAL("textarea-changes.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTextareaReadOnly) {
  RunHtmlTest(FILE_PATH_LITERAL("textarea-read-only.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTextareaWithSelection) {
  RunHtmlTest(FILE_PATH_LITERAL("textarea-with-selection.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityToggleButtonExpandCollapse) {
  RunAriaTest(FILE_PATH_LITERAL("toggle-button-expand-collapse.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTime) {
  RunHtmlTest(FILE_PATH_LITERAL("time.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTitle) {
  RunHtmlTest(FILE_PATH_LITERAL("title.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTitleEmpty) {
  RunHtmlTest(FILE_PATH_LITERAL("title-empty.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTitleAttribute) {
  RunHtmlTest(FILE_PATH_LITERAL("title-attribute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTitleChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("title-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTransition) {
  RunHtmlTest(FILE_PATH_LITERAL("transition.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTruncateLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("truncate-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityUl) {
  RunHtmlTest(FILE_PATH_LITERAL("ul.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityUlContenteditable) {
  RunHtmlTest(FILE_PATH_LITERAL("ul-contenteditable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityNotUserSelectable) {
  RunCSSTest(FILE_PATH_LITERAL("user-select.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityVar) {
  RunHtmlTest(FILE_PATH_LITERAL("var.html"));
}

// crbug.com/281952
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DISABLED_AccessibilityVideo) {
  RunHtmlTest(FILE_PATH_LITERAL("video.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityVideoTextOnly) {
  RunHtmlTest(FILE_PATH_LITERAL("video-text-only.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityNodeChangedCrashInEditableText) {
  RunHtmlTest(FILE_PATH_LITERAL("node-changed-crash-in-editable-text.html"));
}

// TODO(crbug.com/40866942): This test is failing on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_AccessibilityNoSourceVideo DISABLED_AccessibilityNoSourceVideo
#else
#define MAYBE_AccessibilityNoSourceVideo AccessibilityNoSourceVideo
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityNoSourceVideo) {
#if BUILDFLAG(IS_MAC)
  // The /blink test pass is different on macOS than on other platforms. See
  // https://crbug.com/1314896.
  if (GetParam() == ui::AXApiType::kBlink) {
    return;
  }
#endif
  RunHtmlTest(FILE_PATH_LITERAL("no-source-video.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityVideoControls) {
#if BUILDFLAG(IS_MAC)
  // The /blink test pass is different on macOS than on other platforms. See
  // https://crbug.com/1314896.
  if (GetParam() == ui::AXApiType::kBlink) {
    return;
  }
#endif
  RunHtmlTest(FILE_PATH_LITERAL("video-controls.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityWbr) {
  RunHtmlTest(FILE_PATH_LITERAL("wbr.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityWindowCropsItems) {
  RunHtmlTest(FILE_PATH_LITERAL("window-crops-items.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputInsideLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("input-inside-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputImageWithTitle) {
  RunHtmlTest(FILE_PATH_LITERAL("input-image-with-title.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIgnoredSelectionNoUnignored) {
  RunHtmlTest(FILE_PATH_LITERAL("ignored-selection-no-unignored.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIgnoredSelectionBetweenText) {
  RunHtmlTest(FILE_PATH_LITERAL("ignored-selection-between-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIgnoredSelection) {
  RunHtmlTest(FILE_PATH_LITERAL("ignored-selection.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityLabelWithSelectedOption) {
  RunHtmlTest(FILE_PATH_LITERAL("label-with-selected-option.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityLabelWithPresentationalChild) {
  RunHtmlTest(FILE_PATH_LITERAL("label-with-presentational-child.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityNestedList) {
  RunHtmlTest(FILE_PATH_LITERAL("nestedlist.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityNextAndPreviousOnLineId) {
  RunHtmlTest(FILE_PATH_LITERAL("next-and-previous-on-line-id.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityButtonWithListboxPopup) {
  RunHtmlTest(FILE_PATH_LITERAL("button-with-listbox-popup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DeleteSelectionCrash) {
  RunHtmlTest(FILE_PATH_LITERAL("delete-selection-crash.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       DeleteSelectionCrash) {
  RunHtmlTest(FILE_PATH_LITERAL("delete-selection-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ReloadSelectionCrash) {
  RunRegressionTest(FILE_PATH_LITERAL("reload-selection-crash.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       ReloadSelectionCrash) {
  RunRegressionTest(FILE_PATH_LITERAL("reload-selection-crash.html"));
}

//
// DisplayLocking tests
//

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DisplayLockingActivatable) {
  RunDisplayLockingTest(FILE_PATH_LITERAL("activatable.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       DisplayLockingActivatable) {
  RunDisplayLockingTest(FILE_PATH_LITERAL("activatable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DisplayLockingNonActivatable) {
  RunDisplayLockingTest(FILE_PATH_LITERAL("non-activatable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DisplayLockingViewportActivation) {
  RunDisplayLockingTest(FILE_PATH_LITERAL("viewport-activation.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DisplayLockingAll) {
  RunDisplayLockingTest(FILE_PATH_LITERAL("all.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       DisplayLockingAll) {
  RunDisplayLockingTest(FILE_PATH_LITERAL("all.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DisplayLockingAllCommitted) {
  RunDisplayLockingTest(FILE_PATH_LITERAL("all-committed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityDisplayLockedSelectMenu) {
  RunDisplayLockingTest(FILE_PATH_LITERAL("selectmenu.html"));
}

//
// Regression tests. These don't test a specific web platform feature,
// they test a specific web page that crashed or had some bad behavior
// in the past.
//

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AriaPressedChangesButtonRole) {
  RunRegressionTest(FILE_PATH_LITERAL("aria-pressed-changes-button-role.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AddChildOfNotIncludedInTreeChain) {
  RunRegressionTest(
      FILE_PATH_LITERAL("add-child-of-not-included-in-tree-chain.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AddChildOfNotIncludedInTreeChain) {
  RunRegressionTest(
      FILE_PATH_LITERAL("add-child-of-not-included-in-tree-chain.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AddDisplayNoneChildren) {
  RunRegressionTest(FILE_PATH_LITERAL("add-display-none-children.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AddDisplayNoneChildren) {
  RunRegressionTest(FILE_PATH_LITERAL("add-display-none-children.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ComputeNameWhileFreezing) {
  RunRegressionTest(FILE_PATH_LITERAL("compute-name-while-freezing.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       ContentVisibilityWithPseudoElement) {
  RunRegressionTest(
      FILE_PATH_LITERAL("content-visibility-with-pseudo-element.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ContentVisibilityLabel) {
  RunRegressionTest(FILE_PATH_LITERAL("content-visibility-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DisplayContentsSelectCrash) {
  RunRegressionTest(FILE_PATH_LITERAL("display-contents-select-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, LanguageChangedOnHtml) {
  RunRegressionTest(FILE_PATH_LITERAL("language-changed-on-html.html"));
}

// TODO(crbug.com/341125461) Times out on macOS as showing the select popup
// involves a nested runloop, but nothing in the test closes the popup/leaves
// that nested runloop.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       SelectExpandManyOptionsCrash) {
  RunRegressionTest(FILE_PATH_LITERAL("select-expand-many-options-crash.html"));
}
#endif

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, XmlInIframeCrash) {
  RunRegressionTest(FILE_PATH_LITERAL("xml-in-iframe-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       RelationPointsToInvalidNodesCrash) {
  RunRegressionTest(
      FILE_PATH_LITERAL("relation-points-to-invalid-nodes-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AddClickIgnoredChanged) {
  RunRegressionTest(FILE_PATH_LITERAL("add-click-ignored-changed.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       AddClickIgnoredChanged) {
  RunRegressionTest(FILE_PATH_LITERAL("add-click-ignored-changed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AriaHiddenNegativeTabindexIgnoredInTree) {
  RunRegressionTest(
      FILE_PATH_LITERAL("aria-hidden-negative-tabindex-ignored-in-tree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AriaHiddenTabindexChange) {
  RunRegressionTest(FILE_PATH_LITERAL("aria-hidden-tabindex-change.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       ClearChildrenWhileComputingName) {
  RunRegressionTest(
      FILE_PATH_LITERAL("clear-children-while-computing-name.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       FrozenAncestorCannotChangeDescendants) {
  RunRegressionTest(
      FILE_PATH_LITERAL("frozen-ancestor-cannot-change-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, HiddenTable) {
  RunRegressionTest(FILE_PATH_LITERAL("hidden-table.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest, HiddenTable) {
  RunRegressionTest(FILE_PATH_LITERAL("hidden-table.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, IgnoredCrash) {
  RunRegressionTest(FILE_PATH_LITERAL("ignored-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, MissingParent) {
  RunRegressionTest(FILE_PATH_LITERAL("missing-parent.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       OptionAccessibleNameIsSelect) {
  RunRegressionTest(FILE_PATH_LITERAL("option-accessible-name-is-select.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, RemovePseudoContent) {
  RunRegressionTest(FILE_PATH_LITERAL("remove-pseudo-content.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, RemoveAriaOwner) {
  RunRegressionTest(FILE_PATH_LITERAL("remove-aria-owner.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ReusedMap) {
  RunRegressionTest(FILE_PATH_LITERAL("reused-map.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ReusedMapMoveImage) {
  RunRegressionTest(FILE_PATH_LITERAL("reused-map-move-image.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       ReusedMapMoveImage) {
  RunRegressionTest(FILE_PATH_LITERAL("reused-map-move-image.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ReusedMapMoveImageToTop) {
  RunRegressionTest(FILE_PATH_LITERAL("reused-map-move-image-to-top.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ReusedMapChangeUsemap) {
  RunRegressionTest(FILE_PATH_LITERAL("reused-map-change-usemap.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, RootBecomesLeaf) {
  RunRegressionTest(FILE_PATH_LITERAL("root-becomes-leaf.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySlotCreationCrash) {
  RunRegressionTest(FILE_PATH_LITERAL("slot-creation-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, TitleInShadow) {
  RunRegressionTest(FILE_PATH_LITERAL("title-in-shadow.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ReusedMapChangeMapName) {
  RunRegressionTest(FILE_PATH_LITERAL("reused-map-change-map-name.html"));
}

IN_PROC_BROWSER_TEST_P(YieldingParserDumpAccessibilityTreeTest,
                       ReusedMapChangeMapName) {
  RunRegressionTest(FILE_PATH_LITERAL("reused-map-change-map-name.html"));
}

// Enable language detection for both static and dynamic content.
class DumpAccessibilityTreeWithLanguageDetectionTest
    : public DumpAccessibilityTreeTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DumpAccessibilityTreeTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityLanguageDetection);
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityLanguageDetectionDynamic);
  }

  void RunLanguageDetectionTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path =
        GetTestFilePath("accessibility", "language-detection");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath language_detection_file =
        test_path.Append(base::FilePath(file_path));

    RunTest(ui::kAXModeComplete, language_detection_file,
            "accessibility/language-detection");
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityTreeWithLanguageDetectionTest,
    ::testing::ValuesIn(DumpAccessibilityTestBase::TreeTestPasses()),
    DumpAccessibilityTreeTestPassToString());

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeWithLanguageDetectionTest,
                       LangAttribute) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("lang-attribute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeWithLanguageDetectionTest,
                       LangAttributeNested) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("lang-attribute-nested.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeWithLanguageDetectionTest,
                       LangAttributeSwitching) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("lang-attribute-switching.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeWithLanguageDetectionTest,
                       LangDetectionStaticBasic) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("static-basic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeWithLanguageDetectionTest,
                       LangDetectionDynamicBasic) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("dynamic-basic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeWithLanguageDetectionTest,
                       LangDetectionDynamicMultipleInserts) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("dynamic-multiple-inserts.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeWithLanguageDetectionTest,
                       LangDetectionDynamicReparenting) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("dynamic-reparenting.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ComboboxItemVisibility) {
  RunHtmlTest(FILE_PATH_LITERAL("combobox-item-visibility.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, MathNameFromContents) {
  RunHtmlTest(FILE_PATH_LITERAL("math-name-from-contents.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, SVGAndMathElements) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-and-math-elements.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, MathMLSpace) {
  RunMathMLTest(FILE_PATH_LITERAL("mspace.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, MathMLUnknown) {
  RunMathMLTest(FILE_PATH_LITERAL("unknown.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, MalformedMap) {
  RunHtmlTest(FILE_PATH_LITERAL("malformed-map.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ShadowDomFirstChild) {
  RunHtmlTest(FILE_PATH_LITERAL("shadow-dom-first-child.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, WhitespaceDynamic) {
  RunHtmlTest(FILE_PATH_LITERAL("whitespace-dynamic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, TableWithPseudoElements) {
  RunHtmlTest(FILE_PATH_LITERAL("table-with-pseudo-elements.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, SvgImageNameFromTitle) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-image-name-from-title.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, IgnoreDuplicateRelationIds) {
  RunRelationsTest(FILE_PATH_LITERAL("ignore-duplicate-relation-ids.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, IgnoreReflexiveRelations) {
  RunRelationsTest(FILE_PATH_LITERAL("ignore-reflexive-relations.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       RelationsPreserveAuthorOrder) {
  RunRelationsTest(FILE_PATH_LITERAL("relations-preserve-author-order.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSVisibilityInert) {
  RunCSSTest(FILE_PATH_LITERAL("visibility-inert.html"));
}

//
// AccName tests where having the full tree is desired.
//
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, NameImgLabelledbyInputs) {
  RunAccNameTest(FILE_PATH_LITERAL("name-img-labelledby-inputs.html"));
}

//
// These tests cover features of the testing infrastructure itself.
//

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DenyNode) {
  RunTestHarnessTest(FILE_PATH_LITERAL("deny-node.html"));
}

}  // namespace content
