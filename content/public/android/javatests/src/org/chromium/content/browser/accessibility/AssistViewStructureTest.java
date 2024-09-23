// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_BOTTOM;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_HEIGHT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_LEFT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_RIGHT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_TOP;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_WIDTH;

import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.ui.accessibility.AccessibilityFeatures;

import java.util.concurrent.TimeoutException;

/** Tests for the implementation of onProvideVirtualStructure in WebContentsAccessibility. */
@RunWith(BaseJUnit4ClassRunner.class)
@DisableFeatures(ContentFeatureList.ACCESSIBILITY_UNIFIED_SNAPSHOTS)
public class AssistViewStructureTest {

    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    /** Helper to call onProvideVirtualStructure and block until the results are received. */
    private TestViewStructure getViewStructureFromHtml(String htmlContent, String js)
            throws TimeoutException {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(htmlContent));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        if (js != null) {
            JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    mActivityTestRule.getWebContents(), js);
        }

        final WebContentsAccessibilityImpl wcax = mActivityTestRule.getWebContentsAccessibility();

        TestViewStructure testViewStructure = new TestViewStructure();

        ThreadUtils.runOnUiThreadBlocking(
                () -> wcax.onProvideVirtualStructure(testViewStructure, false));

        CriteriaHelper.pollUiThread(
                wcax::hasFinishedLatestAccessibilitySnapshotForTesting,
                "Timed out waiting for onProvideVirtualStructure");
        return testViewStructure;
    }

    /** Call getViewStructureFromHtml without the js parameter. */
    private TestViewStructure getViewStructureFromHtml(String htmlContent) throws TimeoutException {
        return getViewStructureFromHtml(htmlContent, null);
    }

    private String getSelectionScript(String node1, int start, String node2, int end) {
        return "var element1 = document.getElementById('"
                + node1
                + "');"
                + "var node1 = element1.childNodes.item(0);"
                + "var range=document.createRange();"
                + "range.setStart(node1,"
                + start
                + ");"
                + "var element2 = document.getElementById('"
                + node2
                + "');"
                + "var node2 = element2.childNodes.item(0);"
                + "range.setEnd(node2,"
                + end
                + ");"
                + "var selection=window.getSelection();"
                + "selection.removeAllRanges();"
                + "selection.addRange(range);";
    }

    private String addManyNodesScript() {
        return "var body = document.getElementById('container');\n"
                + "for (i = 0; i < 600; i++) {\n"
                + "  var nextContainer = document.createElement('div');\n"
                + "  for (j = 0; j < 10; j++) {\n"
                + "    var paragraph = document.createElement('p');\n"
                + "    paragraph.innerHTML = \"Example Text\";\n"
                + "    nextContainer.appendChild(paragraph);\n"
                + "  }\n"
                + "  body.appendChild(nextContainer);\n"
                + "}\n";
    }

    /** Test that the snapshot contains the url. */
    @Test
    @MediumTest
    public void testUrl() throws Throwable {
        TestViewStructure root = getViewStructureFromHtml("<p>Hello World</p>");
        Assert.assertEquals(1, root.getChildCount());
        TestViewStructure webview = root.getChild(0);
        Assert.assertNotNull(webview);

        Bundle extras = webview.getExtras();
        String url = extras.getCharSequence("url").toString();
        Assert.assertTrue(url.contains("data:"));
        Assert.assertFalse(url.contains("http:"));
        Assert.assertTrue(url.contains("text/html"));
        Assert.assertTrue(url.contains("Hello"));
        Assert.assertTrue(url.contains("World"));
    }

    /** Test selection is propagated when it spans one character. */
    @Test
    @MediumTest
    public void testOneCharacterSelection() throws Throwable {
        final String data = "<html><body><b id='node' role='none'>foo</b></body></html>";
        final String js = getSelectionScript("node", 0, "node", 1);
        TestViewStructure root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child = root.getChild(0);
        TestViewStructure grandchild = child.getChild(0);
        Assert.assertEquals("foo", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(1, grandchild.getTextSelectionEnd());
    }

    /** Test selection is propagated when it spans one node. */
    @Test
    @MediumTest
    public void testOneNodeSelection() throws Throwable {
        final String data = "<html><body><b id='node' role='none'>foo</b></body></html>";
        final String js = getSelectionScript("node", 0, "node", 3);
        TestViewStructure root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child = root.getChild(0);
        TestViewStructure grandchild = child.getChild(0);
        Assert.assertEquals("foo", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(3, grandchild.getTextSelectionEnd());
    }

    /** Test selection is propagated when it spans to the beginning of the next node. */
    @Test
    @MediumTest
    public void testSubsequentNodeSelection() throws Throwable {
        final String data =
                "<html><body><b id='node1' role='none'>foo</b>"
                        + "<b id='node2' role='none'>bar</b></body></html>";
        final String js = getSelectionScript("node1", 1, "node2", 1);
        TestViewStructure root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child = root.getChild(0);
        TestViewStructure grandchild = child.getChild(0);
        Assert.assertEquals("foo", grandchild.getText());
        Assert.assertEquals(1, grandchild.getTextSelectionStart());
        Assert.assertEquals(3, grandchild.getTextSelectionEnd());
        grandchild = child.getChild(1);
        Assert.assertEquals("bar", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(1, grandchild.getTextSelectionEnd());
    }

    /** Test selection is propagated across multiple nodes. */
    @Test
    @MediumTest
    public void testMultiNodeSelection() throws Throwable {
        final String data =
                "<html><body><b id='node1' role='none'>foo</b><b>middle</b>"
                        + "<b id='node2' role='none'>bar</b></body></html>";
        final String js = getSelectionScript("node1", 1, "node2", 1);
        TestViewStructure root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child = root.getChild(0);
        TestViewStructure grandchild = child.getChild(0);
        Assert.assertEquals("foo", grandchild.getText());
        Assert.assertEquals(1, grandchild.getTextSelectionStart());
        Assert.assertEquals(3, grandchild.getTextSelectionEnd());
        grandchild = child.getChild(1);
        Assert.assertEquals("middle", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(6, grandchild.getTextSelectionEnd());
        grandchild = child.getChild(2);
        Assert.assertEquals("bar", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(1, grandchild.getTextSelectionEnd());
    }

    /** Test selection is propagated from an HTML input element. */
    @Test
    @MediumTest
    public void testRequestAccessibilitySnapshotInputSelection() throws Throwable {
        final String data = "<html><body><input id='input' value='Hello, world'></body></html>";
        final String js =
                "var input = document.getElementById('input');"
                        + "input.select();"
                        + "input.selectionStart = 0;"
                        + "input.selectionEnd = 5;";

        TestViewStructure root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child = root.getChild(0);
        TestViewStructure grandchild = child.getChild(0);
        Assert.assertEquals("Hello, world", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(5, grandchild.getTextSelectionEnd());
    }

    /** Test that the snapshot always contains Bundle extras for unclipped bounds. */
    @Test
    @MediumTest
    public void testUnclippedBounds() throws Throwable {
        TestViewStructure root = getViewStructureFromHtml("<p>Hello world</p>").getChild(0);
        TestViewStructure paragraph = root.getChild(0);

        Bundle extras = paragraph.getExtras();
        int unclippedTop = extras.getInt(EXTRAS_KEY_UNCLIPPED_TOP, -1);
        int unclippedBottom = extras.getInt(EXTRAS_KEY_UNCLIPPED_BOTTOM, -1);
        int unclippedLeft = extras.getInt(EXTRAS_KEY_UNCLIPPED_LEFT, -1);
        int unclippedRight = extras.getInt(EXTRAS_KEY_UNCLIPPED_RIGHT, -1);
        int unclippedWidth = extras.getInt(EXTRAS_KEY_UNCLIPPED_WIDTH, -1);
        int unclippedHeight = extras.getInt(EXTRAS_KEY_UNCLIPPED_HEIGHT, -1);

        Assert.assertTrue(unclippedTop > 0);
        Assert.assertTrue(unclippedBottom > 0);
        Assert.assertTrue(unclippedLeft > 0);
        Assert.assertTrue(unclippedRight > 0);
        Assert.assertTrue(unclippedWidth > 0);
        Assert.assertTrue(unclippedHeight > 0);
    }

    /** Test that pages with larger than the max node count result in a partial tree. */
    @Test
    @MediumTest
    @DisableFeatures(AccessibilityFeatures.ACCESSIBILITY_SNAPSHOT_STRESS_TESTS)
    public void testMaxNodesLimit() throws Throwable {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                "Accessibility.AXTreeSnapshotter.Snapshot.EndToEndRuntime", 0)
                        .build();

        // There is a max of 5000 nodes, add many nodes with some children. If the tree is flat
        // then all nodes will end up serialized because the serializer will finish the current
        // node and its children. The number of nodes returned may be more or less than 5000.
        TestViewStructure root =
                getViewStructureFromHtml("<div id='container'></div>", addManyNodesScript())
                        .getChild(0);

        // Recursively count child nodes. Allow for approximately 5000 nodes.
        Assert.assertTrue(
                String.format(
                        "Too many nodes serialized, found %s", root.getTotalDescendantCount()),
                5100 > root.getTotalDescendantCount());

        histogramWatcher.assertExpected();
    }

    /** Test that pages with more than the max node count return a full tree during stress tests. */
    @Test
    @MediumTest
    @EnableFeatures(AccessibilityFeatures.ACCESSIBILITY_SNAPSHOT_STRESS_TESTS)
    @DisabledTest(message = "crbug.com/362208929")
    public void testMaxNodesLimit_ignoredDuringStressTests() throws Throwable {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                "Accessibility.AXTreeSnapshotter.Snapshot.EndToEndRuntime", 1)
                        .build();

        TestViewStructure root =
                getViewStructureFromHtml("<div id='container'></div>", addManyNodesScript())
                        .getChild(0);

        Assert.assertTrue(
                String.format("Too few nodes serialized, found %s", root.getTotalDescendantCount()),
                12000 < root.getTotalDescendantCount());

        histogramWatcher.assertExpected();
    }
}
