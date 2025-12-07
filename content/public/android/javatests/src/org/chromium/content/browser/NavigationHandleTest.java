// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@code NavigationHandle}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class NavigationHandleTest {
    private static final String URL_1 = UrlUtils.encodeHtmlDataUri("<html>1</html>");
    private static final String URL_2 = UrlUtils.encodeHtmlDataUri("<html>2</html>");

    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private final CallbackHelper mDidFinishNavigationCallback = new CallbackHelper();
    private final AtomicReference<NavigationHandle> mLastNavigationHandle = new AtomicReference<>();
    private final WebContentsObserver mObserver =
            new WebContentsObserver() {
                @Override
                public void didFinishNavigationInPrimaryMainFrame(
                        NavigationHandle navigationHandle) {
                    mLastNavigationHandle.set(navigationHandle);
                    mDidFinishNavigationCallback.notifyCalled();
                }
            };

    private NavigationController mNavController;

    @Before
    public void setUp() {
        mActivityTestRule.launchContentShellWithUrl(URL_1);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        WebContents webContents = mActivityTestRule.getWebContents();
        ThreadUtils.runOnUiThreadBlocking(() -> mObserver.observe(webContents));
        mNavController = webContents.getNavigationController();
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void url() throws Throwable {
        NavigationHandle handle =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mNavController.loadUrl(new LoadUrlParams(URL_2)));
        Assert.assertEquals(URL_2, handle.getUrl().getSpec());
    }

    private void runOnUiThreadAndWaitForNavigation(Runnable runnable) throws TimeoutException {
        int count = mDidFinishNavigationCallback.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(runnable);
        mDidFinishNavigationCallback.waitForCallback(count);
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void isReload() throws Throwable {
        runOnUiThreadAndWaitForNavigation(() -> mNavController.reload(false));

        Assert.assertTrue(mLastNavigationHandle.get().isReload());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void isBack() throws Throwable {
        runOnUiThreadAndWaitForNavigation(() -> mNavController.loadUrl(new LoadUrlParams(URL_2)));
        runOnUiThreadAndWaitForNavigation(() -> mNavController.goBack());

        Assert.assertTrue(mLastNavigationHandle.get().isHistory());
        Assert.assertTrue(mLastNavigationHandle.get().isBack());
        Assert.assertEquals(URL_1, mLastNavigationHandle.get().getUrl().getSpec());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void isForward() throws Throwable {
        runOnUiThreadAndWaitForNavigation(() -> mNavController.loadUrl(new LoadUrlParams(URL_2)));
        runOnUiThreadAndWaitForNavigation(() -> mNavController.goBack());
        runOnUiThreadAndWaitForNavigation(() -> mNavController.goForward());

        Assert.assertTrue(mLastNavigationHandle.get().isHistory());
        Assert.assertTrue(mLastNavigationHandle.get().isForward());
        Assert.assertEquals(URL_2, mLastNavigationHandle.get().getUrl().getSpec());
    }
}
