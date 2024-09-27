// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.ALL_UPDATES;
import static org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.NONE;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.RenderFrameHostTestExt;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/** Assertions for GestureListenerManager. */
@RunWith(BaseJUnit4ClassRunner.class)
public class GestureListenerManagerTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    // The page should be large enough so that scrolling occurs.
    private static final String TEST_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<html><body style='height: 10000px'><script>window.addEventListener('load', ()"
                            + " => { document.title = 'loaded'; });</script>");

    private static final class GestureStateListenerImpl extends GestureStateListener {
        private int mNumOnScrollOffsetOrExtentChangedCalls;
        public CallbackHelper mCallbackHelper = new CallbackHelper();
        private boolean mGotStarted;
        private boolean mDidScrollOffsetChangeWhileScrolling;
        private Integer mLastScrollOffsetY;

        @Override
        public void onScrollStarted(int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
            org.chromium.base.Log.e("chrome", "!!!onScrollStarted " + scrollOffsetY);
            mGotStarted = true;
            mLastScrollOffsetY = null;
        }

        @Override
        public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
            org.chromium.base.Log.e(
                    "chrome",
                    "!!!onScrollOffsetOrExtentChanged started="
                            + mGotStarted
                            + " scroll="
                            + scrollOffsetY
                            + " last="
                            + mLastScrollOffsetY);
            if (mGotStarted
                    && (mLastScrollOffsetY == null || !mLastScrollOffsetY.equals(scrollOffsetY))) {
                if (mLastScrollOffsetY != null) mDidScrollOffsetChangeWhileScrolling = true;
                mLastScrollOffsetY = scrollOffsetY;
            }
        }

        @Override
        public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
            org.chromium.base.Log.e("chrome", "!!!onScrollEnded, offset=" + scrollOffsetY);
            // onScrollEnded() should be preceded by onScrollStarted().
            Assert.assertTrue(mGotStarted);
            // onScrollOffsetOrExtentChanged() should be called at least twice. Once with an initial
            // value, and then with a different value.
            Assert.assertTrue(mDidScrollOffsetChangeWhileScrolling);
            mCallbackHelper.notifyCalled();
            mGotStarted = false;
        }
    }

    private float mCurrentX;
    private float mCurrentY;

    /** Assertions for GestureStateListener.onScrollOffsetOrExtentChanged. */
    @Test
    @LargeTest
    @Feature({"Browser"})
    @DisabledTest(message = "https://crbug.com/1324302")
    public void testOnScrollOffsetOrExtentChanged() throws Throwable {
        mActivityTestRule.launchContentShellWithUrl("about:blank");
        WebContents webContents = mActivityTestRule.getWebContents();
        // This needs to wait for first-paint, otherwise scrolling doesn't happen.
        TestCallbackHelperContainer callbackHelperContainer =
                new TestCallbackHelperContainer(webContents);
        mActivityTestRule.loadUrl(
                webContents.getNavigationController(),
                callbackHelperContainer,
                new LoadUrlParams(TEST_URL));
        // Wait for the first non-empty visual paint and the title to change. The title changes when
        // the doc has finished loading, which is a good signal events can be processed.
        callbackHelperContainer.getOnFirstVisuallyNonEmptyPaintHelper().waitForCallback(0);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(webContents.getTitle(), Matchers.is("loaded")));

        // At this point the page has finished loading and a non-empty paint occurred. This does not
        // mean the renderer is fully ready to process events (processing events requires layers,
        // which may not have been created yet). Wait for a visual update, which should ensure the
        // renderer is ready.
        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new RenderFrameHostTestExt(webContents.getMainFrame())
                            .updateVisualState(
                                    (Boolean result) -> {
                                        Assert.assertTrue(result);
                                        callbackHelper.notifyCalled();
                                    });
                });
        callbackHelper.waitForOnly();

        final GestureStateListenerImpl listener = new GestureStateListenerImpl();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GestureListenerManagerImpl manager =
                            (GestureListenerManagerImpl)
                                    GestureListenerManager.fromWebContents(webContents);
                    // getRootScrollOffsetUpdateFrequency() should initially return NONE (as there
                    // are no listeners).
                    Assert.assertEquals(
                            NONE, manager.getRootScrollOffsetUpdateFrequencyForTesting());
                    manager.addListener(listener, ALL_UPDATES);
                    // Adding a listener changes this to ALL_UPDATES.
                    Assert.assertEquals(
                            ALL_UPDATES, manager.getRootScrollOffsetUpdateFrequencyForTesting());
                    View webContentsView = webContents.getViewAndroidDelegate().getContainerView();
                    mCurrentX = webContentsView.getWidth() / 2f;
                    mCurrentY = webContentsView.getHeight() / 2f;
                    Assert.assertTrue(mCurrentY > 0);
                });

        // Perform a scroll.
        TouchCommon.performDrag(
                mActivityTestRule.getActivity(),
                mCurrentX,
                mCurrentX,
                mCurrentY,
                mCurrentY - 50,
                /* stepCount= */ 3, /* duration in ms */
                250);
        listener.mCallbackHelper.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GestureListenerManagerImpl manager =
                            (GestureListenerManagerImpl)
                                    GestureListenerManager.fromWebContents(webContents);
                    manager.removeListener(listener);
                    // Should go back to NONE after removing the only listener.
                    Assert.assertEquals(
                            NONE, manager.getRootScrollOffsetUpdateFrequencyForTesting());
                });
    }
}
