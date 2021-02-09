// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.os.Build;

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
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.M)
    public void testSimpleParagraph() throws Throwable {
        TestViewStructureInterface testViewStructure =
                getViewStructureFromHtml("<p>Hello World</p>");
        Assert.assertEquals(testViewStructure.toString(),
                "\n"
                        + "  android.webkit.WebView\n"
                        + "    android.view.View text='Hello World'\n"
                        + "      android.widget.TextView text='Hello World'\n");
    }

    /**
     * Test static list.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @TargetApi(Build.VERSION_CODES.M)
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.M)
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
                        + "          android.widget.TextView text='1. '\n"
                        + "        android.widget.TextView text='Kirk'\n"
                        + "      android.view.View\n"
                        + "        android.view.View text='2. '\n"
                        + "          android.widget.TextView text='2. '\n"
                        + "        android.widget.TextView text='Picard'\n"
                        + "      android.view.View\n"
                        + "        android.view.View text='3. '\n"
                        + "          android.widget.TextView text='3. '\n"
                        + "        android.widget.TextView text='Janeway'\n");
    }
}
