// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

// TODO(aboxhall): Create expectations on Android for these
#if defined(OS_ANDROID)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

namespace content {

typedef AccessibilityTreeFormatter::Filter Filter;

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
  void AddDefaultFilters(std::vector<Filter>* filters) override;
  void AddFilter(std::vector<Filter>* filters,
                 std::string filter,
                 Filter::Type type = Filter::ALLOW) {
    filters->push_back(Filter(base::ASCIIToUTF16(filter), type));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DumpAccessibilityTestBase::SetUpCommandLine(command_line);
    // Enable <dialog>, which is used in some tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    // Enable accessibility object model, used in other tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "AccessibilityObjectModel");
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

  void RunRegressionTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "regression");
    base::FilePath test_file = test_path.Append(base::FilePath(file_path));

    RunTest(test_file, "accessibility/regression");
  }

  std::vector<std::string> Dump(std::vector<std::string>& unused) override {
    std::unique_ptr<AccessibilityTreeFormatter> formatter(
        CreateAccessibilityTreeFormatter());
    formatter->SetFilters(filters_);
    base::string16 actual_contents_utf16;
    WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
        shell()->web_contents());
    formatter->FormatAccessibilityTree(
        web_contents->GetRootBrowserAccessibilityManager()->GetRoot(),
        &actual_contents_utf16);
    std::string actual_contents = base::UTF16ToUTF8(actual_contents_utf16);
    return base::SplitString(
        actual_contents, "\n",
        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
};

void DumpAccessibilityTreeTest::AddDefaultFilters(
    std::vector<Filter>* filters) {
  // TODO(aleventhal) Each platform deserves separate default filters.

  //
  // Windows
  //

  // Too noisy: HOTTRACKED, LINKED, SELECTABLE, IA2_STATE_EDITABLE,
  //            IA2_STATE_OPAQUE, IA2_STATE_SELECTAbLE_TEXT,
  //            IA2_STATE_SINGLE_LINE, IA2_STATE_VERTICAL.
  // Too unpredictible: OFFSCREEN
  // Windows states to log by default:
  AddFilter(filters, "ALERT*");
  AddFilter(filters, "ANIMATED*");
  AddFilter(filters, "BUSY");
  AddFilter(filters, "CHECKED");
  AddFilter(filters, "COLLAPSED");
  AddFilter(filters, "DEFAULT");
  AddFilter(filters, "EXPANDED");
  AddFilter(filters, "FLOATING");
  AddFilter(filters, "FOCUSABLE");
  AddFilter(filters, "HASPOPUP");
  AddFilter(filters, "INVISIBLE");
  AddFilter(filters, "MARQUEED");
  AddFilter(filters, "MIXED");
  AddFilter(filters, "MOVEABLE");
  AddFilter(filters, "MULTISELECTABLE");
  AddFilter(filters, "PRESSED");
  AddFilter(filters, "PROTECTED");
  AddFilter(filters, "READONLY");
  AddFilter(filters, "SELECTED");
  AddFilter(filters, "SIZEABLE");
  AddFilter(filters, "TRAVERSED");
  AddFilter(filters, "UNAVAILABLE");
  AddFilter(filters, "IA2_STATE_ACTIVE");
  AddFilter(filters, "IA2_STATE_ARMED");
  AddFilter(filters, "IA2_STATE_CHECKABLE");
  AddFilter(filters, "IA2_STATE_DEFUNCT");
  AddFilter(filters, "IA2_STATE_HORIZONTAL");
  AddFilter(filters, "IA2_STATE_ICONIFIED");
  AddFilter(filters, "IA2_STATE_INVALID_ENTRY");
  AddFilter(filters, "IA2_STATE_MODAL");
  AddFilter(filters, "IA2_STATE_MULTI_LINE");
  AddFilter(filters, "IA2_STATE_PINNED");
  AddFilter(filters, "IA2_STATE_REQUIRED");
  AddFilter(filters, "IA2_STATE_STALE");
  AddFilter(filters, "IA2_STATE_TRANSIENT");
  // Reduce flakiness.
  AddFilter(filters, "FOCUSED", Filter::DENY);
  AddFilter(filters, "HOTTRACKED", Filter::DENY);
  AddFilter(filters, "OFFSCREEN", Filter::DENY);
  AddFilter(filters, "value='*'");
  // The value attribute on the document object contains the URL of the current
  // page which will not be the same every time the test is run.
  AddFilter(filters, "value='http*'", Filter::DENY);
  // Object attributes.value
  AddFilter(filters, "layout-guess:*", Filter::ALLOW);

  //
  // Blink
  //

  // Noisy, perhaps add later:
  //   editable, focus*, horizontal, linked, richlyEditable, vertical
  // Too flaky: hovered, offscreen
  // States
  AddFilter(filters, "check*");
  AddFilter(filters, "descript*");
  AddFilter(filters, "collapsed");
  AddFilter(filters, "default");
  AddFilter(filters, "haspopup");
  AddFilter(filters, "horizontal");
  AddFilter(filters, "invisible");
  AddFilter(filters, "multiline");
  AddFilter(filters, "multiselectable");
  AddFilter(filters, "protected");
  AddFilter(filters, "required");
  AddFilter(filters, "select*");
  AddFilter(filters, "visited");
  // Other attributes
  AddFilter(filters, "busy=true");
  AddFilter(filters, "valueForRange*");
  AddFilter(filters, "minValueForRange*");
  AddFilter(filters, "maxValueForRange*");
  AddFilter(filters, "hierarchicalLevel*");
  AddFilter(filters, "aria*");  // ariaCurrentState, ariaInvalidValue, etc.
  AddFilter(filters, "autoComplete*");
  AddFilter(filters, "restriction*");
  AddFilter(filters, "keyShortcuts*");
  AddFilter(filters, "activedescendantId*");
  AddFilter(filters, "controlsIds*");
  AddFilter(filters, "flowtoIds*");
  AddFilter(filters, "detailsIds*");
  AddFilter(filters, "invalidState=*");
  AddFilter(filters, "invalidState=false",
            Filter::DENY);  // Don't show false value
  AddFilter(filters, "roleDescription=*");

  //
  // OS X
  //

  AddFilter(filters, "AXValueAutofill*");
  AddFilter(filters, "AXAutocomplete*");

  //
  // Android
  //

  AddFilter(filters, "hint=*");
  AddFilter(filters, "interesting", Filter::DENY);
  AddFilter(filters, "has_character_locations", Filter::DENY);
  AddFilter(filters, "has_image", Filter::DENY);

  //
  // General
  //

  // Deny most empty values
  AddFilter(filters, "*=''", Filter::DENY);
  // After denying empty values, because we want to allow name=''
  AddFilter(filters, "name=*", Filter::ALLOW_EMPTY);
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCSSColor) {
  RunCSSTest(FILE_PATH_LITERAL("color.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCSSFontStyle) {
  RunCSSTest(FILE_PATH_LITERAL("font-style.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCSSFontFamily) {
  RunCSSTest(FILE_PATH_LITERAL("font-family.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityCSSDisplayToInline) {
  RunCSSTest(FILE_PATH_LITERAL("display-to-inline.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityCSSDisplayToBlock) {
  RunCSSTest(FILE_PATH_LITERAL("display-to-block.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityCSSInlinePositionRelative) {
  RunCSSTest(FILE_PATH_LITERAL("inline-position-relative.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCSSLanguage) {
  RunCSSTest(FILE_PATH_LITERAL("language.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityCSSTableIncomplete) {
  RunCSSTest(FILE_PATH_LITERAL("table-incomplete.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCSSTransform) {
  RunCSSTest(FILE_PATH_LITERAL("transform.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityA) {
  RunHtmlTest(FILE_PATH_LITERAL("a.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAbbr) {
  RunHtmlTest(FILE_PATH_LITERAL("abbr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAbsoluteOffscreen) {
  RunHtmlTest(FILE_PATH_LITERAL("absolute-offscreen.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityActionVerbs) {
  RunHtmlTest(FILE_PATH_LITERAL("action-verbs.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityActions) {
  RunHtmlTest(FILE_PATH_LITERAL("actions.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAddClickListener) {
  RunHtmlTest(FILE_PATH_LITERAL("add-click-listener.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAddress) {
  RunHtmlTest(FILE_PATH_LITERAL("address.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityArea) {
  RunHtmlTest(FILE_PATH_LITERAL("area.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAName) {
  RunHtmlTest(FILE_PATH_LITERAL("a-name.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityANameCalc) {
  RunHtmlTest(FILE_PATH_LITERAL("a-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityANoText) {
  RunHtmlTest(FILE_PATH_LITERAL("a-no-text.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAOnclick) {
  RunHtmlTest(FILE_PATH_LITERAL("a-onclick.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAIsInteresting) {
  RunHtmlTest(FILE_PATH_LITERAL("isInteresting.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityClickableAncestor) {
  RunHtmlTest(FILE_PATH_LITERAL("clickable-ancestor.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAomBusy) {
  RunAomTest(FILE_PATH_LITERAL("aom-busy.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAomChecked) {
  RunAomTest(FILE_PATH_LITERAL("aom-checked.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaActivedescendant) {
  RunAriaTest(FILE_PATH_LITERAL("aria-activedescendant.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaAlert) {
  RunAriaTest(FILE_PATH_LITERAL("aria-alert.html"));
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityAriaAlertDialog) {
  RunAriaTest(FILE_PATH_LITERAL("aria-alertdialog.html"));
}
#else
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaAlertDialog) {
  RunAriaTest(FILE_PATH_LITERAL("aria-alertdialog.html"));
}
#endif

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaAnyUnignored) {
  RunAriaTest(FILE_PATH_LITERAL("aria-any-unignored.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaApplication) {
  RunAriaTest(FILE_PATH_LITERAL("aria-application.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaArticle) {
  RunAriaTest(FILE_PATH_LITERAL("aria-article.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaAtomic) {
  RunAriaTest(FILE_PATH_LITERAL("aria-atomic.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaAutocomplete) {
  RunAriaTest(FILE_PATH_LITERAL("aria-autocomplete.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaBanner) {
  RunAriaTest(FILE_PATH_LITERAL("aria-banner.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaBlockquote) {
  RunAriaTest(FILE_PATH_LITERAL("aria-blockquote.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaBusy) {
  RunAriaTest(FILE_PATH_LITERAL("aria-busy.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaButton) {
  RunAriaTest(FILE_PATH_LITERAL("aria-button.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaCaption) {
  RunAriaTest(FILE_PATH_LITERAL("aria-caption.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaCell) {
  RunAriaTest(FILE_PATH_LITERAL("aria-cell.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaCheckBox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-checkbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaChecked) {
  RunAriaTest(FILE_PATH_LITERAL("aria-checked.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaColAttr) {
  RunAriaTest(FILE_PATH_LITERAL("aria-col-attr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaColumnHeader) {
  RunAriaTest(FILE_PATH_LITERAL("aria-columnheader.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaCombobox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-combobox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaOnePointOneCombobox) {
  RunAriaTest(FILE_PATH_LITERAL("aria1.1-combobox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaComplementary) {
  RunAriaTest(FILE_PATH_LITERAL("aria-complementary.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaContentInfo) {
  RunAriaTest(FILE_PATH_LITERAL("aria-contentinfo.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaControls) {
  RunAriaTest(FILE_PATH_LITERAL("aria-controls.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaCurrent) {
  RunAriaTest(FILE_PATH_LITERAL("aria-current.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaDefinition) {
  RunAriaTest(FILE_PATH_LITERAL("aria-definition.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaDescribedBy) {
  RunAriaTest(FILE_PATH_LITERAL("aria-describedby.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaDescribedByUpdates) {
  RunAriaTest(FILE_PATH_LITERAL("aria-describedby-updates.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaDetails) {
  RunAriaTest(FILE_PATH_LITERAL("aria-details.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaDialog) {
  RunAriaTest(FILE_PATH_LITERAL("aria-dialog.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaDirectory) {
  RunAriaTest(FILE_PATH_LITERAL("aria-directory.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaDisabled) {
  RunAriaTest(FILE_PATH_LITERAL("aria-disabled.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaDocument) {
  RunAriaTest(FILE_PATH_LITERAL("aria-document.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaDropEffect) {
  RunAriaTest(FILE_PATH_LITERAL("aria-dropeffect.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaEditable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-editable.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaErrorMessage) {
  RunAriaTest(FILE_PATH_LITERAL("aria-errormessage.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaExpanded) {
  RunAriaTest(FILE_PATH_LITERAL("aria-expanded.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaFeed) {
  RunAriaTest(FILE_PATH_LITERAL("aria-feed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaFigure) {
  RunAriaTest(FILE_PATH_LITERAL("aria-figure.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaHasPopup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-haspopup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaHeading) {
  RunAriaTest(FILE_PATH_LITERAL("aria-heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaHidden) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaHiddenIframeBody) {
  RunAriaTest(FILE_PATH_LITERAL("aria-hidden-iframe-body.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       MAYBE(AccessibilityAriaFlowto)) {
  RunAriaTest(FILE_PATH_LITERAL("aria-flowto.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaForm) {
  RunAriaTest(FILE_PATH_LITERAL("aria-form.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaGrabbed) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grabbed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaGrid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaGridDynamicAddRow) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid-dynamic-add-row.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaGridExtraWrapElems) {
  RunAriaTest(FILE_PATH_LITERAL("aria-grid-extra-wrap-elems.html"));
}
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaGridCell) {
  RunAriaTest(FILE_PATH_LITERAL("aria-gridcell.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaGroup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-group.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaIllegalVal) {
  RunAriaTest(FILE_PATH_LITERAL("aria-illegal-val.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaImg) {
  RunAriaTest(FILE_PATH_LITERAL("aria-img.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaInvalid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-invalid.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaKeyShortcuts) {
  RunAriaTest(FILE_PATH_LITERAL("aria-keyshortcuts.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaLabel) {
  RunAriaTest(FILE_PATH_LITERAL("aria-label.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByHeading) {
  RunAriaTest(FILE_PATH_LITERAL("aria-labelledby-heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByUpdates) {
  RunAriaTest(FILE_PATH_LITERAL("aria-labelledby-updates.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaLevel) {
  RunAriaTest(FILE_PATH_LITERAL("aria-level.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaLink) {
  RunAriaTest(FILE_PATH_LITERAL("aria-link.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaList) {
  RunAriaTest(FILE_PATH_LITERAL("aria-list.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaListBox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxActiveDescendant) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox-activedescendant.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxAriaSelected) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox-aria-selected.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaListBoxChildFocus) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listbox-childfocus.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaListItem) {
  RunAriaTest(FILE_PATH_LITERAL("aria-listitem.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaLive) {
  RunAriaTest(FILE_PATH_LITERAL("aria-live.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaLiveWithContent) {
  RunAriaTest(FILE_PATH_LITERAL("aria-live-with-content.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaLog) {
  RunAriaTest(FILE_PATH_LITERAL("aria-log.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMain) {
  RunAriaTest(FILE_PATH_LITERAL("aria-main.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMarquee) {
  RunAriaTest(FILE_PATH_LITERAL("aria-marquee.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMenu) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMenuBar) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menubar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMenuItem) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menuitem.html"));
}

// crbug.com/442278 will stop creating new text elements representing title.
// Re-baseline after the Blink change goes in
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaMenuItemCheckBox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menuitemcheckbox.html"));
}

// crbug.com/442278 will stop creating new text elements representing title.
// Re-baseline after the Blink change goes in
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaMenuItemRadio) {
  RunAriaTest(FILE_PATH_LITERAL("aria-menuitemradio.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaModal) {
  RunAriaTest(FILE_PATH_LITERAL("aria-modal.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMultiline) {
  RunAriaTest(FILE_PATH_LITERAL("aria-multiline.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaMultiselectable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-multiselectable.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaNavigation) {
  RunAriaTest(FILE_PATH_LITERAL("aria-navigation.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaNote) {
  RunAriaTest(FILE_PATH_LITERAL("aria-note.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaOrientation) {
  RunAriaTest(FILE_PATH_LITERAL("aria-orientation.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaOwns) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaOwnsList) {
  RunAriaTest(FILE_PATH_LITERAL("aria-owns-list.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMath) {
  RunAriaTest(FILE_PATH_LITERAL("aria-math.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaNone) {
  RunAriaTest(FILE_PATH_LITERAL("aria-none.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaOption) {
  RunAriaTest(FILE_PATH_LITERAL("aria-option.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaParagraph) {
  RunAriaTest(FILE_PATH_LITERAL("aria-paragraph.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaPosinset) {
  RunAriaTest(FILE_PATH_LITERAL("aria-posinset.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaPresentation) {
  RunAriaTest(FILE_PATH_LITERAL("aria-presentation.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaPressed) {
  RunAriaTest(FILE_PATH_LITERAL("aria-pressed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaProgressbar) {
  RunAriaTest(FILE_PATH_LITERAL("aria-progressbar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRadio) {
  RunAriaTest(FILE_PATH_LITERAL("aria-radio.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaRadiogroup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-radiogroup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaReadonly) {
  RunAriaTest(FILE_PATH_LITERAL("aria-readonly.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRegion) {
  RunAriaTest(FILE_PATH_LITERAL("aria-region.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRelevant) {
  RunAriaTest(FILE_PATH_LITERAL("aria-relevant.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRequired) {
  RunAriaTest(FILE_PATH_LITERAL("aria-required.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaRoleDescription) {
  RunAriaTest(FILE_PATH_LITERAL("aria-roledescription.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRow) {
  RunAriaTest(FILE_PATH_LITERAL("aria-row.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRowAttr) {
  RunAriaTest(FILE_PATH_LITERAL("aria-row-attr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRowGroup) {
  RunAriaTest(FILE_PATH_LITERAL("aria-rowgroup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRowHeader) {
  RunAriaTest(FILE_PATH_LITERAL("aria-rowheader.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaRowText) {
  RunAriaTest(FILE_PATH_LITERAL("aria-rowtext.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaScrollbar) {
  RunAriaTest(FILE_PATH_LITERAL("aria-scrollbar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSearch) {
  RunAriaTest(FILE_PATH_LITERAL("aria-search.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSearchbox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-searchbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityAriaSearchboxWithSelection) {
  RunAriaTest(FILE_PATH_LITERAL("aria-searchbox-with-selection.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSelected) {
  RunAriaTest(FILE_PATH_LITERAL("aria-selected.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSeparator) {
  RunAriaTest(FILE_PATH_LITERAL("aria-separator.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSetsize) {
  RunAriaTest(FILE_PATH_LITERAL("aria-setsize.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSlider) {
  RunAriaTest(FILE_PATH_LITERAL("aria-slider.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaSortOnAriaGrid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-sort-aria-grid.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaSortOnHtmlTable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-sort-html-table.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSpinButton) {
  RunAriaTest(FILE_PATH_LITERAL("aria-spinbutton.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaStatus) {
  RunAriaTest(FILE_PATH_LITERAL("aria-status.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaSwitch) {
  RunAriaTest(FILE_PATH_LITERAL("aria-switch.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTab) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tab.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTable) {
  RunAriaTest(FILE_PATH_LITERAL("aria-table.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTabList) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tablist.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTabPanel) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tabpanel.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTerm) {
  RunAriaTest(FILE_PATH_LITERAL("aria-term.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTextbox) {
  RunAriaTest(FILE_PATH_LITERAL("aria-textbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaTextboxWithRichText) {
  RunAriaTest(FILE_PATH_LITERAL("aria-textbox-with-rich-text.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityAriaTextboxWithSelection) {
  RunAriaTest(FILE_PATH_LITERAL("aria-textbox-with-selection.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTimer) {
  RunAriaTest(FILE_PATH_LITERAL("aria-timer.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaToggleButton) {
  RunAriaTest(FILE_PATH_LITERAL("aria-togglebutton.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaToolbar) {
  RunAriaTest(FILE_PATH_LITERAL("aria-toolbar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTooltip) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tooltip.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTree) {
  RunAriaTest(FILE_PATH_LITERAL("aria-tree.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaTreeGrid) {
  RunAriaTest(FILE_PATH_LITERAL("aria-treegrid.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaUndefined) {
  RunAriaTest(FILE_PATH_LITERAL("aria-undefined.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaUndefinedLiteral) {
  RunAriaTest(FILE_PATH_LITERAL("aria-undefined-literal.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaEmptyString) {
  RunAriaTest(FILE_PATH_LITERAL("aria-empty-string.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaValueMin) {
  RunAriaTest(FILE_PATH_LITERAL("aria-valuemin.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaValueMax) {
  RunAriaTest(FILE_PATH_LITERAL("aria-valuemax.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaValueNow) {
  RunAriaTest(FILE_PATH_LITERAL("aria-valuenow.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaValueText) {
  RunAriaTest(FILE_PATH_LITERAL("aria-valuetext.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputTextARIAPlaceholder) {
  RunAriaTest(FILE_PATH_LITERAL("input-text-aria-placeholder.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityTableColumnHidden) {
  RunAriaTest(FILE_PATH_LITERAL("table-column-hidden.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityLabelWithSelectedAriaOption) {
  RunAriaTest(FILE_PATH_LITERAL("label-with-selected-option.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityArticle) {
  RunHtmlTest(FILE_PATH_LITERAL("article.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAside) {
  RunHtmlTest(FILE_PATH_LITERAL("aside.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAudio) {
  RunHtmlTest(FILE_PATH_LITERAL("audio.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAWithImg) {
  RunHtmlTest(FILE_PATH_LITERAL("a-with-img.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityB) {
  RunHtmlTest(FILE_PATH_LITERAL("b.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBase) {
  RunHtmlTest(FILE_PATH_LITERAL("base.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBdo) {
  RunHtmlTest(FILE_PATH_LITERAL("bdo.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBlockquote) {
  RunHtmlTest(FILE_PATH_LITERAL("blockquote.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBody) {
  RunHtmlTest(FILE_PATH_LITERAL("body.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBodyTabIndex) {
  RunHtmlTest(FILE_PATH_LITERAL("body-tabindex.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBoundsInherits) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-inherits.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBoundsClips) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-clips.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBoundsAbsolute) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-absolute.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessiblitiyBoundsFixed) {
  RunHtmlTest(FILE_PATH_LITERAL("bounds-fixed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBR) {
  RunHtmlTest(FILE_PATH_LITERAL("br.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityButton) {
  RunHtmlTest(FILE_PATH_LITERAL("button.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityButtonAltChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("button-alt-changed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityButtonContentChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("button-content-changed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityButtonNameCalc) {
  RunHtmlTest(FILE_PATH_LITERAL("button-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCanvas) {
  RunHtmlTest(FILE_PATH_LITERAL("canvas.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCaption) {
  RunHtmlTest(FILE_PATH_LITERAL("caption.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityCharacterLocations) {
  RunHtmlTest(FILE_PATH_LITERAL("character-locations.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityCheckboxNameCalc) {
  RunHtmlTest(FILE_PATH_LITERAL("checkbox-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCite) {
  RunHtmlTest(FILE_PATH_LITERAL("cite.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCode) {
  RunHtmlTest(FILE_PATH_LITERAL("code.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCol) {
  RunHtmlTest(FILE_PATH_LITERAL("col.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityColgroup) {
  RunHtmlTest(FILE_PATH_LITERAL("colgroup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDd) {
  RunHtmlTest(FILE_PATH_LITERAL("dd.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDel) {
  RunHtmlTest(FILE_PATH_LITERAL("del.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDetails) {
  RunHtmlTest(FILE_PATH_LITERAL("details.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDfn) {
  RunHtmlTest(FILE_PATH_LITERAL("dfn.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDialog) {
  RunHtmlTest(FILE_PATH_LITERAL("dialog.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDiv) {
  RunHtmlTest(FILE_PATH_LITERAL("div.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDl) {
  RunHtmlTest(FILE_PATH_LITERAL("dl.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDt) {
  RunHtmlTest(FILE_PATH_LITERAL("dt.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDpubRoles) {
  RunAriaTest(FILE_PATH_LITERAL("dpub-roles.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityGraphicsRoles) {
  RunAriaTest(FILE_PATH_LITERAL("graphics-roles.html"));
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Flaky failures: http://crbug.com/445929.
// Mac failures: http://crbug.com/571712.
#define MAYBE_AccessibilityContenteditableDescendants \
    DISABLED_AccessibilityContenteditableDescendants
#else
#define MAYBE_AccessibilityContenteditableDescendants \
    AccessibilityContenteditableDescendants
#endif
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityContenteditableDescendants) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-descendants.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityElementClassIdSrcAttr) {
  RunHtmlTest(FILE_PATH_LITERAL("element-class-id-src-attr.html"));
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Flaky failures: http://crbug.com/445929.
// Mac failures: http://crbug.com/571712.
#define MAYBE_AccessibilityContenteditableDescendantsWithSelection \
    DISABLED_AccessibilityContenteditableDescendantsWithSelection
#else
#define MAYBE_AccessibilityContenteditableDescendantsWithSelection \
    AccessibilityContenteditableDescendantsWithSelection
#endif
IN_PROC_BROWSER_TEST_F(
    DumpAccessibilityTreeTest,
    MAYBE_AccessibilityContenteditableDescendantsWithSelection) {
  RunHtmlTest(FILE_PATH_LITERAL(
      "contenteditable-descendants-with-selection.html"));
}

#if defined(OS_ANDROID)
// Flaky failures: http://crbug.com/445929.
#define MAYBE_AccessibilityContenteditableWithEmbeddedContenteditables \
    DISABLED_AccessibilityContenteditableWithEmbeddedContenteditables
#else
#define MAYBE_AccessibilityContenteditableWithEmbeddedContenteditables \
    AccessibilityContenteditableWithEmbeddedContenteditables
#endif
IN_PROC_BROWSER_TEST_F(
    DumpAccessibilityTreeTest,
    MAYBE_AccessibilityContenteditableWithEmbeddedContenteditables) {
  RunHtmlTest(
      FILE_PATH_LITERAL("contenteditable-with-embedded-contenteditables.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableWithNoDescendants) {
  RunHtmlTest(FILE_PATH_LITERAL("contenteditable-with-no-descendants.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityEm) {
  RunHtmlTest(FILE_PATH_LITERAL("em.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityEmbed) {
  RunHtmlTest(FILE_PATH_LITERAL("embed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFieldset) {
  RunHtmlTest(FILE_PATH_LITERAL("fieldset.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFigcaption) {
  RunHtmlTest(FILE_PATH_LITERAL("figcaption.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFigure) {
  RunHtmlTest(FILE_PATH_LITERAL("figure.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFooter) {
  RunHtmlTest(FILE_PATH_LITERAL("footer.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
    AccessibilityFooterInsideOtherSection) {
  RunHtmlTest(FILE_PATH_LITERAL("footer-inside-other-section.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityForm) {
  RunHtmlTest(FILE_PATH_LITERAL("form.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFrameset) {
  RunHtmlTest(FILE_PATH_LITERAL("frameset.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityFramesetPostEnable) {
  enable_accessibility_after_navigating_ = true;
  RunHtmlTest(FILE_PATH_LITERAL("frameset.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHead) {
  RunHtmlTest(FILE_PATH_LITERAL("head.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHeader) {
  RunHtmlTest(FILE_PATH_LITERAL("header.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
    AccessibilityHeaderInsideOtherSection) {
  RunHtmlTest(FILE_PATH_LITERAL("header-inside-other-section.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHeading) {
  RunHtmlTest(FILE_PATH_LITERAL("heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHR) {
  RunHtmlTest(FILE_PATH_LITERAL("hr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHTML) {
  RunHtmlTest(FILE_PATH_LITERAL("html.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityI) {
  RunHtmlTest(FILE_PATH_LITERAL("i.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityIframe) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframePostEnable) {
  enable_accessibility_after_navigating_ = true;
  RunHtmlTest(FILE_PATH_LITERAL("iframe.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeCrossProcess) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-cross-process.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeCoordinates) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-coordinates.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeCoordinatesCrossProcess) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-coordinates-cross-process.html"));
}
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityIframePadding) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-padding.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframePresentational) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-presentational.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransform) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransformCrossProcess) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform-cross-process.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransformNested) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform-nested.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransformNestedCrossProcess) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform-nested-cross-process.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeTransformScrolled) {
  RunHtmlTest(FILE_PATH_LITERAL("iframe-transform-scrolled.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityImg) {
  RunHtmlTest(FILE_PATH_LITERAL("img.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityImgEmptyAlt) {
  RunHtmlTest(FILE_PATH_LITERAL("img-empty-alt.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityImgLinkEmptyAlt) {
  RunHtmlTest(FILE_PATH_LITERAL("img-link-empty-alt.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInPageLinks) {
  RunHtmlTest(FILE_PATH_LITERAL("in-page-links.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputButton) {
  RunHtmlTest(FILE_PATH_LITERAL("input-button.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputButtonInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-button-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputCheckBox) {
  RunHtmlTest(FILE_PATH_LITERAL("input-checkbox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputCheckBoxInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-checkbox-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputCheckBoxLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("input-checkbox-label.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputColor) {
  RunHtmlTest(FILE_PATH_LITERAL("input-color.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputDate) {
  RunHtmlTest(FILE_PATH_LITERAL("input-date.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputDateTime) {
  RunHtmlTest(FILE_PATH_LITERAL("input-datetime.html"));
}

// Fails on OS X 10.9 and higher <https://crbug.com/430622>.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputDateTimeLocal) {
  RunHtmlTest(FILE_PATH_LITERAL("input-datetime-local.html"));
}
#endif

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputEmail) {
  RunHtmlTest(FILE_PATH_LITERAL("input-email.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputFile) {
  RunHtmlTest(FILE_PATH_LITERAL("input-file.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputHidden) {
  RunHtmlTest(FILE_PATH_LITERAL("input-hidden.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputImage) {
  RunHtmlTest(FILE_PATH_LITERAL("input-image.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputImageButtonInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-image-button-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputList) {
  RunHtmlTest(FILE_PATH_LITERAL("input-list.html"));
}

// crbug.com/423675 - AX tree is different for Win7 and Win8.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityInputMonth) {
  RunHtmlTest(FILE_PATH_LITERAL("input-month.html"));
}
#else
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputMonth) {
  RunHtmlTest(FILE_PATH_LITERAL("input-month.html"));
}
#endif

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputNumber) {
  RunHtmlTest(FILE_PATH_LITERAL("input-number.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputPassword) {
  RunHtmlTest(FILE_PATH_LITERAL("input-password.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputRadio) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                      AccessibilityInputRadioInMenu) {
  RunHtmlTest(FILE_PATH_LITERAL("input-radio-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputRange) {
  RunHtmlTest(FILE_PATH_LITERAL("input-range.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputReset) {
  RunHtmlTest(FILE_PATH_LITERAL("input-reset.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputSearch) {
  RunHtmlTest(FILE_PATH_LITERAL("input-search.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityScrollableInput) {
  RunHtmlTest(FILE_PATH_LITERAL("scrollable-input.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityScrollableOverflow) {
  RunHtmlTest(FILE_PATH_LITERAL("scrollable-overflow.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityScrollableTextarea) {
  RunHtmlTest(FILE_PATH_LITERAL("scrollable-textarea.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySmall) {
  RunHtmlTest(FILE_PATH_LITERAL("small.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputSubmit) {
  RunHtmlTest(FILE_PATH_LITERAL("input-submit.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputSuggestionsSourceElement) {
  RunHtmlTest(FILE_PATH_LITERAL("input-suggestions-source-element.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputTel) {
  RunHtmlTest(FILE_PATH_LITERAL("input-tel.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputText) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputTextReadOnly) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-read-only.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputTextNameCalc) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputTextValue) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-value.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputTextValueChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-value-changed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
    AccessibilityInputTextWithSelection) {
  RunHtmlTest(FILE_PATH_LITERAL("input-text-with-selection.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputTime) {
  RunHtmlTest(FILE_PATH_LITERAL("input-time.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputTypes) {
  RunHtmlTest(FILE_PATH_LITERAL("input-types.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputUrl) {
  RunHtmlTest(FILE_PATH_LITERAL("input-url.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputWeek) {
  RunHtmlTest(FILE_PATH_LITERAL("input-week.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityId) {
  RunHtmlTest(FILE_PATH_LITERAL("id.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityIns) {
  RunHtmlTest(FILE_PATH_LITERAL("ins.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("label.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLabelUpdates) {
  RunHtmlTest(FILE_PATH_LITERAL("label-updates.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLandmark) {
  RunHtmlTest(FILE_PATH_LITERAL("landmark.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityLayoutTableInButton) {
  RunHtmlTest(FILE_PATH_LITERAL("layout-table-in-button.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLegend) {
  RunHtmlTest(FILE_PATH_LITERAL("legend.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLi) {
  RunHtmlTest(FILE_PATH_LITERAL("li.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLink) {
  RunHtmlTest(FILE_PATH_LITERAL("link.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityLinkInsideHeading) {
  RunHtmlTest(FILE_PATH_LITERAL("link-inside-heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityList) {
  RunHtmlTest(FILE_PATH_LITERAL("list.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityListMarkers) {
  RunHtmlTest(FILE_PATH_LITERAL("list-markers.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMain) {
  RunHtmlTest(FILE_PATH_LITERAL("main.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMark) {
  RunHtmlTest(FILE_PATH_LITERAL("mark.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMath) {
  RunHtmlTest(FILE_PATH_LITERAL("math.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityMenutypecontext) {
  RunHtmlTest(FILE_PATH_LITERAL("menu-type-context.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMeta) {
  RunHtmlTest(FILE_PATH_LITERAL("meta.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityMeter) {
  RunHtmlTest(FILE_PATH_LITERAL("meter.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogClosed) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-closed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogOpened) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-opened.html"));
}

// http://crbug.com/738497
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityModalDialogInIframeClosed) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-closed.html"));
}

// Disabled because it is flaky in several platforms
/*
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogInIframeOpened) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-opened.html"));
}
*/

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogStack) {
  RunHtmlTest(FILE_PATH_LITERAL("modal-dialog-stack.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityNavigation) {
  RunHtmlTest(FILE_PATH_LITERAL("navigation.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityNoscript) {
  RunHtmlTest(FILE_PATH_LITERAL("noscript.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityOl) {
  RunHtmlTest(FILE_PATH_LITERAL("ol.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityObject) {
  RunHtmlTest(FILE_PATH_LITERAL("object.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityOffscreen) {
  RunHtmlTest(FILE_PATH_LITERAL("offscreen.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityOffscreenScroll) {
  RunHtmlTest(FILE_PATH_LITERAL("offscreen-scroll.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityOptgroup) {
  RunHtmlTest(FILE_PATH_LITERAL("optgroup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityOptionindatalist) {
  RunHtmlTest(FILE_PATH_LITERAL("option-in-datalist.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityOutput) {
  RunHtmlTest(FILE_PATH_LITERAL("output.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityP) {
  RunHtmlTest(FILE_PATH_LITERAL("p.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityParam) {
  RunHtmlTest(FILE_PATH_LITERAL("param.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityPre) {
  RunHtmlTest(FILE_PATH_LITERAL("pre.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityProgress) {
  RunHtmlTest(FILE_PATH_LITERAL("progress.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityQ) {
  RunHtmlTest(FILE_PATH_LITERAL("q.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityReparentCrash) {
  RunHtmlTest(FILE_PATH_LITERAL("reparent-crash.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityReplaceData) {
  RunHtmlTest(FILE_PATH_LITERAL("replace-data.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityRuby) {
  RunHtmlTest(FILE_PATH_LITERAL("ruby.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityS) {
  RunHtmlTest(FILE_PATH_LITERAL("s.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySamp) {
  RunHtmlTest(FILE_PATH_LITERAL("samp.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityScript) {
  RunHtmlTest(FILE_PATH_LITERAL("script.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySection) {
  RunHtmlTest(FILE_PATH_LITERAL("section.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySelect) {
  RunHtmlTest(FILE_PATH_LITERAL("select.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySource) {
  RunHtmlTest(FILE_PATH_LITERAL("source.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySpan) {
  RunHtmlTest(FILE_PATH_LITERAL("span.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySpanLineBreak) {
  RunHtmlTest(FILE_PATH_LITERAL("span-line-break.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityStrong) {
  RunHtmlTest(FILE_PATH_LITERAL("strong.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityStyle) {
  RunHtmlTest(FILE_PATH_LITERAL("style.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySub) {
  RunHtmlTest(FILE_PATH_LITERAL("sub.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySup) {
  RunHtmlTest(FILE_PATH_LITERAL("sup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySummary) {
  RunHtmlTest(FILE_PATH_LITERAL("summary.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySvg) {
  RunHtmlTest(FILE_PATH_LITERAL("svg.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTableSimple) {
  RunHtmlTest(FILE_PATH_LITERAL("table-simple.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTableLayout) {
  RunHtmlTest(FILE_PATH_LITERAL("table-layout.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityTablePresentation) {
  RunHtmlTest(FILE_PATH_LITERAL("table-presentation.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityTableThColHeader) {
  RunHtmlTest(FILE_PATH_LITERAL("table-th-colheader.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityTableThRowHeader) {
  RunHtmlTest(FILE_PATH_LITERAL("table-th-rowheader.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityTableTbodyTfoot) {
  RunHtmlTest(FILE_PATH_LITERAL("table-thead-tbody-tfoot.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTableSpans) {
  RunHtmlTest(FILE_PATH_LITERAL("table-spans.html"));
}



IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTextarea) {
  RunHtmlTest(FILE_PATH_LITERAL("textarea.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityTextareaReadOnly) {
  RunHtmlTest(FILE_PATH_LITERAL("textarea-read-only.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityTextareaWithSelection) {
  RunHtmlTest(FILE_PATH_LITERAL("textarea-with-selection.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTime) {
  RunHtmlTest(FILE_PATH_LITERAL("time.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTitle) {
  RunHtmlTest(FILE_PATH_LITERAL("title.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTitleChanged) {
  RunHtmlTest(FILE_PATH_LITERAL("title-changed.html"));
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// Flaky on Win/Mac: crbug.com/508532
#define MAYBE_AccessibilityTransition DISABLED_AccessibilityTransition
#else
#define MAYBE_AccessibilityTransition AccessibilityTransition
#endif
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       MAYBE_AccessibilityTransition) {
  RunHtmlTest(FILE_PATH_LITERAL("transition.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTruncateLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("truncate-label.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityUl) {
  RunHtmlTest(FILE_PATH_LITERAL("ul.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityVar) {
  RunHtmlTest(FILE_PATH_LITERAL("var.html"));
}

// crbug.com/281952
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, DISABLED_AccessibilityVideo) {
  RunHtmlTest(FILE_PATH_LITERAL("video.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityWbr) {
  RunHtmlTest(FILE_PATH_LITERAL("wbr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityWindowCropsItems) {
  RunHtmlTest(FILE_PATH_LITERAL("window-crops-items.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputInsideLabel) {
  RunHtmlTest(FILE_PATH_LITERAL("input-inside-label.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputImageWithTitle) {
  RunHtmlTest(FILE_PATH_LITERAL("input-image-with-title.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityLabelWithSelectedOption) {
  RunHtmlTest(FILE_PATH_LITERAL("label-with-selected-option.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityLabelWithPresentationalChild) {
  RunHtmlTest(FILE_PATH_LITERAL("label-with-presentational-child.html"));
}

//
// Regression tests. These don't test a specific web platform feature,
// they test a specific web page that crashed or had some bad behavior
// in the past.
//

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, XmlInIframeCrash) {
  RunRegressionTest(FILE_PATH_LITERAL("xml-in-iframe-crash.html"));
}

}  // namespace content
