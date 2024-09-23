// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/** Integration tests for JavaScript execution. */
@RunWith(BaseJUnit4ClassRunner.class)
public class TestsJavaScriptEvalTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String JSTEST_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<html><head><script>"
                            + "  function foobar() { return 'foobar'; }"
                            + "</script></head>"
                            + "<body><button id=\"test\">Test button</button></body></html>");

    public TestsJavaScriptEvalTest() {}

    /**
     * Tests that evaluation of JavaScript for test purposes (using JavaScriptUtils, DOMUtils etc)
     * works even in presence of "background" (non-test-initiated) JavaScript evaluation activity.
     */
    @Test
    @LargeTest
    @Feature({"Browser"})
    public void testJavaScriptEvalIsCorrectlyOrdered() throws Exception, Throwable {
        mActivityTestRule.launchContentShellWithUrl(JSTEST_URL);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        final WebContents webContents = mActivityTestRule.getWebContents();
        for (int i = 0; i < 30; ++i) {
            for (int j = 0; j < 10; ++j) {
                // Start evaluation of a JavaScript script -- we don't need a result.
                ThreadUtils.runOnUiThreadBlocking(
                        () -> webContents.evaluateJavaScriptForTests("foobar();", null));
            }
            // DOMUtils does need to evaluate a JavaScript and get its result to get DOM bounds.
            Assert.assertNotNull(
                    "Failed to get bounds", DOMUtils.getNodeBounds(webContents, "test"));
        }
    }
}
