// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.app.assist.AssistStructure.ViewNode;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the implementation of onProvideVirtualStructure in
 * WebContentsAccessibility.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AssistViewStructureTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    /**
     * Helper to call onProvideVirtualStructure and block until the results are received.
     */
    private TestViewStructureInterface getViewStructureFromHtml(String htmlContent, String js)
            throws TimeoutException {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(htmlContent));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        if (js != null) {
            JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    mActivityTestRule.getWebContents(), js);
        }

        final WebContentsAccessibilityImpl wcax = mActivityTestRule.getWebContentsAccessibility();

        TestViewStructureInterface testViewStructure =
                TestViewStructureFactory.createTestViewStructure();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> wcax.onProvideVirtualStructure((TestViewStructure) testViewStructure, false));

        CriteriaHelper.pollUiThread(()
                                            -> testViewStructure.isDone(),
                "Timed out waiting for onProvideVirtualStructure");
        return testViewStructure;
    }

    /**
     * Call getViewStructureFromHtml without the js parameter.
     */
    private TestViewStructureInterface getViewStructureFromHtml(String htmlContent)
            throws TimeoutException {
        return getViewStructureFromHtml(htmlContent, null);
    }

    private String getSelectionScript(String node1, int start, String node2, int end) {
        return "var element1 = document.getElementById('" + node1 + "');"
                + "var node1 = element1.childNodes.item(0);"
                + "var range=document.createRange();"
                + "range.setStart(node1," + start + ");"
                + "var element2 = document.getElementById('" + node2 + "');"
                + "var node2 = element2.childNodes.item(0);"
                + "range.setEnd(node2," + end + ");"
                + "var selection=window.getSelection();"
                + "selection.removeAllRanges();"
                + "selection.addRange(range);";
    }

    /**
     * Test simple paragraph.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testSimpleParagraph() throws Throwable {
        TestViewStructureInterface testViewStructure =
                getViewStructureFromHtml("<p>Hello World</p>");
        Assert.assertEquals(testViewStructure.toString(),
                "\n"
                        + "  android.webkit.WebView\n"
                        + "    android.view.View\n"
                        + "      android.widget.TextView text='Hello World'\n");
    }

    /**
     * Test static list.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testStaticList() throws Throwable {
        TestViewStructureInterface testViewStructure = getViewStructureFromHtml("<ol>"
                + "  <li>Kirk</li>"
                + "  <li>Picard</li>"
                + "  <li>Janeway</li>"
                + "</ol>");
        Assert.assertEquals(testViewStructure.toString(),
                "\n"
                        + "  android.webkit.WebView\n"
                        + "    android.widget.ListView\n"
                        + "      android.view.View\n"
                        + "        android.view.View text='1. '\n"
                        + "        android.widget.TextView text='Kirk'\n"
                        + "      android.view.View\n"
                        + "        android.view.View text='2. '\n"
                        + "        android.widget.TextView text='Picard'\n"
                        + "      android.view.View\n"
                        + "        android.view.View text='3. '\n"
                        + "        android.widget.TextView text='Janeway'\n");
    }

    /**
     * Test that the snapshot contains the url.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testUrl() throws Throwable {
        TestViewStructureInterface root = getViewStructureFromHtml("<p>Hello World</p>");
        Assert.assertEquals(1, root.getChildCount());
        TestViewStructureInterface webview = root.getChild(0);
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testAccessibleLabelsAugmentInnerText() throws Throwable {
        TestViewStructureInterface testViewStructure =
                getViewStructureFromHtml("<a href='#'>Link</a>"
                        + "<a href='#' aria-label='AriaLabel'>Link</a>"
                        + "<button>Button</button>"
                        + "<button aria-label='AriaLabel'>Button</button>");
        Assert.assertEquals(testViewStructure.toString(),
                "\n"
                        + "  android.webkit.WebView\n"
                        + "    android.view.View\n"
                        + "      android.view.View\n"
                        + "        android.widget.TextView text='Link'\n"
                        + "      android.view.View text='AriaLabel'\n"
                        + "        android.widget.TextView text='Link'\n"
                        + "      android.widget.Button\n"
                        + "        android.widget.TextView text='Button'\n"
                        + "      android.widget.Button text='AriaLabel'\n"
                        + "        android.widget.TextView text='Button'\n");
    }

    /**
     * Test that the snapshot contains HTML tag names.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testHtmlTagNames() throws Throwable {
        TestViewStructureInterface testViewStructure = getViewStructureFromHtml("<h1>Heading</h1>"
                + "  <p>Paragraph</p>"
                + "  <div><input></div>");
        testViewStructure.dumpHtmlTags();
        Assert.assertEquals(testViewStructure.toString(),
                "\n"
                        + "  android.webkit.WebView htmlTag='#document'\n"
                        + "    android.view.View htmlTag='h1'\n"
                        + "      android.widget.TextView text='Heading'\n"
                        + "    android.view.View htmlTag='p'\n"
                        + "      android.widget.TextView text='Paragraph'\n"
                        + "    android.view.View htmlTag='div'\n"
                        + "      android.widget.EditText htmlTag='input'\n"
                        + "        android.view.View htmlTag='div'\n");
    }

    /**
     * Test that the snapshot contains HTML attributes.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testHtmlAttributes() throws Throwable {
        TestViewStructureInterface node =
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

    /**
     * Test that the snapshot contains HTML metadata.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testHtmlMetadata() throws Throwable {
        TestViewStructureInterface root = getViewStructureFromHtml("<head>"
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

    /**
     * Verifies that AX tree is returned.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testButton() throws Throwable {
        final String data = "<button>Click</button>";
        TestViewStructureInterface root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child = root.getChild(0);
        Assert.assertEquals(1, child.getChildCount());
        Assert.assertEquals("", child.getText());
        TestViewStructureInterface button = child.getChild(0);
        Assert.assertEquals(1, button.getChildCount());
        Assert.assertEquals("android.widget.Button", button.getClassName());
        TestViewStructureInterface buttonText = button.getChild(0);
        Assert.assertEquals("Click", buttonText.getText());
    }

    /**
     * Verifies colors are propagated correctly.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testColors() throws Throwable {
        final String data = "<p style=\"color:#123456;background:#abcdef\">color</p>";
        TestViewStructureInterface root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface para = root.getChild(0);
        Assert.assertEquals("ff123456", Integer.toHexString(para.getFgColor()));
        Assert.assertEquals("ffabcdef", Integer.toHexString(para.getBgColor()));
        TestViewStructureInterface paraText = para.getChild(0);
        Assert.assertEquals("color", paraText.getText());
    }

    /**
     * Verifies font sizes are propagated correctly.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1224422")
    public void testFontSize() throws Throwable {
        final String data = "<html><head><style> "
                + "    p { font-size:16px; transform: scale(2); }"
                + "    </style></head><body><p>foo</p></body></html>";
        TestViewStructureInterface root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface para = root.getChild(0);
        TestViewStructureInterface paraText = para.getChild(0);
        Assert.assertEquals("foo", paraText.getText());

        // The font size should not be affected by page zoom or CSS transform.
        Assert.assertEquals(16, para.getTextSize(), 0.01);
    }

    /**
     * Verifies text styles are propagated correctly.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testTextStyles() throws Throwable {
        final String data = "<html><head><style> "
                + "    body { font: italic bold 12px Courier; }"
                + "    </style></head><body><p>foo</p></body></html>";
        TestViewStructureInterface root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface para = root.getChild(0);
        int style = para.getStyle();
        Assert.assertTrue(0 != (style & ViewNode.TEXT_STYLE_BOLD));
        Assert.assertTrue(0 != (style & ViewNode.TEXT_STYLE_ITALIC));
        Assert.assertFalse(0 != (style & ViewNode.TEXT_STYLE_UNDERLINE));
        Assert.assertFalse(0 != (style & ViewNode.TEXT_STYLE_STRIKE_THRU));

        TestViewStructureInterface paraText = para.getChild(0);
        Assert.assertEquals("foo", paraText.getText());
    }

    /**
     * Verifies the strong style is propagated correctly.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testStrongStyle() throws Throwable {
        final String data = "<html><body><p>foo</p><p><strong>bar</strong></p></body></html>";
        TestViewStructureInterface root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(2, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child1 = root.getChild(0);
        Assert.assertEquals("foo", child1.getChild(0).getText());
        int child1style = child1.getStyle();
        Assert.assertFalse(0 != (child1style & ViewNode.TEXT_STYLE_BOLD));
        TestViewStructureInterface child2 = root.getChild(1);
        TestViewStructureInterface child2child = child2.getChild(0);
        Assert.assertEquals("bar", child2child.getText());
        Assert.assertEquals(child1.getTextSize(), child2child.getTextSize(), 0);
        int child2childstyle = child2child.getStyle();
        Assert.assertTrue(0 != (child2childstyle & ViewNode.TEXT_STYLE_BOLD));
    }

    /**
     * Verifies the italic style is propagated correctly.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testItalicStyle() throws Throwable {
        final String data = "<html><body><i>foo</i></body></html>";
        TestViewStructureInterface root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child = root.getChild(0);
        TestViewStructureInterface grandchild = child.getChild(0);
        int style = grandchild.getStyle();
        Assert.assertTrue(0 != (style & ViewNode.TEXT_STYLE_ITALIC));
    }

    /**
     * Verifies the bold style is propagated correctly.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testBoldStyle() throws Throwable {
        final String data = "<html><body><b>foo</b></body></html>";
        TestViewStructureInterface root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child = root.getChild(0);
        TestViewStructureInterface grandchild = child.getChild(0);
        int style = grandchild.getStyle();
        Assert.assertTrue(0 != (style & ViewNode.TEXT_STYLE_BOLD));
    }

    /**
     * Test selection is propagated when it spans one character.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testOneCharacterSelection() throws Throwable {
        final String data = "<html><body><b id='node' role='none'>foo</b></body></html>";
        final String js = getSelectionScript("node", 0, "node", 1);
        TestViewStructureInterface root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child = root.getChild(0);
        TestViewStructureInterface grandchild = child.getChild(0);
        Assert.assertEquals("foo", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(1, grandchild.getTextSelectionEnd());
    }

    /**
     * Test selection is propagated when it spans one node.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testOneNodeSelection() throws Throwable {
        final String data = "<html><body><b id='node' role='none'>foo</b></body></html>";
        final String js = getSelectionScript("node", 0, "node", 3);
        TestViewStructureInterface root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child = root.getChild(0);
        TestViewStructureInterface grandchild = child.getChild(0);
        Assert.assertEquals("foo", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(3, grandchild.getTextSelectionEnd());
    }

    /**
     * Test selection is propagated when it spans to the beginning of the next node.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testSubsequentNodeSelection() throws Throwable {
        final String data = "<html><body><b id='node1' role='none'>foo</b>"
                + "<b id='node2' role='none'>bar</b></body></html>";
        final String js = getSelectionScript("node1", 1, "node2", 1);
        TestViewStructureInterface root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child = root.getChild(0);
        TestViewStructureInterface grandchild = child.getChild(0);
        Assert.assertEquals("foo", grandchild.getText());
        Assert.assertEquals(1, grandchild.getTextSelectionStart());
        Assert.assertEquals(3, grandchild.getTextSelectionEnd());
        grandchild = child.getChild(1);
        Assert.assertEquals("bar", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(1, grandchild.getTextSelectionEnd());
    }

    /**
     * Test selection is propagated across multiple nodes.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testMultiNodeSelection() throws Throwable {
        final String data = "<html><body><b id='node1' role='none'>foo</b><b>middle</b>"
                + "<b id='node2' role='none'>bar</b></body></html>";
        final String js = getSelectionScript("node1", 1, "node2", 1);
        TestViewStructureInterface root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child = root.getChild(0);
        TestViewStructureInterface grandchild = child.getChild(0);
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

    /**
     * Test selection is propagated from an HTML input element.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testRequestAccessibilitySnapshotInputSelection() throws Throwable {
        final String data = "<html><body><input id='input' value='Hello, world'></body></html>";
        final String js = "var input = document.getElementById('input');"
                + "input.select();"
                + "input.selectionStart = 0;"
                + "input.selectionEnd = 5;";

        TestViewStructureInterface root = getViewStructureFromHtml(data, js).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child = root.getChild(0);
        TestViewStructureInterface grandchild = child.getChild(0);
        Assert.assertEquals("Hello, world", grandchild.getText());
        Assert.assertEquals(0, grandchild.getTextSelectionStart());
        Assert.assertEquals(5, grandchild.getTextSelectionEnd());
    }

    /**
     * Test that the value is propagated from an HTML password field.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @RequiresApi(Build.VERSION_CODES.M)
    public void testRequestAccessibilitySnapshotPasswordField() throws Throwable {
        final String data =
                "<html><body><input id='input' type='password' value='foo'></body></html>";
        TestViewStructureInterface root = getViewStructureFromHtml(data).getChild(0);

        Assert.assertEquals(1, root.getChildCount());
        Assert.assertEquals("", root.getText());
        TestViewStructureInterface child = root.getChild(0);
        TestViewStructureInterface grandchild = child.getChild(0);
        Assert.assertEquals("•••", grandchild.getText());
    }
}
