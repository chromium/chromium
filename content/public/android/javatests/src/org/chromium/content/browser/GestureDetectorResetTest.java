// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.TimeUnit;

/**
 * Provides test environment for Gesture Detector Reset for Content Shell.
 * This is a helper class for Content Shell tests.
*/
@RunWith(BaseJUnit4ClassRunner.class)
public class GestureDetectorResetTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final long WAIT_TIMEOUT_SECONDS = 2L;
    private static final String CLICK_TEST_URL = UrlUtils.encodeHtmlDataUri("<html><body>"
            + "<button id=\"button\" "
            + "  onclick=\"document.getElementById('test').textContent = 'clicked';\">"
            + "Button"
            + "</button><br/>"
            + "<div id=\"test\">not clicked</div><br/>"
            + "</body></html>");

    private static class NodeContentsIsEqualToCriteria extends Criteria {
        private final WebContents mWebContents;
        private final String mNodeId;
        private final String mExpectedContents;

        public NodeContentsIsEqualToCriteria(String failureReason, WebContents webContents,
                String nodeId, String expectedContents) {
            super(failureReason);
            mWebContents = webContents;
            mNodeId = nodeId;
            mExpectedContents = expectedContents;
            assert mExpectedContents != null;
        }

        @Override
        public boolean isSatisfied() {
            try {
                String contents = DOMUtils.getNodeContents(mWebContents, mNodeId);
                return mExpectedContents.equals(contents);
            } catch (Throwable e) {
                Assert.fail("Failed to retrieve node contents: " + e);
                return false;
            }
        }
    }

    public GestureDetectorResetTest() {
    }

    private void verifyClicksAreRegistered(String disambiguation, WebContents webContents)
            throws Exception, Throwable {
        // Initially the text on the page should say "not clicked".
        CriteriaHelper.pollInstrumentationThread(
                new NodeContentsIsEqualToCriteria("The page contents is invalid " + disambiguation,
                        webContents, "test", "not clicked"));

        // Click the button.
        DOMUtils.clickNode(webContents, "button");

        // After the click, the text on the page should say "clicked".
        CriteriaHelper.pollInstrumentationThread(new NodeContentsIsEqualToCriteria(
                "The page contents didn't change after a click " + disambiguation, webContents,
                "test", "clicked"));
    }

    /**
     * Tests that showing a select popup and having the page reload while the popup is showing does
     * not assert.
     */
    @Test
    @LargeTest
    @Feature({"Browser"})
    @RetryOnFailure
    public void testSeparateClicksAreRegisteredOnReload()
            throws InterruptedException, Exception, Throwable {
        // Load the test page.
        mActivityTestRule.launchContentShellWithUrl(CLICK_TEST_URL);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        final WebContents webContents = mActivityTestRule.getWebContents();
        final TestCallbackHelperContainer viewClient = new TestCallbackHelperContainer(webContents);
        final OnPageFinishedHelper onPageFinishedHelper =
                viewClient.getOnPageFinishedHelper();

        // Test that the button click works.
        verifyClicksAreRegistered("on initial load", webContents);

        // Reload the test page.
        int currentCallCount = onPageFinishedHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().getActiveShell().loadUrl(CLICK_TEST_URL);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1,
                WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Test that the button click still works.
        verifyClicksAreRegistered("after reload", webContents);

        // Directly navigate to the test page.
        currentCallCount = onPageFinishedHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity()
                        .getActiveShell()
                        .getWebContents()
                        .getNavigationController()
                        .loadUrl(new LoadUrlParams(CLICK_TEST_URL));
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1,
                WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Test that the button click still works.
        verifyClicksAreRegistered("after direct navigation", webContents);
    }
}
