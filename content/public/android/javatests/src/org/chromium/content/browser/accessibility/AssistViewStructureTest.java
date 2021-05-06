// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

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
    private TestViewStructureInterface getViewStructureFromHtml(String htmlContent) {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(htmlContent));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

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
     * Test simple paragraph.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @TargetApi(Build.VERSION_CODES.M)
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
    @TargetApi(Build.VERSION_CODES.M)
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
                        + "        android.view.View\n"
                        + "          android.widget.TextView text='1. '\n"
                        + "        android.widget.TextView text='Kirk'\n"
                        + "      android.view.View\n"
                        + "        android.view.View\n"
                        + "          android.widget.TextView text='2. '\n"
                        + "        android.widget.TextView text='Picard'\n"
                        + "      android.view.View\n"
                        + "        android.view.View\n"
                        + "          android.widget.TextView text='3. '\n"
                        + "        android.widget.TextView text='Janeway'\n");
    }

    /**
     * Test that the snapshot contains the url.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @TargetApi(Build.VERSION_CODES.M)
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
    @TargetApi(Build.VERSION_CODES.M)
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
    @TargetApi(Build.VERSION_CODES.M)
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
    @TargetApi(Build.VERSION_CODES.M)
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
}
