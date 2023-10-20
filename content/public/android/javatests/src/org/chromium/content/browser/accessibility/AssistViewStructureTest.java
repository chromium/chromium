// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_HEIGHT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_LEFT;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_TOP;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_WIDTH;

import android.app.assist.AssistStructure.ViewNode;
import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/** Tests for the implementation of onProvideVirtualStructure in WebContentsAccessibility. */
@RunWith(BaseJUnit4ClassRunner.class)
// TODO(mschillaci): Migrate all these tests to the WebContentsAccessibilityTreeTest suite.
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

        TestThreadUtils.runOnUiThreadBlocking(
                () -> wcax.onProvideVirtualStructure(testViewStructure, false));

        CriteriaHelper.pollUiThread(
                testViewStructure::isDone, "Timed out waiting for onProvideVirtualStructure");
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

    /** Test simple paragraph. */
    @Test
    @MediumTest
    public void testSimpleParagraph() throws Throwable {
        TestViewStructure testViewStructure = getViewStructureFromHtml("<p>Hello World</p>");
        Assert.assertEquals(
                "WebView textSize:16.00 style:0 bundle:[display=\"\", htmlTag=\"#document\"]\n"
                    + "++View textSize:16.00 style:0 bundle:[display=\"block\", htmlTag=\"p\"]\n"
                    + "++++TextView text:\"Hello World\" textSize:16.00 style:0"
                    + " bundle:[display=\"\", htmlTag=\"\"]",
                testViewStructure.toString());
    }

    /** Test static list. */
    @Test
    @MediumTest
    public void testStaticList() throws Throwable {
        TestViewStructure testViewStructure =
                getViewStructureFromHtml(
                        "<ol>"
                                + "  <li>Kirk</li>"
                                + "  <li>Picard</li>"
                                + "  <li>Janeway</li>"
                                + "</ol>");
        Assert.assertEquals(
                "WebView textSize:16.00 style:0 bundle:[display=\"\", htmlTag=\"#document\"]\n"
                    + "++ListView textSize:16.00 style:0 bundle:[display=\"block\","
                    + " htmlTag=\"ol\"]\n"
                    + "++++View textSize:16.00 style:0 bundle:[display=\"list-item\","
                    + " htmlTag=\"li\"]\n"
                    + "++++++View text:\"1. \" textSize:16.00 style:0"
                    + " bundle:[display=\"inline-block\", htmlTag=\"::marker\"]\n"
                    + "++++++TextView text:\"Kirk\" textSize:16.00 style:0 bundle:[display=\"\","
                    + " htmlTag=\"\"]\n"
                    + "++++View textSize:16.00 style:0 bundle:[display=\"list-item\","
                    + " htmlTag=\"li\"]\n"
                    + "++++++View text:\"2. \" textSize:16.00 style:0"
                    + " bundle:[display=\"inline-block\", htmlTag=\"::marker\"]\n"
                    + "++++++TextView text:\"Picard\" textSize:16.00 style:0 bundle:[display=\"\","
                    + " htmlTag=\"\"]\n"
                    + "++++View textSize:16.00 style:0 bundle:[display=\"list-item\","
                    + " htmlTag=\"li\"]\n"
                    + "++++++View text:\"3. \" textSize:16.00 style:0"
                    + " bundle:[display=\"inline-block\", htmlTag=\"::marker\"]\n"
                    + "++++++TextView text:\"Janeway\" textSize:16.00 style:0 bundle:[display=\"\","
                    + " htmlTag=\"\"]",
                testViewStructure.toString());
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

    /**
     * Test that accessible descriptions (like title, aria-label) augments visible
     * text that's in the document, but that visible text isn't redundantly repeated
     * otherwise.
     *
     * For example, a simple link like <a href="#">Hello</a> should not have the text
     * "Hello" on both the link and the inner content node, but if the link has an
     * aria-label like <a href="#" aria-label="Friday">Tomorrow</a> then the
     * link's text should be the aria-label and the inner text should still be present.
     */
    @Test
    @MediumTest
    public void testAccessibleLabelsAugmentInnerText() throws Throwable {
        TestViewStructure testViewStructure =
                getViewStructureFromHtml(
                        "<a href='#'>Link</a>"
                                + "<a href='#' aria-label='AriaLabel'>Link</a>"
                                + "<button>Button</button>"
                                + "<button aria-label='AriaLabel'>Button</button>");
        Assert.assertEquals(
                "WebView textSize:16.00 style:0 bundle:[display=\"\", htmlTag=\"#document\"]\n"
                    + "++View textSize:16.00 style:0 bundle:[display=\"block\", htmlTag=\"body\"]\n"
                    + "++++View textSize:16.00 style:4 fgColor:-16776978"
                    + " bundle:[display=\"inline\", href=\"#\", htmlTag=\"a\"]\n"
                    + "++++++TextView text:\"Link\" textSize:16.00 style:4 fgColor:-16776978"
                    + " bundle:[display=\"\", htmlTag=\"\"]\n"
                    + "++++View text:\"AriaLabel\" textSize:16.00 style:4 fgColor:-16776978"
                    + " bundle:[aria-label=\"AriaLabel\", display=\"inline\", href=\"#\","
                    + " htmlTag=\"a\"]\n"
                    + "++++++TextView text:\"Link\" textSize:16.00 style:4 fgColor:-16776978"
                    + " bundle:[display=\"\", htmlTag=\"\"]\n"
                    + "++++Button textSize:13.33 style:0 bgColor:-1052689"
                    + " bundle:[display=\"inline-block\", htmlTag=\"button\"]\n"
                    + "++++++TextView text:\"Button\" textSize:13.33 style:0 bgColor:-1052689"
                    + " bundle:[display=\"\", htmlTag=\"\"]\n"
                    + "++++Button text:\"AriaLabel\" textSize:13.33 style:0 bgColor:-1052689"
                    + " bundle:[aria-label=\"AriaLabel\", display=\"inline-block\","
                    + " htmlTag=\"button\"]\n"
                    + "++++++TextView text:\"Button\" textSize:13.33 style:0 bgColor:-1052689"
                    + " bundle:[display=\"\", htmlTag=\"\"]",
                testViewStructure.toString());
    }

    /** Test that the snapshot contains HTML tag names. */
    @Test
    @MediumTest
    public void testHtmlTagNames() throws Throwable {
        TestViewStructure testViewStructure =
                getViewStructureFromHtml(
                        "<h1>Heading</h1>" + "  <p>Paragraph</p>" + "  <div><input></div>");
        Assert.assertEquals(
                "WebView textSize:16.00 style:0 bundle:[display=\"\", htmlTag=\"#document\"]\n"
                    + "++View textSize:32.00 style:1 bundle:[display=\"block\", htmlTag=\"h1\"]\n"
                    + "++++TextView text:\"Heading\" textSize:32.00 style:1 bundle:[display=\"\","
                    + " htmlTag=\"\"]\n"
                    + "++View textSize:16.00 style:0 bundle:[display=\"block\", htmlTag=\"p\"]\n"
                    + "++++TextView text:\"Paragraph\" textSize:16.00 style:0 bundle:[display=\"\","
                    + " htmlTag=\"\"]\n"
                    + "++View textSize:16.00 style:0 bundle:[display=\"block\", htmlTag=\"div\"]\n"
                    + "++++EditText textSize:13.33 style:0 bundle:[display=\"inline-block\","
                    + " htmlTag=\"input\"]\n"
                    + "++++++View textSize:13.33 style:0 bundle:[display=\"flow-root\","
                    + " htmlTag=\"div\"]",
                testViewStructure.toString());
    }

    /** Test that the snapshot contains HTML attributes. */
    @Test
    @MediumTest
    public void testHtmlAttributes() throws Throwable {
        TestViewStructure node =
                getViewStructureFromHtml("<button id='a' class='b' aria-label='c'>D</button>");

        while (node != null
                && (node.getClassName() == null
                        || !node.getClassName().equals("android.widget.Button"))) {
            node = node.getChild(0);
        }

        Bundle extras = node.getExtras();
        Assert.assertEquals("a", extras.getCharSequence("id").toString());
        Assert.assertEquals("b", extras.getCharSequence("class").toString());
        Assert.assertEquals("c", extras.getCharSequence("aria-label").toString());
        Assert.assertNull(extras.getCharSequence("disabled"));
        Assert.assertNull(extras.getCharSequence("onclick"));
    }

    /** Test that the snapshot contains HTML metadata. */
    @Test
    @MediumTest
    public void testHtmlMetadata() throws Throwable {
        TestViewStructure root =
                getViewStructureFromHtml(
                                "<head>"
                                        + "  <title>Hello World</title>"
                                        + "  <script>console.log(\"Skip me!\");</script>"
                                        + "  <meta charset=\"utf-8\">"
                                        + "  <link ref=\"canonical\" href=\"https://abc.com\">"
                                        + "  <script type=\"application/ld+json\">{}</script>"
                                        + "</head>"
                                        + "<body>Hello, world</body>")
                        .getChild(0);
        Bundle extras = root.getExtras();
        ArrayList<String> metadata = extras.getStringArrayList("metadata");
        Assert.assertNotNull(metadata);
        Assert.assertEquals(4, metadata.size());
        Assert.assertEquals("<title>Hello World</title>", metadata.get(0));
        Assert.assertEquals("<meta charset=\"utf-8\"></meta>", metadata.get(1));
        Assert.assertEquals(
                "<link ref=\"canonical\" href=\"https://abc.com\"></link>", metadata.get(2));
        Assert.assertEquals("<script type=\"application/ld+json\">{}</script>", metadata.get(3));
    }

    /** Verifies that AX tree is returned. */
    @Test
    @MediumTest
    public void testButton() throws Throwable {
        final String data = "<button>Click</button>";
        TestViewStructure root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child = root.getChild(0);
        Assert.assertEquals(1, child.getChildCount());
        Assert.assertEquals("", child.getText());
        TestViewStructure button = child.getChild(0);
        Assert.assertEquals(1, button.getChildCount());
        Assert.assertEquals("android.widget.Button", button.getClassName());
        TestViewStructure buttonText = button.getChild(0);
        Assert.assertEquals("Click", buttonText.getText());
    }

    /** Verifies colors are propagated correctly. */
    @Test
    @MediumTest
    public void testColors() throws Throwable {
        final String data = "<p style=\"color:#123456;background:#abcdef\">color</p>";
        TestViewStructure root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure para = root.getChild(0);
        Assert.assertEquals("ff123456", Integer.toHexString(para.getFgColor()));
        Assert.assertEquals("ffabcdef", Integer.toHexString(para.getBgColor()));
        TestViewStructure paraText = para.getChild(0);
        Assert.assertEquals("color", paraText.getText());
    }

    /** Verifies font sizes are propagated correctly. */
    @Test
    @MediumTest
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1224422")
    public void testFontSize() throws Throwable {
        final String data =
                "<html><head><style> "
                        + "    p { font-size:16px; transform: scale(2); }"
                        + "    </style></head><body><p>foo</p></body></html>";
        TestViewStructure root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure para = root.getChild(0);
        TestViewStructure paraText = para.getChild(0);
        Assert.assertEquals("foo", paraText.getText());

        // The font size should not be affected by page zoom or CSS transform.
        Assert.assertEquals(16, para.getTextSize(), 0.01);
    }

    /** Verifies text styles are propagated correctly. */
    @Test
    @MediumTest
    public void testTextStyles() throws Throwable {
        final String data =
                "<html><head><style> "
                        + "    body { font: italic bold 12px Courier; }"
                        + "    </style></head><body><p>foo</p></body></html>";
        TestViewStructure root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure para = root.getChild(0);
        int style = para.getStyle();
        Assert.assertTrue(0 != (style & ViewNode.TEXT_STYLE_BOLD));
        Assert.assertTrue(0 != (style & ViewNode.TEXT_STYLE_ITALIC));
        Assert.assertFalse(0 != (style & ViewNode.TEXT_STYLE_UNDERLINE));
        Assert.assertFalse(0 != (style & ViewNode.TEXT_STYLE_STRIKE_THRU));

        TestViewStructure paraText = para.getChild(0);
        Assert.assertEquals("foo", paraText.getText());
    }

    /** Verifies the strong style is propagated correctly. */
    @Test
    @MediumTest
    public void testStrongStyle() throws Throwable {
        final String data = "<html><body><p>foo</p><p><strong>bar</strong></p></body></html>";
        TestViewStructure root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(2, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child1 = root.getChild(0);
        Assert.assertEquals("foo", child1.getChild(0).getText());
        int child1style = child1.getStyle();
        Assert.assertFalse(0 != (child1style & ViewNode.TEXT_STYLE_BOLD));
        TestViewStructure child2 = root.getChild(1);
        TestViewStructure child2child = child2.getChild(0);
        Assert.assertEquals("bar", child2child.getText());
        Assert.assertEquals(child1.getTextSize(), child2child.getTextSize(), 0);
        int child2childstyle = child2child.getStyle();
        Assert.assertTrue(0 != (child2childstyle & ViewNode.TEXT_STYLE_BOLD));
    }

    /** Verifies the italic style is propagated correctly. */
    @Test
    @MediumTest
    public void testItalicStyle() throws Throwable {
        final String data = "<html><body><i>foo</i></body></html>";
        TestViewStructure root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child = root.getChild(0);
        TestViewStructure grandchild = child.getChild(0);
        int style = grandchild.getStyle();
        Assert.assertTrue(0 != (style & ViewNode.TEXT_STYLE_ITALIC));
    }

    /** Verifies the bold style is propagated correctly. */
    @Test
    @MediumTest
    public void testBoldStyle() throws Throwable {
        final String data = "<html><body><b>foo</b></body></html>";
        TestViewStructure root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child = root.getChild(0);
        TestViewStructure grandchild = child.getChild(0);
        int style = grandchild.getStyle();
        Assert.assertTrue(0 != (style & ViewNode.TEXT_STYLE_BOLD));
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

    /** Test that the value is propagated from an HTML password field. */
    @Test
    @MediumTest
    public void testRequestAccessibilitySnapshotPasswordField() throws Throwable {
        final String data =
                "<html><body><input id='input' type='password' value='foo'></body></html>";
        TestViewStructure root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructure child = root.getChild(0);
        TestViewStructure grandchild = child.getChild(0);
        Assert.assertEquals("•••", grandchild.getText());
    }

    /** Test that the snapshot contains Bundle extras for unclipped bounds. */
    @Test
    @MediumTest
    public void testUnclippedBounds() throws Throwable {
        TestViewStructure root = getViewStructureFromHtml("<p>Hello world</p>").getChild(0);
        TestViewStructure paragraph = root.getChild(0);

        Bundle extras = paragraph.getExtras();
        int unclippedLeft = extras.getInt(EXTRAS_KEY_UNCLIPPED_LEFT, -1);
        int unclippedTop = extras.getInt(EXTRAS_KEY_UNCLIPPED_TOP, -1);
        int unclippedWidth = extras.getInt(EXTRAS_KEY_UNCLIPPED_WIDTH, -1);
        int unclippedHeight = extras.getInt(EXTRAS_KEY_UNCLIPPED_HEIGHT, -1);

        Assert.assertTrue(unclippedLeft > 0);
        Assert.assertTrue(unclippedTop > 0);
        Assert.assertTrue(unclippedWidth > 0);
        Assert.assertTrue(unclippedHeight > 0);
    }
}
