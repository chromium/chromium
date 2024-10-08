// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.AccessibilityContentShellActivityTestRule.NODE_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellActivityTestRule.RESULTS_NULL;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sClassNameMatcher;

import android.annotation.SuppressLint;
import android.os.Build.VERSION_CODES;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.TestAnimations;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for WebContentsAccessibilityImpl integration with accessibility services. */
@RunWith(ContentJUnit4ClassRunner.class)
@SuppressLint("VisibleForTests")
@Batch(Batch.PER_CLASS)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
@DisableFeatures(ContentFeatureList.ACCESSIBILITY_UNIFIED_SNAPSHOTS)
@TestAnimations.EnableAnimations
public class WebContentsAccessibilityTreeTest {
    // File path that holds all the relevant tests.
    private static final String BASE_ACCNAME_FILE_PATH = "content/test/data/accessibility/accname/";
    private static final String BASE_ARIA_FILE_PATH = "content/test/data/accessibility/aria/";
    private static final String BASE_CSS_FILE_PATH = "content/test/data/accessibility/css/";
    private static final String BASE_HTML_FILE_PATH = "content/test/data/accessibility/html/";
    private static final String DEFAULT_FILE_SUFFIX = "-expected-android-external.txt";
    private static final String ASSIST_DATA_FILE_SUFFIX = "-expected-android-assist-data.txt";

    // Debug flag to print bounding boxes etc which are normally excluded in test outputs.
    private static final boolean sIncludeScreenSizeDependentAttributes = false;

    @Rule
    public AccessibilityContentShellActivityTestRule mActivityTestRule =
            new AccessibilityContentShellActivityTestRule();

    /**
     * Perform a single test which will:
     *     1. Open the given HTML file
     *     2. Generate the full AccessibilityNodeInfo tree
     *     3. Generate the full AssistData tree
     *     4. Read expectations file and compare both trees with results
     *
     * @param inputFile HTML test input file
     * @param expectationFile TXT expectations file
     * @param expectationFilePath directory that holds the test files
     */
    private void performTest(String inputFile, String expectationFile, String expectationFilePath) {
        // Build page from given file and enable testing framework.
        mActivityTestRule.setupTestFromFile(expectationFilePath + inputFile);

        // Create extra strings to print to logs along with potential error(s) for rebase tool.
        String accessibilityNodeInfoErrorPrefix =
                String.format(
                        "\n\nTesting: %s%s\nExpected output: %s%s",
                        expectationFilePath,
                        inputFile,
                        expectationFilePath,
                        expectationFile + DEFAULT_FILE_SUFFIX);
        // String assistDataErrorPrefix =
        //         String.format(
        //                 "\n\nTesting: %s%s\nExpected output: %s%s",
        //                 expectationFilePath,
        //                 inputFile,
        //                 expectationFilePath,
        //                 expectationFile + ASSIST_DATA_FILE_SUFFIX);

        // Generate full AccessibilityNodeInfo and AssistData trees
        String accessibilityNodeInfoTree = generateAccessibilityNodeInfoTree();
        String assistDataTree = generateViewStructureTree();
        Assert.assertNotNull(RESULTS_NULL, accessibilityNodeInfoTree);
        Assert.assertNotNull(RESULTS_NULL, assistDataTree);

        // Attempt to read expectation files (will throw an error if files do not exist).
        String accessibilityNodeInfoTreeExpectedResults =
                mActivityTestRule
                        .readExpectationFile(
                                expectationFilePath + expectationFile + DEFAULT_FILE_SUFFIX)
                        .trim();
        // String assistDataTreeExpectedResults =
        //         mActivityTestRule
        //                 .readExpectationFile(
        //                         expectationFilePath + expectationFile + ASSIST_DATA_FILE_SUFFIX)
        //                 .trim();

        // We want to test both trees so that the rebase tree only needs to be run once for newly
        // added tests, so we will first check equivalency without using Assert and create an error
        // message that can include results for both tree tests.
        String outputError = "";
        if (!accessibilityNodeInfoTree.equals(accessibilityNodeInfoTreeExpectedResults)) {
            outputError +=
                    NODE_ERROR
                            + accessibilityNodeInfoErrorPrefix
                            + "\n\nExpected\n--------\n"
                            + accessibilityNodeInfoTreeExpectedResults
                            + "\n\nActual\n------\n"
                            + accessibilityNodeInfoTree
                            + "\n<-- End-of-file -->\n\n\n";
        }
        // TODO(mschillaci): Re-enable once full unification path is complete.
        // if (!assistDataTree.equals(assistDataTreeExpectedResults)) {
        //     outputError +=
        //             NODE_ERROR
        //                     + assistDataErrorPrefix
        //                     + "\n\nExpected\n--------\n"
        //                     + assistDataTreeExpectedResults
        //                     + "\n\nActual\n------\n"
        //                     + assistDataTree
        //                     + "\n<-- End-of-file -->\n\n\n";
        // }

        // Assert expectations and print error if needed.
        Assert.assertEquals(
                outputError, accessibilityNodeInfoTreeExpectedResults, accessibilityNodeInfoTree);
        // TODO(mschillaci): Re-enable once full unification path is complete.
        // Assert.assertEquals(outputError, assistDataTreeExpectedResults, assistDataTree);
    }

    // Helper methods to pass-through to the performTest method so each individual test does
    // not need to include its own filepath.
    private void performAccnameTest(String input) {
        // Remove the '.html' from the input file, and append the standard suffix.
        performAccnameTest(input, input.substring(0, input.length() - 5));
    }

    private void performAccnameTest(String inputFile, String expectationFile) {
        performTest(inputFile, expectationFile, BASE_ACCNAME_FILE_PATH);
    }

    private void performAriaTest(String input) {
        // Remove the '.html' from the input file, and append the standard suffix.
        performAriaTest(input, input.substring(0, input.length() - 5));
    }

    private void performAriaTest(String inputFile, String expectationFile) {
        performTest(inputFile, expectationFile, BASE_ARIA_FILE_PATH);
    }

    private void performCssTest(String input) {
        // Remove the '.html' from the input file, and append the standard suffix.
        performCssTest(input, input.substring(0, input.length() - 5));
    }

    private void performCssTest(String inputFile, String expectationFile) {
        performTest(inputFile, expectationFile, BASE_CSS_FILE_PATH);
    }

    private void performHtmlTest(String input) {
        // Remove the '.html' from the input file, and append the standard suffix.
        performHtmlTest(input, input.substring(0, input.length() - 5));
    }

    private void performHtmlTest(String inputFile, String expectationFile) {
        performTest(inputFile, expectationFile, BASE_HTML_FILE_PATH);
    }

    /**
     * Generate the full AccessibilityNodeInfo tree as a String of text.
     *
     * @return String The AccessibilityNodeInfo tree in text form
     */
    private String generateAccessibilityNodeInfoTree() {
        StringBuilder builder = new StringBuilder();

        // Find the root node and generate its string.
        int rootNodevvId =
                mActivityTestRule.waitForNodeMatching(sClassNameMatcher, "android.webkit.WebView");
        AccessibilityNodeInfoCompat nodeInfo = createAccessibilityNodeInfo(rootNodevvId);
        builder.append(
                AccessibilityNodeInfoUtils.toString(
                        nodeInfo, sIncludeScreenSizeDependentAttributes));

        // Recursively generate strings for all descendants.
        for (int i = 0; i < nodeInfo.getChildCount(); ++i) {
            int childId = mActivityTestRule.getChildId(nodeInfo, i);
            AccessibilityNodeInfoCompat childNodeInfo = createAccessibilityNodeInfo(childId);
            recursivelyFormatTree(childNodeInfo, builder, "++");
        }

        return builder.toString();
    }

    private String generateViewStructureTree() {
        TestViewStructure testViewStructure = new TestViewStructure();
        testViewStructure.setShouldIncludeScreenSizeDependentAttributes(
                sIncludeScreenSizeDependentAttributes);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.mWcax.onProvideVirtualStructure(testViewStructure, false));
        CriteriaHelper.pollUiThread(
                mActivityTestRule.mWcax::hasFinishedLatestAccessibilitySnapshotForTesting,
                "Failed to get AssistData.");
        return testViewStructure.toString();
    }

    /**
     * Recursively add AccessibilityNodeInfo descendants to the given builder.
     *
     * @param node Given object to print all descendants for
     * @param builder builder to add generated Strings to
     * @param indent prefix to indent each generation, e.g. "++"
     */
    private void recursivelyFormatTree(
            AccessibilityNodeInfoCompat node, StringBuilder builder, String indent) {
        builder.append("\n")
                .append(indent)
                .append(
                        AccessibilityNodeInfoUtils.toString(
                                node, sIncludeScreenSizeDependentAttributes));
        for (int j = 0; j < node.getChildCount(); ++j) {
            int childId = mActivityTestRule.getChildId(node, j);
            AccessibilityNodeInfoCompat childNodeInfo = createAccessibilityNodeInfo(childId);
            recursivelyFormatTree(childNodeInfo, builder, indent + "++");
        }
    }

    // Helper method to create an AccessibilityNodeInfo object.
    private AccessibilityNodeInfoCompat createAccessibilityNodeInfo(int virtualViewId) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.mNodeProvider.createAccessibilityNodeInfo(virtualViewId));
    }

    // ------------------ ACCNAME TESTS ------------------ //

    @Test
    @SmallTest
    public void test_descComboboxFocusable() {
        performAccnameTest("desc-combobox-focusable.html");
    }

    @Test
    @SmallTest
    public void test_descFromContentOfDescribedbyElement() {
        performAccnameTest("desc-from-content-of-describedby-element.html");
    }

    @Test
    @SmallTest
    public void test_nameComboboxFocusable() {
        performAccnameTest("name-combobox-focusable.html");
    }

    @Test
    @SmallTest
    public void test_nameDivContentOnly() {
        performAccnameTest("name-div-content-only.html");
    }

    // ------------------ ARIA TESTS ------------------ //

    @Test
    @SmallTest
    public void test_annotationRoles() {
        performAriaTest("annotation-roles.html");
    }

    @Test
    @SmallTest
    public void test_ariaAlertdialog() {
        performAriaTest("aria-alertdialog.html");
    }

    @Test
    @SmallTest
    public void test_ariaAlert() {
        performAriaTest("aria-alert.html");
    }

    @Test
    @SmallTest
    public void test_ariaApplication() {
        performAriaTest("aria-application.html");
    }

    @Test
    @SmallTest
    public void test_ariaArticle() {
        performAriaTest("aria-article.html");
    }

    @Test
    @SmallTest
    public void test_ariaAtomic() {
        performAriaTest("aria-atomic.html");
    }

    @Test
    @SmallTest
    public void test_ariaAutocomplete() {
        performAriaTest("aria-autocomplete.html");
    }

    @Test
    @SmallTest
    public void test_ariaBanner() {
        performAriaTest("aria-banner.html");
    }

    @Test
    @SmallTest
    public void test_ariaBrailleLabel() {
        performAriaTest("aria-braillelabel.html");
    }

    @Test
    @SmallTest
    public void test_ariaBrailleRoleDescription() {
        performAriaTest("aria-brailleroledescription.html");
    }

    @Test
    @SmallTest
    public void test_ariaBusy() {
        performAriaTest("aria-busy.html");
    }

    @Test
    @SmallTest
    public void test_ariaButton() {
        performAriaTest("aria-button.html");
    }

    @Test
    @SmallTest
    public void test_ariaCell() {
        performAriaTest("aria-cell.html");
    }

    @Test
    @SmallTest
    public void test_ariaCheckbox() {
        performAriaTest("aria-checkbox.html");
    }

    @Test
    @SmallTest
    public void test_ariaChecked() {
        performAriaTest("aria-checked.html");
    }

    @Test
    @SmallTest
    public void test_ariaCode() {
        performAriaTest("aria-code.html");
    }

    @Test
    @SmallTest
    public void test_ariaColAttr() {
        performAriaTest("aria-col-attr.html");
    }

    @Test
    @SmallTest
    public void test_ariaColRowIndex() {
        performAriaTest("aria-col-row-index.html");
    }

    @Test
    @SmallTest
    public void test_ariaColRowIndexUndefined() {
        performAriaTest("aria-col-row-index-undefined.html");
    }

    @Test
    @SmallTest
    public void test_ariaColumnheader() {
        performAriaTest("aria-columnheader.html");
    }

    @Test
    @SmallTest
    public void test_ariaCombobox() {
        performAriaTest("aria-combobox.html");
    }

    @Test
    @SmallTest
    public void test_ariaComboboxImplicitHaspopup() {
        performAriaTest("aria-combobox-implicit-haspopup.html");
    }

    @Test
    @SmallTest
    public void test_ariaComboboxUneditable() {
        performAriaTest("aria-combobox-uneditable.html");
    }

    @Test
    @SmallTest
    public void test_ariaComplementary() {
        performAriaTest("aria-complementary.html");
    }

    @Test
    @SmallTest
    public void test_ariaContentinfo() {
        performAriaTest("aria-contentinfo.html");
    }

    @Test
    @SmallTest
    public void test_ariaControls() {
        performAriaTest("aria-controls.html");
    }

    @Test
    @SmallTest
    public void test_ariaCurrent() {
        performAriaTest("aria-current.html");
    }

    @Test
    @SmallTest
    public void test_ariaDefinition() {
        performAriaTest("aria-definition.html");
    }

    @Test
    @SmallTest
    public void test_ariaDescribedby() {
        performAriaTest("aria-describedby.html");
    }

    @Test
    @SmallTest
    public void test_ariaDescription() {
        performAriaTest("aria-description.html");
    }

    @Test
    @SmallTest
    public void test_ariaDetails() {
        performAriaTest("aria-details.html");
    }

    @Test
    @SmallTest
    public void test_ariaDetailsMultiple() {
        performAriaTest("aria-details-multiple.html");
    }

    @Test
    @SmallTest
    public void test_ariaDialog() {
        performAriaTest("aria-dialog.html");
    }

    @Test
    @SmallTest
    public void test_ariaDirectory() {
        performAriaTest("aria-directory.html");
    }

    @Test
    @SmallTest
    public void test_ariaDisabled() {
        performAriaTest("aria-disabled.html");
    }

    @Test
    @SmallTest
    public void test_ariaDocument() {
        performAriaTest("aria-document.html");
    }

    @Test
    @SmallTest
    public void test_ariaEmphasis() {
        performAriaTest("aria-emphasis.html");
    }

    @Test
    @SmallTest
    public void test_ariaErrormessage() {
        performAriaTest("aria-errormessage.html");
    }

    @Test
    @SmallTest
    public void test_ariaExpanded() {
        performAriaTest("aria-expanded.html");
    }

    @Test
    @SmallTest
    public void test_ariaFigure() {
        performAriaTest("aria-figure.html");
    }

    @Test
    @SmallTest
    public void test_ariaFlowto() {
        performAriaTest("aria-flowto.html");
    }

    @Test
    @SmallTest
    public void test_ariaForm() {
        performAriaTest("aria-form.html");
    }

    @Test
    @SmallTest
    public void test_ariaGeneric() {
        performAriaTest("aria-generic.html");
    }

    @Test
    @SmallTest
    public void test_ariaGridcell() {
        performAriaTest("aria-gridcell.html");
    }

    @Test
    @SmallTest
    public void test_ariaGrid() {
        performAriaTest("aria-grid.html");
    }

    @Test
    @SmallTest
    public void test_ariaGroup() {
        performAriaTest("aria-group.html");
    }

    @Test
    @SmallTest
    public void test_ariaHaspopup() {
        performAriaTest("aria-haspopup.html");
    }

    @Test
    @SmallTest
    public void test_ariaHeading() {
        performAriaTest("aria-heading.html");
    }

    @Test
    @SmallTest
    public void test_ariaHiddenDescribedBy() {
        performAriaTest("aria-hidden-described-by.html");
    }

    @Test
    @SmallTest
    public void test_ariaHidden() {
        performAriaTest("aria-hidden.html");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1286036")
    public void test_ariaHiddenIframe() {
        performAriaTest("aria-hidden-iframe.html");
    }

    @Test
    @SmallTest
    public void test_ariaHiddenLabelledBy() {
        performAriaTest("aria-hidden-labelled-by.html");
    }

    @Test
    @SmallTest
    public void test_ariaIllegalVal() {
        performAriaTest("aria-illegal-val.html");
    }

    @Test
    @SmallTest
    public void test_ariaImgChild() {
        performAriaTest("aria-img-child.html");
    }

    @Test
    @SmallTest
    public void test_ariaImg() {
        performAriaTest("aria-img.html");
    }

    @Test
    @SmallTest
    public void test_ariaInsertionDeletion() {
        performAriaTest("aria-insertion-deletion.html");
    }

    @Test
    @SmallTest
    public void test_ariaKeyshortcuts() {
        performAriaTest("aria-keyshortcuts.html");
    }

    @Test
    @SmallTest
    public void test_ariaLabel() {
        performAriaTest("aria-label.html");
    }

    @Test
    @SmallTest
    public void test_ariaLabelWithTabIndex() {
        performAriaTest("aria-label-with-tabindex.html");
    }

    @Test
    @SmallTest
    public void test_ariaLabelAugmentInnerText() {
        performAriaTest("aria-label-augment-inner-text.html");
    }

    @Test
    @SmallTest
    public void test_ariaLabelledbyHeading() {
        performAriaTest("aria-labelledby-heading.html");
    }

    @Test
    @SmallTest
    public void test_ariaListboxAriaSelected() {
        performAriaTest("aria-listbox-aria-selected.html");
    }

    @Test
    @SmallTest
    public void test_ariaListboxDisabled() {
        performAriaTest("aria-listbox-disabled.html");
    }

    @Test
    @SmallTest
    public void test_ariaListbox() {
        performAriaTest("aria-listbox.html");
    }

    @Test
    @SmallTest
    public void test_ariaList() {
        performAriaTest("aria-list.html");
    }

    @Test
    @SmallTest
    public void test_ariaListitem() {
        performAriaTest("aria-listitem.html");
    }

    @Test
    @SmallTest
    public void test_ariaLive() {
        performAriaTest("aria-live.html");
    }

    @Test
    @SmallTest
    public void test_ariaLiveWithContent() {
        performAriaTest("aria-live-with-content.html");
    }

    @Test
    @SmallTest
    public void test_ariaLog() {
        performAriaTest("aria-log.html");
    }

    @Test
    @SmallTest
    public void test_ariaMain() {
        performAriaTest("aria-main.html");
    }

    @Test
    @SmallTest
    public void test_ariaMarquee() {
        performAriaTest("aria-marquee.html");
    }

    @Test
    @SmallTest
    public void test_ariaMath() {
        performAriaTest("aria-math.html");
    }

    @Test
    @SmallTest
    public void test_ariaMenubar() {
        performAriaTest("aria-menubar.html");
    }

    @Test
    @SmallTest
    public void test_ariaMenu() {
        performAriaTest("aria-menu.html");
    }

    @Test
    @SmallTest
    public void test_ariaMenuitemcheckbox() {
        performAriaTest("aria-menuitemcheckbox.html");
    }

    @Test
    @SmallTest
    public void test_ariaMenuitem() {
        performAriaTest("aria-menuitem.html");
    }

    @Test
    @SmallTest
    public void test_ariaMenuitemInGroup() {
        performAriaTest("aria-menuitem-in-group.html");
    }

    @Test
    @SmallTest
    public void test_ariaMenuitemradio() {
        performAriaTest("aria-menuitemradio.html");
    }

    @Test
    @SmallTest
    public void test_ariaMeter() {
        performAriaTest("aria-meter.html");
    }

    @Test
    @SmallTest
    public void test_ariaMismatchedTableAttr() {
        performAriaTest("aria-mismatched-table-attr.html");
    }

    @Test
    @SmallTest
    public void test_ariaMultiline() {
        performAriaTest("aria-multiline.html");
    }

    @Test
    @SmallTest
    public void test_ariaMultiselectable() {
        performAriaTest("aria-multiselectable.html");
    }

    @Test
    @SmallTest
    public void test_ariaNavigation() {
        performAriaTest("aria-navigation.html");
    }

    @Test
    @SmallTest
    public void test_ariaNone() {
        performAriaTest("aria-none.html");
    }

    @Test
    @SmallTest
    public void test_ariaNote() {
        performAriaTest("aria-note.html");
    }

    @Test
    @SmallTest
    public void test_ariaOptionComplexChildren() {
        performAriaTest("aria-option-complex-children.html");
    }

    @Test
    @SmallTest
    @DisableIf.Build(supported_abis_includes = "x86_64", message = "https://crbug.com/349962563")
    public void test_ariaOption() {
        performAriaTest("aria-option.html");
    }

    @Test
    @SmallTest
    public void test_ariaOrientation() {
        performAriaTest("aria-orientation.html");
    }

    @Test
    @SmallTest
    public void test_ariaOwnsFromDisplayNone() {
        performAriaTest("aria-owns-from-display-none.html");
    }

    @Test
    @SmallTest
    public void test_ariaOwns() {
        performAriaTest("aria-owns.html");
    }

    @Test
    @SmallTest
    public void test_ariaOwnsIgnored() {
        performAriaTest("aria-owns-ignored.html");
    }

    @Test
    @SmallTest
    public void test_ariaOwnsList() {
        performAriaTest("aria-owns-list.html");
    }

    @Test
    @SmallTest
    public void test_ariaPresentation() {
        performAriaTest("aria-presentation.html");
    }

    @Test
    @SmallTest
    public void test_ariaPresentationInList() {
        performAriaTest("aria-presentation-in-list.html");
    }

    @Test
    @SmallTest
    public void test_ariaPressed() {
        performAriaTest("aria-pressed.html");
    }

    @Test
    @SmallTest
    public void test_ariaProgressbar() {
        performAriaTest("aria-progressbar.html");
    }

    @Test
    @SmallTest
    public void test_ariaRadiogroup() {
        performAriaTest("aria-radiogroup.html");
    }

    @Test
    @SmallTest
    public void test_ariaRadio() {
        performAriaTest("aria-radio.html");
    }

    @Test
    @SmallTest
    public void test_ariaReadonly() {
        performAriaTest("aria-readonly.html");
    }

    @Test
    @SmallTest
    public void test_ariaRegion() {
        performAriaTest("aria-region.html");
    }

    @Test
    @SmallTest
    public void test_ariaRelevant() {
        performAriaTest("aria-relevant.html");
    }

    @Test
    @SmallTest
    public void test_ariaRequired() {
        performAriaTest("aria-required.html");
    }

    @Test
    @SmallTest
    public void test_ariaRoledescription() {
        performAriaTest("aria-roledescription.html");
    }

    @Test
    @SmallTest
    public void test_ariaRowAttr() {
        performAriaTest("aria-row-attr.html");
    }

    @Test
    @SmallTest
    public void test_ariaRowgroup() {
        performAriaTest("aria-rowgroup.html");
    }

    @Test
    @SmallTest
    public void test_ariaRowheader() {
        performAriaTest("aria-rowheader.html");
    }

    @Test
    @SmallTest
    public void test_ariaRow() {
        performAriaTest("aria-row.html");
    }

    @Test
    @SmallTest
    public void test_ariaScrollbar() {
        performAriaTest("aria-scrollbar.html");
    }

    @Test
    @SmallTest
    public void test_ariaSearch() {
        performAriaTest("aria-search.html");
    }

    @Test
    @SmallTest
    public void test_ariaSelected() {
        performAriaTest("aria-selected.html");
    }

    @Test
    @SmallTest
    public void test_ariaSeparator() {
        performAriaTest("aria-separator.html");
    }

    @Test
    @SmallTest
    public void test_ariaSetsize() {
        performAriaTest("aria-setsize.html");
    }

    @Test
    @SmallTest
    public void test_ariaSlider() {
        performAriaTest("aria-slider.html");
    }

    @Test
    @SmallTest
    public void test_ariaSortAriaGrid() {
        performAriaTest("aria-sort-aria-grid.html");
    }

    @Test
    @SmallTest
    public void test_ariaSortHtmlTable() {
        performAriaTest("aria-sort-html-table.html");
    }

    @Test
    @SmallTest
    public void test_ariaStatus() {
        performAriaTest("aria-status.html");
    }

    @Test
    @SmallTest
    public void test_ariaStrong() {
        performAriaTest("aria-strong.html");
    }

    @Test
    @SmallTest
    public void test_ariaSubscript() {
        performAriaTest("aria-subscript.html");
    }

    @Test
    @SmallTest
    public void test_ariaSuperscript() {
        performAriaTest("aria-superscript.html");
    }

    @Test
    @SmallTest
    public void test_ariaSwitch() {
        performAriaTest("aria-switch.html");
    }

    @Test
    @SmallTest
    public void test_ariaTab() {
        performAriaTest("aria-tab.html");
    }

    @Test
    @SmallTest
    public void test_ariaTable() {
        performAriaTest("aria-table.html");
    }

    @Test
    @SmallTest
    public void test_ariaTablistAriaLevel() {
        performAriaTest("aria-tablist-aria-level.html");
    }

    @Test
    @SmallTest
    public void test_ariaTablist() {
        performAriaTest("aria-tablist.html");
    }

    @Test
    @SmallTest
    public void test_ariaTabNestedInLists() {
        performAriaTest("aria-tab-nested-in-lists.html");
    }

    @Test
    @SmallTest
    public void test_ariaTabpanel() {
        performAriaTest("aria-tabpanel.html");
    }

    @Test
    @SmallTest
    public void test_ariaTerm() {
        performAriaTest("aria-term.html");
    }

    @Test
    @SmallTest
    public void test_ariaTextbox() {
        performAriaTest("aria-textbox.html");
    }

    @Test
    @SmallTest
    public void test_ariaTextboxWithAriaTextboxChild() {
        performAriaTest("aria-textbox-with-aria-textbox-child.html");
    }

    @Test
    @SmallTest
    public void test_ariaTextboxWithNonTextChildren() {
        performAriaTest("aria-textbox-with-non-text-children.html");
    }

    @Test
    @SmallTest
    public void test_ariaTime() {
        performAriaTest("aria-time.html");
    }

    @Test
    @SmallTest
    public void test_ariaTimer() {
        performAriaTest("aria-timer.html");
    }

    @Test
    @SmallTest
    public void test_ariaTogglebutton() {
        performAriaTest("aria-togglebutton.html");
    }

    @Test
    @SmallTest
    public void test_ariaToolbar() {
        performAriaTest("aria-toolbar.html");
    }

    @Test
    @SmallTest
    public void test_ariaTooltip() {
        performAriaTest("aria-tooltip.html");
    }

    @Test
    @SmallTest
    public void test_ariaTreeDiscontinuous() {
        performAriaTest("aria-tree-discontinuous.html");
    }

    @Test
    @SmallTest
    public void test_ariaTreegrid() {
        performAriaTest("aria-treegrid.html");
    }

    @Test
    @SmallTest
    public void test_ariaTree() {
        performAriaTest("aria-tree.html");
    }

    @Test
    @SmallTest
    public void test_ariaTreeitemNestedInLists() {
        performAriaTest("aria-treeitem-nested-in-lists.html");
    }

    @Test
    @SmallTest
    public void test_ariaValuenow() {
        performAriaTest("aria-valuenow.html");
    }

    @Test
    @SmallTest
    public void test_ariaValuetext() {
        performAriaTest("aria-valuetext.html");
    }

    @Test
    @SmallTest
    public void test_ariaVirtualcontent() {
        performAriaTest("aria-virtualcontent.html");
    }

    @Test
    @SmallTest
    public void test_dpubRoles() {
        performAriaTest("dpub-roles.html");
    }

    @Test
    @SmallTest
    public void test_graphicsRoles() {
        performAriaTest("graphics-roles.html");
    }

    @Test
    @SmallTest
    public void test_hiddenDescribedBy() {
        performAriaTest("hidden-described-by.html");
    }

    @Test
    @SmallTest
    public void test_hidden() {
        performAriaTest("hidden.html");
    }

    @Test
    @SmallTest
    public void test_hiddenLabelledBy() {
        performAriaTest("hidden-labelled-by.html");
    }

    @Test
    @SmallTest
    public void test_inputTextAriaPlaceholder() {
        performAriaTest("input-text-aria-placeholder.html");
    }

    @Test
    @SmallTest
    public void test_presentational() {
        performAriaTest("presentational.html");
    }

    @Test
    @SmallTest
    public void test_toggleButtonExpandCollapse() {
        performAriaTest("toggle-button-expand-collapse.html");
    }

    // ------------------ CSS TESTS ------------------ //

    @Test
    @SmallTest
    public void test_altText() {
        performCssTest("alt-text.html");
    }

    @Test
    @SmallTest
    public void test_backgroundColorTransparent() {
        performCssTest("background-color-transparent.html");
    }

    @Test
    @SmallTest
    public void test_beforeAfterCode() {
        performCssTest("before-after-code.html");
    }

    @Test
    @SmallTest
    public void test_color() {
        performCssTest("color.html");
    }

    @Test
    @SmallTest
    public void test_domElementCssAlternativeText() {
        performCssTest("dom-element-css-alternative-text.html");
    }

    @Test
    @SmallTest
    public void test_fontSize() {
        performCssTest("font-size.html");
    }

    @Test
    @SmallTest
    public void test_fontStyle() {
        performCssTest("font-style.html");
    }

    @Test
    @SmallTest
    public void test_language() {
        performCssTest("language.html");
    }

    @Test
    @SmallTest
    public void test_listMarkerStylesCustom() {
        performCssTest("list-marker-styles-custom.html");
    }

    @Test
    @SmallTest
    public void test_pseudoElementAlternativeText() {
        performCssTest("pseudo-element-alternative-text.html");
    }

    @Test
    @SmallTest
    public void test_pseudoElementPositioned() {
        performCssTest("pseudo-element-positioned.html");
    }

    @Test
    @SmallTest
    public void test_pseudoElements() {
        performCssTest("pseudo-elements.html");
    }

    // ------------------ HTML TESTS ------------------ //

    @Test
    @SmallTest
    public void test_aNameCalc() {
        performHtmlTest("a-name-calc.html");
    }

    @Test
    @SmallTest
    public void test_aName() {
        performHtmlTest("a-name.html");
    }

    @Test
    @SmallTest
    public void test_aNestedStructure() {
        performHtmlTest("a-nested-structure.html");
    }

    @Test
    @SmallTest
    public void test_aNoText() {
        performHtmlTest("a-no-text.html");
    }

    @Test
    @SmallTest
    public void test_aWithImg() {
        performHtmlTest("a-with-img.html");
    }

    @Test
    @SmallTest
    public void test_a() {
        performHtmlTest("a.html");
    }

    @Test
    @SmallTest
    public void test_abbr() {
        performHtmlTest("abbr.html");
    }

    @Test
    @SmallTest
    public void test_accordion() {
        performHtmlTest("accordion.html");
    }

    @Test
    @SmallTest
    public void test_actionVerbs() {
        performHtmlTest("action-verbs.html");
    }

    @Test
    @SmallTest
    public void test_actions() {
        performHtmlTest("actions.html");
    }

    @Test
    @SmallTest
    public void test_address() {
        performHtmlTest("address.html");
    }

    @Test
    @SmallTest
    public void test_area() {
        performHtmlTest("area.html");
    }

    @Test
    @SmallTest
    public void test_article() {
        performHtmlTest("article.html");
    }

    @Test
    @SmallTest
    public void test_aside() {
        performHtmlTest("aside.html");
    }

    @Test
    @SmallTest
    public void test_aside_inside_other_section() {
        performHtmlTest("aside-inside-other-section.html");
    }

    @Test
    @SmallTest
    public void test_aside_inside_section_role_generic() {
        performHtmlTest("aside-inside-section-role-generic.html");
    }

    @Test
    @SmallTest
    public void test_b() {
        performHtmlTest("b.html");
    }

    @Test
    @SmallTest
    public void test_bdo() {
        performHtmlTest("bdo.html");
    }

    @Test
    @SmallTest
    public void test_blockquoteLevels() {
        performHtmlTest("blockquote-levels.html");
    }

    @Test
    @SmallTest
    public void test_blockquote() {
        performHtmlTest("blockquote.html");
    }

    @Test
    @SmallTest
    public void test_body() {
        performHtmlTest("body.html");
    }

    @Test
    @SmallTest
    public void test_br() {
        performHtmlTest("br.html");
    }

    @Test
    @SmallTest
    public void test_buttonNameCalc() {
        performHtmlTest("button-name-calc.html");
    }

    @Test
    @SmallTest
    public void test_buttonWithListboxPopup() {
        performHtmlTest("button-with-listbox-popup.html");
    }

    @Test
    @SmallTest
    public void test_button() {
        performHtmlTest("button.html");
    }

    @Test
    @SmallTest
    public void test_canvasFallback() {
        performHtmlTest("canvas-fallback.html");
    }

    @Test
    @SmallTest
    public void test_canvas() {
        performHtmlTest("canvas.html");
    }

    @Test
    @SmallTest
    public void test_caption() {
        performHtmlTest("caption.html");
    }

    @Test
    @SmallTest
    public void test_characterLocations() {
        performHtmlTest("character-locations.html");
    }

    @Test
    @SmallTest
    public void test_checkboxNameCalc() {
        performHtmlTest("checkbox-name-calc.html");
    }

    @Test
    @SmallTest
    public void test_cite() {
        performHtmlTest("cite.html");
    }

    @Test
    @SmallTest
    public void test_clickableAncestor() {
        performHtmlTest("clickable-ancestor.html");
    }

    @Test
    @SmallTest
    public void test_clickableScore() {
        performHtmlTest("clickable-score.html");
    }

    @Test
    @SmallTest
    public void test_code() {
        performHtmlTest("code.html");
    }

    @Test
    @SmallTest
    public void test_col() {
        performHtmlTest("col.html");
    }

    @Test
    @SmallTest
    public void test_colgroup() {
        performHtmlTest("colgroup.html");
    }

    @Test
    @SmallTest
    public void test_comboboxOptgroup() {
        performHtmlTest("combobox-optgroup.html");
    }

    @Test
    @SmallTest
    public void test_contenteditableDescendants() {
        performHtmlTest("contenteditable-descendants.html");
    }

    @Test
    @SmallTest
    public void test_contenteditableWithEmbeddedContenteditables() {
        performHtmlTest("contenteditable-with-embedded-contenteditables.html");
    }

    @Test
    @SmallTest
    public void test_contenteditableWithNoDescendants() {
        performHtmlTest("contenteditable-with-no-descendants.html");
    }

    @Test
    @SmallTest
    public void test_continuations() {
        performHtmlTest("continuations.html");
    }

    @Test
    @SmallTest
    public void test_customSelect() {
        performHtmlTest("custom-select.html");
    }

    @Test
    @SmallTest
    public void test_customSelectOpen() {
        performHtmlTest("custom-select-open.html");
    }

    @Test
    @SmallTest
    public void test_customSelectSimple() {
        performHtmlTest("custom-select-simple.html");
    }

    @Test
    @SmallTest
    public void test_customSelectSimpleOpen() {
        performHtmlTest("custom-select-simple-open.html");
    }

    @Test
    @SmallTest
    public void test_dd() {
        performHtmlTest("dd.html");
    }

    @Test
    @SmallTest
    public void test_del() {
        performHtmlTest("del.html");
    }

    @Test
    @SmallTest
    public void test_details() {
        performHtmlTest("details.html");
    }

    @Test
    @SmallTest
    public void test_details_name() {
        performHtmlTest("details-name.html");
    }

    @Test
    @SmallTest
    public void test_dfn() {
        performHtmlTest("dfn.html");
    }

    @Test
    @SmallTest
    public void test_dialog() {
        performHtmlTest("dialog.html");
    }

    @Test
    @SmallTest
    public void test_disabled() {
        performHtmlTest("disabled.html");
    }

    @Test
    @SmallTest
    public void test_div() {
        performHtmlTest("div.html");
    }

    @Test
    @SmallTest
    public void test_dl() {
        performHtmlTest("dl.html");
    }

    @Test
    @SmallTest
    public void test_dt() {
        performHtmlTest("dt.html");
    }

    @Test
    @SmallTest
    public void test_elementClassIdAttr() {
        performHtmlTest("element-class-id-attr.html");
    }

    @Test
    @SmallTest
    public void test_em() {
        performHtmlTest("em.html");
    }

    @Test
    @SmallTest
    public void test_embed() {
        performHtmlTest("embed.html");
    }

    @Test
    @SmallTest
    public void test_fieldset() {
        performHtmlTest("fieldset.html");
    }

    @Test
    @SmallTest
    public void test_figcaption() {
        performHtmlTest("figcaption.html");
    }

    @Test
    @SmallTest
    public void test_figure() {
        performHtmlTest("figure.html");
    }

    @DisableIf.Build(sdk_is_less_than = VERSION_CODES.O, message = "https://crbug.com/1376954")
    @Test
    @SmallTest
    public void test_fixedWidthText() {
        performHtmlTest("fixed-width-text.html");
    }

    @Test
    @SmallTest
    public void test_footerInsideOtherSection() {
        performHtmlTest("footer-inside-other-section.html");
    }

    @Test
    @SmallTest
    public void test_footer() {
        performHtmlTest("footer.html");
    }

    @Test
    @SmallTest
    public void test_form() {
        performHtmlTest("form.html");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1286036")
    public void test_frameset() {
        performHtmlTest("frameset.html");
    }

    @Test
    @SmallTest
    public void test_headerInsideOtherSection() {
        performHtmlTest("header-inside-other-section.html");
    }

    @Test
    @SmallTest
    public void test_header() {
        performHtmlTest("header.html");
    }

    @Test
    @SmallTest
    public void test_headingWithTabindex() {
        performHtmlTest("heading-with-tabIndex.html");
    }

    @Test
    @SmallTest
    public void test_heading() {
        performHtmlTest("heading.html");
    }

    @Test
    @SmallTest
    public void test_hr() {
        performHtmlTest("hr.html");
    }

    @Test
    @SmallTest
    public void test_html() {
        performHtmlTest("html.html");
    }

    @Test
    @SmallTest
    public void test_htmlAttributesAndTagNames() {
        performHtmlTest("html-attributes-and-tag-names.html");
    }

    @Test
    @SmallTest
    public void test_i() {
        performHtmlTest("i.html");
    }

    @Test
    @SmallTest
    public void test_id() {
        performHtmlTest("id.html");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1181596")
    public void test_iframeAriaHidden() {
        performHtmlTest("iframe-aria-hidden.html");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1286036")
    public void test_iframeCoordinatesCrossProcess() {
        performHtmlTest("iframe-coordinates-cross-process.html");
    }

    @Test
    @SmallTest
    public void test_iframeCrossProcess() {
        performHtmlTest("iframe-cross-process.html");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1379850")
    public void test_iframeEmptyPositioned() {
        performHtmlTest("iframe-empty-positioned.html");
    }

    @Test
    @SmallTest
    public void test_iframeFocus() {
        performHtmlTest("iframe-focus.html");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1286036")
    public void test_iframePresentational() {
        performHtmlTest("iframe-presentational.html");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1286036")
    public void test_iframeTransform() {
        performHtmlTest("iframe-transform.html");
    }

    @Test
    @SmallTest
    public void test_iframeTraversal() {
        performHtmlTest("iframe-traversal.html");
    }

    @Test
    @SmallTest
    public void test_iframe() {
        performHtmlTest("iframe.html");
    }

    @Test
    @SmallTest
    public void test_imgEmptyAlt() {
        performHtmlTest("img-empty-alt.html");
    }

    @Test
    @SmallTest
    public void test_imgLinkEmptyAlt() {
        performHtmlTest("img-link-empty-alt.html");
    }

    @Test
    @SmallTest
    public void test_img() {
        performHtmlTest("img.html");
    }

    @Test
    @SmallTest
    public void test_inPageLinks() {
        performHtmlTest("in-page-links.html");
    }

    @Test
    @SmallTest
    public void test_inputButton() {
        performHtmlTest("input-button.html");
    }

    @Test
    @SmallTest
    public void test_inputCheckboxLabel() {
        performHtmlTest("input-checkbox-label.html");
    }

    @Test
    @SmallTest
    public void test_inputColorWithPopupOpen() {
        performHtmlTest("input-color-with-popup-open.html");
    }

    @Test
    @SmallTest
    public void test_inputColor() {
        performHtmlTest("input-color.html");
    }

    @Test
    @SmallTest
    public void test_inputDateWithPopupOpenMultipleForWin() {
        performHtmlTest("input-date-with-popup-open-multiple-for-win.html");
    }

    @Test
    @SmallTest
    public void test_inputDateWithPopupOpenMultiple() {
        performHtmlTest("input-date-with-popup-open-multiple.html");
    }

    @Test
    @SmallTest
    public void test_inputDateWithPopupOpen() {
        performHtmlTest("input-date-with-popup-open.html");
    }

    @Test
    @SmallTest
    public void test_inputDate() {
        performHtmlTest("input-date.html");
    }

    @Test
    @SmallTest
    public void test_inputDatetimeLocal() {
        performHtmlTest("input-datetime-local.html");
    }

    @Test
    @SmallTest
    public void test_inputDatetime() {
        performHtmlTest("input-datetime.html");
    }

    @Test
    @SmallTest
    public void test_inputEmail() {
        performHtmlTest("input-email.html");
    }

    @Test
    @SmallTest
    public void test_inputFile() {
        performHtmlTest("input-file.html");
    }

    @Test
    @SmallTest
    public void test_inputImage() {
        performHtmlTest("input-image.html");
    }

    @Test
    @SmallTest
    public void test_inputList() {
        performHtmlTest("input-list.html");
    }

    @Test
    @SmallTest
    public void test_inputMonth() {
        performHtmlTest("input-month.html");
    }

    @Test
    @SmallTest
    public void test_inputNumber() {
        performHtmlTest("input-number.html");
    }

    @Test
    @SmallTest
    public void test_inputPasswordObscured() {
        performHtmlTest("input-password-obscured.html");
    }

    @Test
    @SmallTest
    public void test_inputRadioCheckboxLabel() {
        performHtmlTest("input-radio-checkbox-label.html");
    }

    @Test
    @SmallTest
    public void test_inputRadioInMenu() {
        performHtmlTest("input-radio-in-menu.html");
    }

    @Test
    @SmallTest
    public void test_inputRadio() {
        performHtmlTest("input-radio.html");
    }

    @Test
    @SmallTest
    public void test_inputRange() {
        performHtmlTest("input-range.html");
    }

    @Test
    @SmallTest
    public void test_inputReset() {
        performHtmlTest("input-reset.html");
    }

    @Test
    @SmallTest
    public void test_inputSearch() {
        performHtmlTest("input-search.html");
    }

    @Test
    @SmallTest
    public void test_inputSubmit() {
        performHtmlTest("input-submit.html");
    }

    @Test
    @SmallTest
    public void test_inputSuggestionsSourceElement() {
        performHtmlTest("input-suggestions-source-element.html");
    }

    @Test
    @SmallTest
    public void test_inputTel() {
        performHtmlTest("input-tel.html");
    }

    @Test
    @SmallTest
    public void test_inputTextNameCalc() {
        performHtmlTest("input-text-name-calc.html");
    }

    @Test
    @SmallTest
    public void test_inputTextRange() {
        performHtmlTest("input-text-range.html");
    }

    @Test
    @SmallTest
    public void test_inputTextReadOnly() {
        performHtmlTest("input-text-read-only.html");
    }

    @Test
    @SmallTest
    public void test_inputTextValue() {
        performHtmlTest("input-text-value.html");
    }

    @Test
    @SmallTest
    public void test_inputText() {
        performHtmlTest("input-text.html");
    }

    @Test
    @SmallTest
    public void test_inputTimeWithPopupOpen() {
        performHtmlTest("input-time-with-popup-open.html");
    }

    @Test
    @SmallTest
    public void test_inputTime() {
        performHtmlTest("input-time.html");
    }

    @Test
    @SmallTest
    public void test_inputTypesWithPlaceholder() {
        performHtmlTest("input-types-with-placeholder.html");
    }

    @Test
    @SmallTest
    public void test_inputTypesWithValueAndPlaceholder() {
        performHtmlTest("input-types-with-value-and-placeholder.html");
    }

    @Test
    @SmallTest
    public void test_inputTypesWithValue() {
        performHtmlTest("input-types-with-value.html");
    }

    @Test
    @SmallTest
    public void test_inputTypes() {
        performHtmlTest("input-types.html");
    }

    @Test
    @SmallTest
    public void test_inputUrl() {
        performHtmlTest("input-url.html");
    }

    @Test
    @SmallTest
    public void test_inputWeek() {
        performHtmlTest("input-week.html");
    }

    @Test
    @SmallTest
    public void test_ins() {
        performHtmlTest("ins.html");
    }

    @Test
    @SmallTest
    public void test_interactiveControlsWithLabels() {
        performHtmlTest("interactive-controls-with-labels.html");
    }

    @Test
    @SmallTest
    public void test_isInteresting() {
        performHtmlTest("isInteresting.html");
    }

    @Test
    @SmallTest
    public void test_label() {
        performHtmlTest("label.html");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1258230")
    public void test_landmark() {
        performHtmlTest("landmark.html");
    }

    @Test
    @SmallTest
    public void test_legend() {
        performHtmlTest("legend.html");
    }

    @Test
    @SmallTest
    public void test_li() {
        performHtmlTest("li.html");
    }

    @Test
    @SmallTest
    public void test_linkInsideHeading() {
        performHtmlTest("link-inside-heading.html");
    }

    @Test
    @SmallTest
    public void test_listItemAriaSetsizeUnknownFlattened() {
        performHtmlTest("list-item-aria-setsize-unknown-flattened.html");
    }

    @Test
    @SmallTest
    public void test_listItemAriaSetsizeUnknown() {
        performHtmlTest("list-item-aria-setsize-unknown.html");
    }

    @Test
    @SmallTest
    public void test_listMarkers() {
        performHtmlTest("list-markers.html");
    }

    @Test
    @SmallTest
    public void test_list() {
        performHtmlTest("list.html");
    }

    @Test
    @SmallTest
    public void test_main() {
        performHtmlTest("main.html");
    }

    @Test
    @SmallTest
    public void test_mapAnyContents() {
        performHtmlTest("map-any-contents.html");
    }

    @Test
    @SmallTest
    public void test_mark() {
        performHtmlTest("mark.html");
    }

    @Test
    @SmallTest
    public void test_math() {
        performHtmlTest("math.html");
    }

    @Test
    @SmallTest
    public void test_menu() {
        performHtmlTest("menu.html");
    }

    @Test
    @SmallTest
    public void test_meter() {
        performHtmlTest("meter.html");
    }

    @Test
    @SmallTest
    public void test_navigation() {
        performHtmlTest("navigation.html");
    }

    @Test
    @SmallTest
    @DisableIf.Build(supported_abis_includes = "x86_64", message = "https://crbug.com/349962563")
    public void test_nestedlist() {
        performHtmlTest("nestedlist.html");
    }

    @Test
    @SmallTest
    public void test_object() {
        performHtmlTest("object.html");
    }

    @Test
    @SmallTest
    public void test_offscreenIframeContent() {
        performHtmlTest("offscreen-iframe-content.html");
    }

    @Test
    @SmallTest
    public void test_offscreenIframe() {
        performHtmlTest("offscreen-iframe.html");
    }

    @Test
    @SmallTest
    public void test_offscreenSelect() {
        performHtmlTest("offscreen-select.html");
    }

    @Test
    @SmallTest
    public void test_ol() {
        performHtmlTest("ol.html");
    }

    @Test
    @SmallTest
    public void test_optgroup() {
        performHtmlTest("optgroup.html");
    }

    @Test
    @SmallTest
    public void test_optgroupMenulist() {
        performHtmlTest("optgroup-menulist.html");
    }

    @Test
    @SmallTest
    public void test_optgroupCustomMenulist() {
        performHtmlTest("optgroup-custom-menulist.html");
    }

    @Test
    @SmallTest
    public void test_output() {
        performHtmlTest("output.html");
    }

    @Test
    @SmallTest
    public void test_overflowActions() {
        performHtmlTest("overflow-actions.html");
    }

    @Test
    @SmallTest
    public void test_p() {
        performHtmlTest("p.html");
    }

    @Test
    @SmallTest
    public void test_param() {
        performHtmlTest("param.html");
    }

    @Test
    @SmallTest
    public void test_picture() {
        performHtmlTest("picture.html");
    }

    @Test
    @SmallTest
    public void test_pre() {
        performHtmlTest("pre.html");
    }

    @Test
    @SmallTest
    public void test_progress() {
        performHtmlTest("progress.html");
    }

    @Test
    @SmallTest
    public void test_q() {
        performHtmlTest("q.html");
    }

    @Test
    @SmallTest
    public void test_replacedNodeAcrossTrees() {
        performHtmlTest("replaced-node-across-trees.html");
    }

    @Test
    @SmallTest
    public void test_ruby() {
        performHtmlTest("ruby.html");
    }

    @Test
    @SmallTest
    public void test_s() {
        performHtmlTest("s.html");
    }

    @Test
    @SmallTest
    public void test_samp() {
        performHtmlTest("samp.html");
    }

    @Test
    @SmallTest
    public void test_scrollableOverflow() {
        performHtmlTest("scrollable-overflow.html");
    }

    @Test
    @SmallTest
    public void test_scrollableTextarea() {
        performHtmlTest("scrollable-textarea.html");
    }

    @Test
    @SmallTest
    public void test_scrollable() {
        performHtmlTest("scrollable.html");
    }

    @Test
    @SmallTest
    public void test_search() {
        performHtmlTest("search.html");
    }

    @Test
    @SmallTest
    public void test_section() {
        performHtmlTest("section.html");
    }

    @Test
    @SmallTest
    public void test_select() {
        performHtmlTest("select.html");
    }

    @Test
    @SmallTest
    public void test_selectionContainer() {
        performHtmlTest("selection-container.html");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1280713")
    public void test_selectmenu() {
        performHtmlTest("selectmenu.html");
    }

    @Test
    @SmallTest
    public void test_simple_spans() {
        performHtmlTest("simple_spans.html");
    }

    @Test
    @SmallTest
    public void test_small() {
        performHtmlTest("small.html");
    }

    @Test
    @SmallTest
    public void test_span() {
        performHtmlTest("span.html");
    }

    @Test
    @SmallTest
    public void test_spansSeparatedBySpace() {
        performHtmlTest("spans-separated-by-space.html");
    }

    @Test
    @SmallTest
    public void test_staticList() {
        performHtmlTest("static-list.html");
    }

    @Test
    @SmallTest
    public void test_strong() {
        performHtmlTest("strong.html");
    }

    @Test
    @SmallTest
    public void test_sub() {
        performHtmlTest("sub.html");
    }

    @Test
    @SmallTest
    public void test_summary() {
        performHtmlTest("summary.html");
    }

    @Test
    @SmallTest
    public void test_sup() {
        performHtmlTest("sup.html");
    }

    @Test
    @SmallTest
    public void test_svgAsObjectSource() {
        performHtmlTest("svg-as-object-source.html");
    }

    @Test
    @SmallTest
    public void test_svgChildOfButton() {
        performHtmlTest("svg-child-of-button.html");
    }

    @Test
    @SmallTest
    public void test_svgChildOfSvg() {
        performHtmlTest("svg-child-of-svg.html");
    }

    @Test
    @SmallTest
    public void test_svgDescInGroup() {
        performHtmlTest("svg-desc-in-group.html");
    }

    @Test
    @SmallTest
    public void test_svgSymbolWithRole() {
        performHtmlTest("svg-symbol-with-role.html");
    }

    @Test
    @SmallTest
    public void test_svgTextAlternativeComputation() {
        performHtmlTest("svg-text-alternative-computation.html");
    }

    @Test
    @SmallTest
    public void test_svgWithClickableRect() {
        performHtmlTest("svg-with-clickable-rect.html");
    }

    @Test
    @SmallTest
    public void test_svgWithForeignObject() {
        performHtmlTest("svg-with-foreign-object.html");
    }

    @Test
    @SmallTest
    public void test_svgWithLinkToDocument() {
        performHtmlTest("svg-with-link-to-document.html");
    }

    @Test
    @SmallTest
    public void test_svgWithNonLinkAnchors() {
        performHtmlTest("svg-with-non-link-anchors.html");
    }

    @Test
    @SmallTest
    public void test_svg() {
        performHtmlTest("svg.html");
    }

    @Test
    @SmallTest
    public void test_tabindexExposeChildren() {
        performHtmlTest("tabindex-expose-children.html");
    }

    @Test
    @SmallTest
    public void test_tableFocusableSections() {
        performHtmlTest("table-focusable-sections.html");
    }

    @Test
    @SmallTest
    public void test_tableLayout() {
        performHtmlTest("table-layout.html");
    }

    @Test
    @SmallTest
    public void test_tablePresentation() {
        performHtmlTest("table-presentation.html");
    }

    @Test
    @SmallTest
    public void test_tableSimple2() {
        performHtmlTest("table-simple-2.html");
    }

    @Test
    @SmallTest
    public void test_tableSimple() {
        performHtmlTest("table-simple.html");
    }

    @Test
    @SmallTest
    public void test_tableSpans() {
        performHtmlTest("table-spans.html");
    }

    @Test
    @SmallTest
    public void test_tableThColheader() {
        performHtmlTest("table-th-colheader.html");
    }

    @Test
    @SmallTest
    public void test_tableThRowheader() {
        performHtmlTest("table-th-rowheader.html");
    }

    @Test
    @SmallTest
    public void test_tableTheadTbodyTfoot() {
        performHtmlTest("table-thead-tbody-tfoot.html");
    }

    @Test
    @SmallTest
    public void test_tabPanel() {
        performHtmlTest("tab-panel.html");
    }

    @Test
    @SmallTest
    public void test_textAlign() {
        performHtmlTest("text-align.html");
    }

    @Test
    @SmallTest
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1224422")
    public void test_textColorsAndStyles() {
        performHtmlTest("text-colors-and-styles.html");
    }

    @Test
    @SmallTest
    public void test_textIndent() {
        performHtmlTest("text-indent.html");
    }

    @Test
    @SmallTest
    public void test_textareaReadOnly() {
        performHtmlTest("textarea-read-only.html");
    }

    @Test
    @SmallTest
    public void test_textarea() {
        performHtmlTest("textarea.html");
    }

    @Test
    @SmallTest
    public void test_time() {
        performHtmlTest("time.html");
    }

    @Test
    @SmallTest
    public void test_ulContenteditable() {
        performHtmlTest("ul-contenteditable.html");
    }

    @Test
    @SmallTest
    public void test_ul() {
        performHtmlTest("ul.html");
    }

    @Test
    @SmallTest
    public void test_var() {
        performHtmlTest("var.html");
    }

    @Test
    @SmallTest
    public void test_wbr() {
        performHtmlTest("wbr.html");
    }
}
