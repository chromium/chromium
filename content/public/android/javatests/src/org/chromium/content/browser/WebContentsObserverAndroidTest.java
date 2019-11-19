// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.Callable;

/**
 * Tests for the WebContentsObserver APIs.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebContentsObserverAndroidTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String URL = UrlUtils.encodeHtmlDataUri(
            "<html><head></head><body>didFirstVisuallyNonEmptyPaint test</body></html>");

    private static class TestWebContentsObserver extends WebContentsObserver {
        private CallbackHelper mDidFirstVisuallyNonEmptyPaintCallbackHelper = new CallbackHelper();

        public TestWebContentsObserver(WebContents webContents) {
            super(webContents);
        }

        public CallbackHelper getDidFirstVisuallyNonEmptyPaintCallbackHelper() {
            return mDidFirstVisuallyNonEmptyPaintCallbackHelper;
        }

        @Override
        public void didFirstVisuallyNonEmptyPaint() {
            mDidFirstVisuallyNonEmptyPaintCallbackHelper.notifyCalled();
        }
    }

    @Before
    public void setUp() {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl(null);
        Assert.assertNotNull(activity);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
    }

    /*
    @SmallTest
    @Feature({"Navigation"})
    */
    @Test
    @DisabledTest(message = "crbug.com/411931")
    public void testDidFirstVisuallyNonEmptyPaint() throws Throwable {
        TestWebContentsObserver observer =
                TestThreadUtils.runOnUiThreadBlocking(new Callable<TestWebContentsObserver>() {
                    @Override
                    public TestWebContentsObserver call() {
                        return new TestWebContentsObserver(mActivityTestRule.getWebContents());
                    }
                });

        int callCount = observer.getDidFirstVisuallyNonEmptyPaintCallbackHelper().getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getWebContents().getNavigationController().loadUrl(
                        new LoadUrlParams(URL));
            }
        });
        observer.getDidFirstVisuallyNonEmptyPaintCallbackHelper().waitForCallback(callCount);
    }
}
