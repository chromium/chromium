// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

// TODO(aboxhall): Create expectations on Android for these
#if defined(OS_ANDROID)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

namespace content {

typedef AccessibilityTreeFormatter::PropertyFilter PropertyFilter;

// See content/test/data/accessibility/readme.md for an overview.
//
// This test takes a snapshot of the platform BrowserAccessibility tree and
// tests it against an expected baseline.
//
// The flow of the test is as outlined below.
// 1. Load an html file from content/test/data/accessibility.
// 2. Read the expectation.
// 3. Browse to the page and serialize the platform specific tree into a human
//    readable string.
// 4. Perform a comparison between actual and expected and fail if they do not
//    exactly match.
class DumpAccessibilityTreeTest : public DumpAccessibilityTestBase {
 public:
  void AddDefaultFilters(
      std::vector<PropertyFilter>* property_filters) override;
  void AddPropertyFilter(std::vector<PropertyFilter>* property_filters,
                         const std::string& filter,
                         PropertyFilter::Type type = PropertyFilter::ALLOW) {
    property_filters->push_back(PropertyFilter(filter, type));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DumpAccessibilityTestBase::SetUpCommandLine(command_line);
    // Enable <dialog>, which is used in some tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    // Enable accessibility object model, used in other tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "AccessibilityObjectModel");
    // Enable display locking, used in some tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "CSSContentVisibilityHiddenMatchable");
    // kDisableAXMenuList is true on Chrome OS by default. Make it consistent
    // for these cross-platform tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kDisableAXMenuList, "false");
  }

  void RunAriaTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "aria");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath aria_file = test_path.Append(base::FilePath(file_path));

    RunTest(aria_file, "accessibility/aria");
  }

  void RunAomTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "aom");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath aom_file = test_path.Append(base::FilePath(file_path));

    RunTest(aom_file, "accessibility/aom");
  }

  void RunCSSTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "css");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath css_file = test_path.Append(base::FilePath(file_path));

    RunTest(css_file, "accessibility/css");
  }

  void RunHtmlTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "html");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath html_file = test_path.Append(base::FilePath(file_path));

    RunTest(html_file, "accessibility/html");
  }

  void RunDisplayLockingTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path =
        GetTestFilePath("accessibility", "display-locking");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath html_file = test_path.Append(base::FilePath(file_path));

    RunTest(html_file, "accessibility/display-locking");
  }

  void RunRegressionTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "regression");
    base::FilePath test_file = test_path.Append(base::FilePath(file_path));

    RunTest(test_file, "accessibility/regression");
  }

  std::vector<std::string> Dump(std::vector<std::string>& unused) override {
    std::unique_ptr<AccessibilityTreeFormatter> formatter(formatter_factory_());
    formatter->SetPropertyFilters(property_filters_);
    formatter->SetNodeFilters(node_filters_);
    std::string actual_contents;
    formatter->FormatAccessibilityTreeForTesting(
        GetRootAccessibilityNode(shell()->web_contents()), &actual_contents);
    return base::SplitString(actual_contents, "\n", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
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

    // Enable language detection for both static and dynamic content.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityLanguageDetection);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityLanguageDetectionDynamic);

    RunTest(language_detection_file, "accessibility/language-detection");
  }

  // Testing of the Test Harness itself.
  void RunTestHarnessTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "test-harness");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath test_harness_file =
        test_path.Append(base::FilePath(file_path));

    RunTest(test_harness_file, "accessibility/test-harness");
  }

 protected:
  // Override from DumpAccessibilityTestBase.
  void ChooseFeatures(std::vector<base::Feature>* enabled_features,
                      std::vector<base::Feature>* disabled_features) override {
    // http://crbug.com/1063155 - temporary until this is enabled
    // everywhere.
    enabled_features->emplace_back(
        features::kEnableAccessibilityExposeHTMLElement);
    DumpAccessibilityTestBase::ChooseFeatures(enabled_features,
                                              disabled_features);
  }
};

void DumpAccessibilityTreeTest::AddDefaultFilters(
    std::vector<PropertyFilter>* property_filters) {
  AddPropertyFilter(property_filters, "value='*'");
  // The value attribute on the document object contains the URL of the current
  // page which will not be the same every time the test is run.
  AddPropertyFilter(property_filters, "value='http*'", PropertyFilter::DENY);
  // Object attributes.value
  AddPropertyFilter(property_filters, "layout-guess:*", PropertyFilter::ALLOW);

  AddPropertyFilter(property_filters, "select*");
  AddPropertyFilter(property_filters, "selectedFromFocus=*",
                    PropertyFilter::DENY);
  AddPropertyFilter(property_filters, "descript*");
  AddPropertyFilter(property_filters, "check*");
  AddPropertyFilter(property_filters, "horizontal");
  AddPropertyFilter(property_filters, "multiselectable");

  // Deny most empty values
  AddPropertyFilter(property_filters, "*=''", PropertyFilter::DENY);
  // After denying empty values, because we want to allow name=''
  AddPropertyFilter(property_filters, "name=*", PropertyFilter::ALLOW_EMPTY);
}

// Parameterize the tests so that each test-pass is run independently.
struct DumpAccessibilityTreeTestPassToString {
  std::string operator()(const ::testing::TestParamInfo<size_t>& i) const {
    auto passes = AccessibilityTreeFormatter::GetTestPasses();
    CHECK_LT(i.param, passes.size());
    return passes[i.param].name;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityTreeTest,
    ::testing::Range(size_t{0},
                     AccessibilityTreeFormatter::GetTestPasses().size()),
    DumpAccessibilityTreeTestPassToString());

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSColor) {
  RunCSSTest(FILE_PATH_LITERAL("color.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSDisplayNone) {
  RunCSSTest(FILE_PATH_LITERAL("display-none.html"));
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
                       AccessibilityCSSInlinePositionRelative) {
  RunCSSTest(FILE_PATH_LITERAL("inline-position-relative.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSLanguage) {
  RunCSSTest(FILE_PATH_LITERAL("language.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSPseudoElements) {
  RunCSSTest(FILE_PATH_LITERAL("pseudo-element-alternative-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSDOMElements) {
  RunCSSTest(FILE_PATH_LITERAL("dom-element-css-alternative-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityCSSTableIncomplete) {
  RunCSSTest(FILE_PATH_LITERAL("table-incomplete.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSOverflow) {
  RunCSSTest(FILE_PATH_LITERAL("overflow.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityCSSTransform) {
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityActionVerbs) {
  RunHtmlTest(FILE_PATH_LITERAL("action-verbs.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityActions) {
  RunHtmlTest(FILE_PATH_LITERAL("actions.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAddClickListener) {
  RunHtmlTest(FILE_PATH_LITERAL("add-click-listener.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAreaCrash) {
  RunHtmlTest(FILE_PATH_LITERAL("area-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAreaSerializationCrash) {
  RunHtmlTest(FILE_PATH_LITERAL("area-serialization-crash.html"));
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
                       AccessibilityComboboxOptgroup) {
  RunHtmlTest(FILE_PATH_LITERAL("combobox-optgroup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySvgStyleElement) {
  RunHtmlTest(FILE_PATH_LITERAL("svg-style-element.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAomBusy) {
  RunAomTest(FILE_PATH_LITERAL("aom-busy.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAomChecked) {
  RunAomTest(FILE_PATH_LITERAL("aom-checked.html"));
}

// TODO(crbug.com/983709): Flaky.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityAriaActivedescendant) {
  RunAriaTest(FILE_PATH_LITERAL("aria-activedescendant.html"));
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
                       AccessibilityAriaColumnHeader) {
  RunAriaTest(FILE_PATH_LITERAL("aria-columnheader.html"));
}

#if defined(OS_ANDROID)
// TODO(crbug.com/986673): test is flaky on android.
#define MAYBE_AccessibilityAriaCombobox DISABLED_AccessibilityAriaCombobox
#else
#define MAYBE_AccessibilityAriaCombobox AccessibilityAriaCombobox
#endif

// DISABLE A BUNCH OF TESTS FOR ANDROID
// ------------------------------------
// TODO(crbug.com/1137967): tests are flaky on android.
#if defined(OS_ANDROID)
#define MAYBE_AccessibilityAriaMenuItemRadio \
  DISABLED_AccessibilityAriaMenuItemRadio
#define MAYBE_AccessibilityAriaComboboxUneditable \
  DISABLED_AccessibilityAriaComboboxUneditable
#define MAYBE_AccessibilityAriaListBox DISABLED_AccessibilityAriaListBox
#define MAYBE_AccessibilityAriaListBoxDisabled \
  DISABLED_AccessibilityAriaListBoxDisabled
#define MAYBE_AccessibilityAriaOption DISABLED_AccessibilityAriaOption
#define MAYBE_AccessibilityAriaPosinset DISABLED_AccessibilityAriaPosinset
#define MAYBE_AccessibilityAriaSelected DISABLED_AccessibilityAriaSelected
#define MAYBE_AccessibilityAriaSetsize DISABLED_AccessibilityAriaSetsize
#define MAYBE_AccessibilityAriaTree DISABLED_AccessibilityAriaTree
#define MAYBE_AccessibilityButtonWithListboxPopup \
  DISABLED_AccessibilityButtonWithListboxPopup
#else
#define MAYBE_AccessibilityAriaMenuItemRadio AccessibilityAriaMenuItemRadio
#define MAYBE_AccessibilityAriaComboboxUneditable \
  AccessibilityAriaComboboxUneditable
#define MAYBE_AccessibilityAriaListBox AccessibilityAriaListBox
#define MAYBE_AccessibilityAriaListBoxDisabled AccessibilityAriaListBoxDisabled
#define MAYBE_AccessibilityAriaOption AccessibilityAriaOption
#define MAYBE_AccessibilityAriaPosinset AccessibilityAriaPosinset
#define MAYBE_AccessibilityAriaSelected AccessibilityAriaSelected
#define MAYBE_AccessibilityAriaSetsize AccessibilityAriaSetsize
#define MAYBE_AccessibilityAriaTree AccessibilityAriaTree
#define MAYBE_AccessibilityButtonWithListboxPopup \
  AccessibilityButtonWithListboxPopup
#endif
// ------------------------------------

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaCombobox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-combobox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaComboboxUneditable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-combobox-uneditable.html"));
}

#if defined(OS_ANDROID)
// TODO(crbug.com/986673): test is flaky on android.
#define MAYBE_AccessibilityAriaOnePointOneCombobox \
  DISABLED_AccessibilityAriaOnePointOneCombobox
#else
#define MAYBE_AccessibilityAriaOnePointOneCombobox \
  AccessibilityAriaOnePointOneCombobox
#endif

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaOnePointOneCombobox) {
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaControls) {
  RunAriaTest(FILE_PATH_LITERAL("aria-controls.html"));
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
                       AccessibilityAriaDetailsRoles) {
  RunAriaTest(FILE_PATH_LITERAL("aria-details-roles.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDialog) {
  RunAriaTest(FILE_PATH_LITERAL("aria-dialog.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDirectory) {
  RunAriaTest(FILE_PATH_LITERAL("aria-directory.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDisabled) {
  RunAriaTest(FILE_PATH_LITERAL("aria-disabled.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDocument) {
  RunAriaTest(FILE_PATH_LITERAL("aria-document.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaDropEffect) {
  RunAriaTest(FILE_PATH_LITERAL("aria-dropeffect.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaEmphasis) {
  RunAriaTest(FILE_PATH_LITERAL("aria-emphasis.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaErrorMessage) {
  RunAriaTest(FILE_PATH_LITERAL("aria-errormessage.html"));
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
                       AccessibilityAriaHiddenDescendants) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-descendants.html"));
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
                       AccessibilityAriaHiddenLabelledBy) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-labelled-by.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenIframeBody) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-iframe-body.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenIframe) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-iframe.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE(AccessibilityAriaFlowto)) {
  RunAriaTest(FILE_PATH_LITERAL("aria-flowto.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE(AccessibilityAriaFlowtoMultiple)) {
  RunAriaTest(FILE_PATH_LITERAL("aria-flowto-multiple.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaForm) {
  RunAriaTest(FILE_PATH_LITERAL("aria-form.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaGeneric) {
  RunAriaTest(FILE_PATH_LITERAL("aria-generic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaGrabbed) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grabbed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaGrid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaGridDynamicAddRow) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid-dynamic-add-row.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaGridExtraWrapElems) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid-extra-wrap-elems.html"));
}

// https://crbug.com/1117594
#if defined(OS_ANDROID)
#define MAYBE_AccessibilityAriaGridCell DISABLED_AccessibilityAriaGridCell
#else
#define MAYBE_AccessibilityAriaGridCell AccessibilityAriaGridCell
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaGridCell) {
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaInsertionDeletion) {
  RunAriaTest(FILE_PATH_LITERAL("aria-insertion-deletion.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaInvalid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-invalid.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaKeyShortcuts) {
  RunAriaTest(FILE_PATH_LITERAL("aria-keyshortcuts.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaLabel) {
  RunAriaTest(FILE_PATH_LITERAL("aria-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByHeading) {
  RunAriaTest(FILE_PATH_LITERAL("aria-labelledby-heading.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByUpdates) {
  RunAriaTest(FILE_PATH_LITERAL("aria-labelledby-updates.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaListBox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaListBoxDisabled) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox-disabled.html"));
}
// TODO(crbug.com/983802): Flaky.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityAriaListBoxActiveDescendant) {
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

#if defined(OS_ANDROID)
#define MAYBE_AccessibilityAriaMenuItemInGroup \
  DISABLED_AccessibilityAriaMenuItemInGroup
#else
#define MAYBE_AccessibilityAriaMenuItemInGroup AccessibilityAriaMenuItemInGroup
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaMenuItemInGroup) {
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
                       MAYBE_AccessibilityAriaMenuItemRadio) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menuitemradio.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMeter) {
  RunAriaTest(FILE_PATH_LITERAL("aria-meter.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaMismatchedTableAttr) {
  RunHtmlTest(FILE_PATH_LITERAL("aria-mismatched-table-attr.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaOwnsList) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaMath) {
  RunAriaTest(FILE_PATH_LITERAL("aria-math.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaNone) {
  RunAriaTest(FILE_PATH_LITERAL("aria-none.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaOption) {
  RunAriaTest(FILE_PATH_LITERAL("aria-option.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaParagraph) {
  RunAriaTest(FILE_PATH_LITERAL("aria-paragraph.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaPosinset) {
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

// TODO(http://crbug.com/1061624): fails on Windows.
#if defined(OS_WIN)
#define MAYBE_AccessibilityAriaRowText DISABLED_AccessibilityAriaRowText
#else
#define MAYBE_AccessibilityAriaRowText AccessibilityAriaRowText
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaRowText) {
  RunAriaTest(FILE_PATH_LITERAL("aria-rowtext.html"));
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
                       DISABLED_AccessibilityAriaSearchboxWithSelection) {
  RunAriaTest(FILE_PATH_LITERAL("aria-searchbox-with-selection.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaSelected) {
  RunAriaTest(FILE_PATH_LITERAL("aria-selected.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaSeparator) {
  RunAriaTest(FILE_PATH_LITERAL("aria-separator.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityAriaSetsize) {
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
                       AccessibilityAriaTableIllegalRoles) {
  RunAriaTest(FILE_PATH_LITERAL("aria-table-illegal-roles.html"));
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
                       AccessibilityAriaTextboxWithRichText) {
  RunAriaTest(FILE_PATH_LITERAL("aria-textbox-with-rich-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityAriaTextboxWithSelection) {
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, MAYBE_AccessibilityAriaTree) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tree.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityAriaTreeGrid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-treegrid.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTreeDiscontinuous) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tree-discontinuous.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityAriaTreeitemNestedInLists) {
  RunAriaTest(FILE_PATH_LITERAL("aria-treeitem-nested-in-lists.html"));
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
                       AccessibilityInputTextARIAPlaceholder) {
  RunAriaTest(FILE_PATH_LITERAL("input-text-aria-placeholder.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTableColumnHidden) {
  RunAriaTest(FILE_PATH_LITERAL("table-column-hidden.html"));
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

// https://crbug.com/923993
// Super flaky with NetworkService.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DISABLED_AccessibilityAudio) {
  RunHtmlTest(FILE_PATH_LITERAL("audio.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessiblitiyBoundsFixed) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-fixed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessiblitiyBoundsFixedScrolling) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-fixed-scrolling.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityBR) {
  RunHtmlTest(FILE_PATH_LITERAL("br.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDd) {
  RunHtmlTest(FILE_PATH_LITERAL("dd.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDel) {
  RunHtmlTest(FILE_PATH_LITERAL("del.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityDetails) {
  RunHtmlTest(FILE_PATH_LITERAL("details.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityGraphicsRoles) {
  RunAriaTest(FILE_PATH_LITERAL("graphics-roles.html"));
}

#if defined(OS_ANDROID) || defined(OS_MAC)
// Flaky failures: http://crbug.com/445929.
// Mac failures: http://crbug.com/571712.
#define MAYBE_AccessibilityContenteditableDescendants \
  DISABLED_AccessibilityContenteditableDescendants
#else
#define MAYBE_AccessibilityContenteditableDescendants \
  AccessibilityContenteditableDescendants
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityContenteditableDescendants) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityElementClassIdSrcAttr) {
  RunHtmlTest(FILE_PATH_LITERAL("element-class-id-src-attr.html"));
}

#if defined(OS_ANDROID) || defined(OS_MAC)
// Flaky failures: http://crbug.com/445929.
// Mac failures: http://crbug.com/571712.
#define MAYBE_AccessibilityContenteditableDescendantsWithSelection \
  DISABLED_AccessibilityContenteditableDescendantsWithSelection
#else
#define MAYBE_AccessibilityContenteditableDescendantsWithSelection \
  AccessibilityContenteditableDescendantsWithSelection
#endif
IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    MAYBE_AccessibilityContenteditableDescendantsWithSelection) {
  RunHtmlTest(
      FILE_PATH_LITERAL("contenteditable-descendants-with-selection.html"));
}

#if defined(OS_ANDROID)
// Flaky failures: http://crbug.com/445929.
#define MAYBE_AccessibilityContenteditableWithEmbeddedContenteditables \
  DISABLED_AccessibilityContenteditableWithEmbeddedContenteditables
#else
#define MAYBE_AccessibilityContenteditableWithEmbeddedContenteditables \
  AccessibilityContenteditableWithEmbeddedContenteditables
#endif
IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    MAYBE_AccessibilityContenteditableWithEmbeddedContenteditables) {
  RunHtmlTest(
      FILE_PATH_LITERAL("contenteditable-with-embedded-contenteditables.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableWithNoDescendants) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-with-no-descendants.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityEm) {
  RunHtmlTest(FILE_PATH_LITERAL("em.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityEmbed) {
  RunHtmlTest(FILE_PATH_LITERAL("embed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityFieldset) {
  RunHtmlTest(FILE_PATH_LITERAL("fieldset.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityFigcaption) {
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityForm) {
  RunHtmlTest(FILE_PATH_LITERAL("form.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityFormValidationMessage) {
  RunHtmlTest(FILE_PATH_LITERAL("form-validation-message.html"));
}

// https://crbug.com/944519
IN_PROC_BROWSER_TEST_P(
    DumpAccessibilityTreeTest,
    DISABLED_AccessibilityFormValidationMessageRemovedAfterErrorCorrected) {
  RunHtmlTest(FILE_PATH_LITERAL(
      "form-validation-message-removed-after-error-corrected.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityFormValidationMessageAfterHideTimeout) {
  RunHtmlTest(
      FILE_PATH_LITERAL("form-validation-message-after-hide-timeout.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityHeaderInsideOtherSection) {
  RunHtmlTest(FILE_PATH_LITERAL("header-inside-other-section.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityI) {
  RunHtmlTest(FILE_PATH_LITERAL("i.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityIframe) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframeScrollable) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-scrollable.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityIframePostEnable) {
  enable_accessibility_after_navigating_ = true;
  RunHtmlTest(FILE_PATH_LITERAL("iframe.html"));
}

// https://crbug.com/622387
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityIframeCrossProcess) {
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityImg) {
  RunHtmlTest(FILE_PATH_LITERAL("img.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityImgEmptyAlt) {
  RunHtmlTest(FILE_PATH_LITERAL("img-empty-alt.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityImgLinkEmptyAlt) {
  RunHtmlTest(FILE_PATH_LITERAL("img-link-empty-alt.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInPageLinks) {
  RunHtmlTest(FILE_PATH_LITERAL("in-page-links.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputButton) {
  RunHtmlTest(FILE_PATH_LITERAL("input-button.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputButtonInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-button-in-menu.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputCheckBox) {
  RunHtmlTest(FILE_PATH_LITERAL("input-checkbox.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
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
                       AccessibilityInputColorWithPopupOpen) {
  RunHtmlTest(FILE_PATH_LITERAL("input-color-with-popup-open.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputDate) {
  RunHtmlTest(FILE_PATH_LITERAL("input-date.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputDateWithPopupOpen) {
  RunHtmlTest(FILE_PATH_LITERAL("input-date-with-popup-open.html"));
}

// The /blink test pass is different when run on Windows vs other OSs.
// So separate into two different tests.
#if defined(OS_WIN)
#define AccessibilityInputDateWithPopupOpenMultiple_TestFile \
  FILE_PATH_LITERAL("input-date-with-popup-open-multiple-for-win.html")
#else
#define AccessibilityInputDateWithPopupOpenMultiple_TestFile \
  FILE_PATH_LITERAL("input-date-with-popup-open-multiple.html")
#endif
// Flaky on all platforms. http://crbug.com/1055764
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityInputDateWithPopupOpenMultiple) {
  RunHtmlTest(AccessibilityInputDateWithPopupOpenMultiple_TestFile);
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputTimeWithPopupOpen) {
  RunHtmlTest(FILE_PATH_LITERAL("input-time-with-popup-open.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputDateTime) {
  RunHtmlTest(FILE_PATH_LITERAL("input-datetime.html"));
}

// Fails on OS X 10.9 and higher <https://crbug.com/430622>.
#if !defined(OS_MAC)
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputDateTimeLocal) {
  RunHtmlTest(FILE_PATH_LITERAL("input-datetime-local.html"));
}
#endif

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputEmail) {
  RunHtmlTest(FILE_PATH_LITERAL("input-email.html"));
}

// http://crbug.com/1114193
#if defined(OS_ANDROID)
#define MAYBE_AccessibilityInputFile DISABLED_AccessibilityInputFile
#else
#define MAYBE_AccessibilityInputFile AccessibilityInputFile
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityInputFile) {
  RunHtmlTest(FILE_PATH_LITERAL("input-file.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputHidden) {
  RunHtmlTest(FILE_PATH_LITERAL("input-hidden.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputImage) {
  RunHtmlTest(FILE_PATH_LITERAL("input-image.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputImageButtonInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-image-button-in-menu.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputList) {
  RunHtmlTest(FILE_PATH_LITERAL("input-list.html"));
}

// crbug.com/423675 - AX tree is different for Win7 and Win8.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityInputMonth) {
  RunHtmlTest(FILE_PATH_LITERAL("input-month.html"));
}
#else
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputMonth) {
  RunHtmlTest(FILE_PATH_LITERAL("input-month.html"));
}
#endif

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputNumber) {
  RunHtmlTest(FILE_PATH_LITERAL("input-number.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputPassword) {
  RunHtmlTest(FILE_PATH_LITERAL("input-password.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputRadio) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputRadioCheckboxLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio-checkbox-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityInputRadioInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio-in-menu.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityScrollableInput) {
  RunHtmlTest(FILE_PATH_LITERAL("scrollable-input.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityScrollableOverflow) {
  RunHtmlTest(FILE_PATH_LITERAL("scrollable-overflow.html"));
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

#if defined(OS_ANDROID)
// TODO(crbug.com/986673): test is flaky on android.
#define MAYBE_AccessibilityInputTextReadOnly \
  DISABLED_AccessibilityInputTextReadOnly
#else
#define MAYBE_AccessibilityInputTextReadOnly AccessibilityInputTextReadOnly
#endif

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityInputTextReadOnly) {
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

#if defined(OS_MAC)
// TODO(1038813): The /blink test pass is different on Windows and Mac, versus
// Linux.
#define MAYBE_AccessibilityInputTime DISABLED_AccessibilityInputTime
#else
#define MAYBE_AccessibilityInputTime AccessibilityInputTime
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityInputTime) {
  RunHtmlTest(FILE_PATH_LITERAL("input-time.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityInputTypes) {
  RunHtmlTest(FILE_PATH_LITERAL("input-types.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityIns) {
  RunHtmlTest(FILE_PATH_LITERAL("ins.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("label.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityLinkInsideHeading) {
  RunHtmlTest(FILE_PATH_LITERAL("link-inside-heading.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityList) {
  RunHtmlTest(FILE_PATH_LITERAL("list.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityListItemLevel) {
  RunHtmlTest(FILE_PATH_LITERAL("list-item-level.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityLongText) {
  RunHtmlTest(FILE_PATH_LITERAL("long-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMain) {
  RunHtmlTest(FILE_PATH_LITERAL("main.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMark) {
  RunHtmlTest(FILE_PATH_LITERAL("mark.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMath) {
  RunHtmlTest(FILE_PATH_LITERAL("math.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityMenutypecontext) {
  RunHtmlTest(FILE_PATH_LITERAL("menu-type-context.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMeta) {
  RunHtmlTest(FILE_PATH_LITERAL("meta.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityMeter) {
  RunHtmlTest(FILE_PATH_LITERAL("meter.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogClosed) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-closed.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogOpened) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-opened.html"));
}

// http://crbug.com/738497
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityModalDialogInIframeClosed) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-closed.html"));
}

// Disabled because it is flaky in several platforms
/*
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogInIframeOpened) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-opened.html"));
}
*/

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogStack) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-stack.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityNavigation) {
  RunHtmlTest(FILE_PATH_LITERAL("navigation.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityObjectImageError) {
  RunHtmlTest(FILE_PATH_LITERAL("object-image-error.html"));
}

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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityP) {
  RunHtmlTest(FILE_PATH_LITERAL("p.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityParam) {
  RunHtmlTest(FILE_PATH_LITERAL("param.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityPre) {
  RunHtmlTest(FILE_PATH_LITERAL("pre.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityProgress) {
  RunHtmlTest(FILE_PATH_LITERAL("progress.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityPortal) {
  RunHtmlTest(FILE_PATH_LITERAL("portal.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityPortalNameFromText) {
  RunHtmlTest(FILE_PATH_LITERAL("portal-name-from-text.html"));
}

// Flaky on all platforms: crbug.com/1103753.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityPortalWithWidgetInside) {
  RunHtmlTest(FILE_PATH_LITERAL("portal-with-widget-inside.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityPortalNameFromVisibleText) {
  RunHtmlTest(FILE_PATH_LITERAL("portal-name-from-visible-text.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityQ) {
  RunHtmlTest(FILE_PATH_LITERAL("q.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySection) {
  RunHtmlTest(FILE_PATH_LITERAL("section.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilitySelectionContainer) {
  RunHtmlTest(FILE_PATH_LITERAL("selection-container.html"));
}

// https://crbug.com/1117594
#if defined(OS_ANDROID)
#define MAYBE_AccessibilitySelect DISABLED_AccessibilitySelect
#else
#define MAYBE_AccessibilitySelect AccessibilitySelect
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, MAYBE_AccessibilitySelect) {
  RunHtmlTest(FILE_PATH_LITERAL("select.html"));
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

// https://crbug.com/1117594
#if defined(OS_ANDROID)
#define MAYBE_AccessibilitySelectFollowsFocusMultiselect \
  DISABLED_AccessibilitySelectFollowsFocusMultiselect
#else
#define MAYBE_AccessibilitySelectFollowsFocusMultiselect \
  AccessibilitySelectFollowsFocusMultiselect
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilitySelectFollowsFocusMultiselect) {
  RunHtmlTest(FILE_PATH_LITERAL("select-follows-focus-multiselect.html"));
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_AccessibilitySource DISABLED_AccessibilitySource
#else
#define MAYBE_AccessibilitySource AccessibilitySource
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, MAYBE_AccessibilitySource) {
  RunHtmlTest(FILE_PATH_LITERAL("source.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySpan) {
  RunHtmlTest(FILE_PATH_LITERAL("span.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySpanLineBreak) {
  RunHtmlTest(FILE_PATH_LITERAL("span-line-break.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySup) {
  RunHtmlTest(FILE_PATH_LITERAL("sup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySummary) {
  RunHtmlTest(FILE_PATH_LITERAL("summary.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySvg) {
  RunHtmlTest(FILE_PATH_LITERAL("svg.html"));
}

// On ChromeOS, SVG <g> elements are included.
#if defined(OS_CHROMEOS)
#define AccessibilitySvgG_TestFile FILE_PATH_LITERAL("svg-g-for-cros.html")
#else
#define AccessibilitySvgG_TestFile FILE_PATH_LITERAL("svg-g.html")
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilitySvgG) {
  RunHtmlTest(AccessibilitySvgG_TestFile);
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTableSimple) {
  RunHtmlTest(FILE_PATH_LITERAL("table-simple.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTableLayout) {
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
                       AccessibilityTableMultipleRowAndColumnHeaders) {
  RunHtmlTest(FILE_PATH_LITERAL("table-multiple-row-and-column-headers.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTextAlign) {
  RunHtmlTest(FILE_PATH_LITERAL("text-align.html"));
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
                       AccessibilityTextareaReadOnly) {
  RunHtmlTest(FILE_PATH_LITERAL("textarea-read-only.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       AccessibilityTextareaWithSelection) {
  RunHtmlTest(FILE_PATH_LITERAL("textarea-with-selection.html"));
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTitleChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("title-changed.html"));
}

#if defined(OS_WIN) || defined(OS_MAC)
// Flaky on Win/Mac: crbug.com/508532
#define MAYBE_AccessibilityTransition DISABLED_AccessibilityTransition
#else
#define MAYBE_AccessibilityTransition AccessibilityTransition
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityTransition) {
  RunHtmlTest(FILE_PATH_LITERAL("transition.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityTruncateLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("truncate-label.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, AccessibilityUl) {
  RunHtmlTest(FILE_PATH_LITERAL("ul.html"));
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

// TODO(crbug.com/916003): Fix race condition.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityNoSourceVideo) {
  RunHtmlTest(FILE_PATH_LITERAL("no-source-video.html"));
}

// TODO(crbug.com/916003): Fix race condition.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityVideoControls) {
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
                       MAYBE_AccessibilityButtonWithListboxPopup) {
  RunHtmlTest(FILE_PATH_LITERAL("button-with-listbox-popup.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DeleteSelectionCrash) {
  RunHtmlTest(FILE_PATH_LITERAL("delete-selection-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DisplayLockingActivatable) {
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

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DisplayLockingAllCommitted) {
  RunDisplayLockingTest(FILE_PATH_LITERAL("all-committed.html"));
}

//
// Regression tests. These don't test a specific web platform feature,
// they test a specific web page that crashed or had some bad behavior
// in the past.
//

// Flaky on all platforms. http://crbug.com/1055764
IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DISABLED_XmlInIframeCrash) {
  RunRegressionTest(FILE_PATH_LITERAL("xml-in-iframe-crash.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, HiddenTable) {
  RunRegressionTest(FILE_PATH_LITERAL("hidden-table.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       LanguageDetectionLangAttribute) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("lang-attribute.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       LanguageDetectionLangAttributeNested) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("lang-attribute-nested.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       LanguageDetectionLangAttributeSwitching) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("lang-attribute-switching.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       LanguageDetectionLangDetectionStaticBasic) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("static-basic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       LanguageDetectionLangDetectionDynamicBasic) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("dynamic-basic.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       LanguageDetectionLangDetectionDynamicMultipleInserts) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("dynamic-multiple-inserts.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest,
                       LanguageDetectionLangDetectionDynamicReparenting) {
  RunLanguageDetectionTest(FILE_PATH_LITERAL("dynamic-reparenting.html"));
}

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, ComboboxItemVisibility) {
  RunHtmlTest(FILE_PATH_LITERAL("combobox-item-visibility.html"));
}

//
// These tests cover features of the testing infrastructure itself.
//

IN_PROC_BROWSER_TEST_P(DumpAccessibilityTreeTest, DenyNode) {
  RunTestHarnessTest(FILE_PATH_LITERAL("deny-node.html"));
}

}  // namespace content
