// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.InterstitialPageDelegateAndroid;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for interstitial pages.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class InterstitialPageTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String URL = UrlUtils.encodeHtmlDataUri(
            "<html><head></head><body>test</body></html>");

    private static class TestWebContentsObserver extends WebContentsObserver {
        private boolean mInterstitialShowing;

        public TestWebContentsObserver(WebContents webContents) {
            super(webContents);
        }

        public boolean isInterstitialShowing() throws ExecutionException {
            return TestThreadUtils
                    .runOnUiThreadBlocking(new Callable<Boolean>() {
                        @Override
                        public Boolean call() {
                            return mInterstitialShowing;
                        }
                    })
                    .booleanValue();
        }

        @Override
        public void didAttachInterstitialPage() {
            mInterstitialShowing = true;
        }

        @Override
        public void didDetachInterstitialPage() {
            mInterstitialShowing = false;
        }
    }

    @Before
    public void setUp() {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl(URL);
        Assert.assertNotNull(activity);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
    }

    private void waitForInterstitial(final boolean shouldBeShown) {
        CriteriaHelper.pollUiThread(
                Criteria.equals(shouldBeShown, new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return mActivityTestRule.getWebContents().isShowingInterstitialPage();
                    }
                }));
    }

    /**
     * Tests that showing and hiding an interstitial works.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    @DisabledTest(message = "crbug.com/1022324")
    public void testCloseInterstitial() throws ExecutionException {
        final String proceedCommand = "PROCEED";
        final String htmlContent = "<html>"
                + "<head>"
                + "  <script>"
                + "    function sendCommand(command) {"
                + "      window.domAutomationController.send(command);"
                + "    }"
                + "  </script>"
                + "</head>"
                + "<body style='background-color:#FF0000' "
                + "  onclick='sendCommand(\"" + proceedCommand + "\");'>"
                + "  <h1>This is a scary interstitial page</h1>"
                + "</body>"
                + "</html>";
        final InterstitialPageDelegateAndroid delegate =
                new InterstitialPageDelegateAndroid(htmlContent) {
            @Override
            protected void commandReceived(String command) {
                Assert.assertEquals(command, proceedCommand);
                proceed();
            }
        };
        TestWebContentsObserver observer =
                TestThreadUtils.runOnUiThreadBlocking(new Callable<TestWebContentsObserver>() {
                    @Override
                    public TestWebContentsObserver call() {
                        delegate.showInterstitialPage(URL, mActivityTestRule.getWebContents());
                        return new TestWebContentsObserver(mActivityTestRule.getWebContents());
                    }
                });

        waitForInterstitial(true);
        Assert.assertTrue("WebContentsObserver not notified of interstitial showing",
                observer.isInterstitialShowing());
        TouchCommon.singleClickView(mActivityTestRule.getContainerView(), 10, 10);
        waitForInterstitial(false);
        Assert.assertTrue("WebContentsObserver not notified of interstitial hiding",
                !observer.isInterstitialShowing());
    }
}
