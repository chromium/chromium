// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests pausing the VSync loop for a WindowAndroid. */
@RunWith(BaseJUnit4ClassRunner.class)
public class VSyncPausedTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String VSYNC_HTML = "content/test/data/android/vsync.html";
    private static final String CALL_RAF = "window.requestAnimationFrame(onAnimationFrame);";

    private CallbackHelper mOnTitleUpdatedHelper;
    private String mTitle;

    private WebContentsObserver mObserver;
    private ContentShellActivity mActivity;

    @Before
    public void setUp() throws Exception {
        mActivity =
                mActivityTestRule.launchContentShellWithUrl(
                        UrlUtils.getIsolatedTestFileUrl(VSYNC_HTML));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        final WebContents webContents = mActivity.getActiveWebContents();
        mObserver =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new WebContentsObserver(webContents) {
                                    @Override
                                    public void titleWasSet(String title) {
                                        mTitle = title;
                                        mOnTitleUpdatedHelper.notifyCalled();
                                    }
                                });
        mOnTitleUpdatedHelper = new CallbackHelper();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mObserver.destroy());
    }

    @Test
    @MediumTest
    public void testPauseVSync() throws Throwable {
        int callCount = mOnTitleUpdatedHelper.getCallCount();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivity.getActiveWebContents(), CALL_RAF);
        mOnTitleUpdatedHelper.waitForCallback(callCount);
        Assert.assertEquals("1", mTitle);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getActiveShell()
                            .getWebContents()
                            .getTopLevelNativeWindow()
                            .setVSyncPaused(true);
                });
        callCount = mOnTitleUpdatedHelper.getCallCount();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivity.getActiveWebContents(), CALL_RAF);
        try {
            mOnTitleUpdatedHelper.waitForCallback(callCount, 1, 1, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            // Timeout is expected.
        }
        // There may be a VSync already propagating before we pause VSync, so we may receive a
        // single extra VSync.
        String expected = "2";
        if (mTitle.equals("2")) {
            expected = "3";
            callCount = mOnTitleUpdatedHelper.getCallCount();
            // Make sure we don't receive another extra VSync.
            JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    mActivity.getActiveWebContents(), CALL_RAF);
            try {
                mOnTitleUpdatedHelper.waitForCallback(callCount, 1, 1, TimeUnit.SECONDS);
            } catch (TimeoutException e) {
                // Timeout is expected.
            }
            Assert.assertEquals("2", mTitle);
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getActiveShell()
                            .getWebContents()
                            .getTopLevelNativeWindow()
                            .setVSyncPaused(false);
                });
        mOnTitleUpdatedHelper.waitForCallback(callCount);
        Assert.assertEquals(expected, mTitle);
    }
}
