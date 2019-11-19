// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.ViewEventSink.InternalAccessDelegate;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.content_shell_apk.ContentShellActivityTestRule.RerunWithUpdatedContainerView;

/**
 * Tests that we can scroll and fling a ContentView running inside ContentShell.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ContentViewScrollingTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String LARGE_PAGE = UrlUtils.encodeHtmlDataUri("<html><head>"
            + "<meta name=\"viewport\" content=\"width=device-width, "
            + "initial-scale=2.0, maximum-scale=2.0\" />"
            + "<style>body { width: 5000px; height: 5000px; }</style></head>"
            + "<body>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</body>"
            + "</html>");

    /**
     * InternalAccessDelegate to ensure AccessibilityEvent notifications (Eg:TYPE_VIEW_SCROLLED)
     * are being sent properly on scrolling a page.
     */
    static class TestInternalAccessDelegate implements InternalAccessDelegate {

        private boolean mScrollChanged;
        private final Object mLock = new Object();

        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return false;
        }

        @Override
        public void onScrollChanged(int lPix, int tPix, int oldlPix, int oldtPix) {
            synchronized (mLock) {
                mScrollChanged = true;
            }
        }

        /**
         * @return Whether OnScrollChanged() has been called.
         */
        public boolean isScrollChanged() {
            synchronized (mLock) {
                return mScrollChanged;
            }
        }
    }

    private RenderCoordinatesImpl mCoordinates;

    private void waitForScroll(final boolean hugLeft, final boolean hugTop) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // Scrolling and flinging don't result in exact coordinates.
                final int minThreshold = 5;
                final int maxThreshold = 100;

                boolean xCorrect = hugLeft ? mCoordinates.getScrollXPixInt() < minThreshold
                                           : mCoordinates.getScrollXPixInt() > maxThreshold;
                boolean yCorrect = hugTop ? mCoordinates.getScrollYPixInt() < minThreshold
                                          : mCoordinates.getScrollYPixInt() > maxThreshold;
                return xCorrect && yCorrect;
            }
        });
    }

    private void waitForScrollToPosition(final int x, final int y) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // Scrolling and flinging don't result in exact coordinates.
                final int threshold = 5;

                boolean xCorrect = (mCoordinates.getScrollXPixInt() < x + threshold
                        && mCoordinates.getScrollXPixInt() > x - threshold);
                boolean yCorrect = (mCoordinates.getScrollYPixInt() < y + threshold
                        && mCoordinates.getScrollYPixInt() > y - threshold);
                return xCorrect && yCorrect;
            }
        });
    }

    private void waitForViewportInitialization() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCoordinates.getLastFrameViewportWidthPixInt() != 0;
            }
        });
    }

    private void fling(final int vx, final int vy) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getWebContents().getEventForwarder().startFling(
                        SystemClock.uptimeMillis(), vx, vy, false, true);
            }
        });
    }

    private void scrollTo(final int x, final int y) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getContainerView().scrollTo(x, y);
            }
        });
    }

    private void scrollBy(final int dx, final int dy) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getContainerView().scrollBy(dx, dy);
            }
        });
    }

    private void scrollWithJoystick(final float deltaAxisX, final float deltaAxisY) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Synthesize joystick motion event and send to content layer.
                MotionEvent leftJoystickMotionEvent =
                        MotionEvent.obtain(0, SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE,
                                deltaAxisX, deltaAxisY, 0);
                leftJoystickMotionEvent.setSource(
                        leftJoystickMotionEvent.getSource() | InputDevice.SOURCE_CLASS_JOYSTICK);
                mActivityTestRule.getContainerView().onGenericMotionEvent(leftJoystickMotionEvent);
            }
        });
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchContentShellWithUrl(LARGE_PAGE);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mCoordinates = mActivityTestRule.getRenderCoordinates();
        WebContentsUtils.reportAllFrameSubmissions(mActivityTestRule.getWebContents(), true);
        waitForViewportInitialization();

        Assert.assertEquals(0, mCoordinates.getScrollXPixInt());
        Assert.assertEquals(0, mCoordinates.getScrollYPixInt());
    }

    @Test
    @SmallTest
    @Feature({"Main"})
    @RetryOnFailure
    public void testFling() {
        // Scaling the initial velocity by the device scale factor ensures that
        // it's of sufficient magnitude for all displays densities.
        float deviceScaleFactor = InstrumentationRegistry.getInstrumentation()
                                          .getTargetContext()
                                          .getResources()
                                          .getDisplayMetrics()
                                          .density;
        int velocity = (int) (1000 * deviceScaleFactor);

        // Vertical fling to lower-left.
        fling(0, -velocity);
        waitForScroll(true, false);

        // Horizontal fling to lower-right.
        fling(-velocity, 0);
        waitForScroll(false, false);

        // Vertical fling to upper-right.
        fling(0, velocity);
        waitForScroll(false, true);

        // Horizontal fling to top-left.
        fling(velocity, 0);
        waitForScroll(true, true);

        // Diagonal fling to bottom-right.
        fling(-velocity, -velocity);
        waitForScroll(false, false);
    }

    @Test
    @SmallTest
    @Feature({"Main"})
    @RetryOnFailure
    public void testFlingDistance() {
        // Scaling the initial velocity by the device scale factor ensures that
        // it's of sufficient magnitude for all displays densities.
        float deviceScaleFactor = InstrumentationRegistry.getInstrumentation()
                                          .getTargetContext()
                                          .getResources()
                                          .getDisplayMetrics()
                                          .density;
        int velocity = (int) (1000 * deviceScaleFactor);
        // Expected total fling distance calculated by FlingCurve with initial
        // velocity 1000.
        int expected_dist = (int) (180 * deviceScaleFactor);

        // Vertical fling to lower-left.
        fling(0, -velocity);
        waitForScrollToPosition(0, expected_dist);

        // Horizontal fling to lower-right.
        fling(-velocity, 0);
        waitForScrollToPosition(expected_dist, expected_dist);

        // Vertical fling to upper-right.
        fling(0, velocity);
        waitForScrollToPosition(expected_dist, 0);

        // Horizontal fling to top-left.
        fling(velocity, 0);
        waitForScrollToPosition(0, 0);
    }

    @Test
    @SmallTest
    @RerunWithUpdatedContainerView
    @Feature({"Main"})
    @RetryOnFailure
    public void testScrollTo() {
        // Vertical scroll to lower-left.
        scrollTo(0, 2500);
        waitForScroll(true, false);

        // Horizontal scroll to lower-right.
        scrollTo(2500, 2500);
        waitForScroll(false, false);

        // Vertical scroll to upper-right.
        scrollTo(2500, 0);
        waitForScroll(false, true);

        // Horizontal scroll to top-left.
        scrollTo(0, 0);
        waitForScroll(true, true);

        // Diagonal scroll to bottom-right.
        scrollTo(2500, 2500);
        waitForScroll(false, false);
    }

    @Test
    @SmallTest
    @RerunWithUpdatedContainerView
    @Feature({"Main"})
    @RetryOnFailure
    public void testScrollBy() {
        scrollTo(0, 0);
        waitForScroll(true, true);

        // No scroll
        scrollBy(0, 0);
        waitForScroll(true, true);

        // Vertical scroll to lower-left.
        scrollBy(0, 2500);
        waitForScroll(true, false);

        // Horizontal scroll to lower-right.
        scrollBy(2500, 0);
        waitForScroll(false, false);

        // Vertical scroll to upper-right.
        scrollBy(0, -2500);
        waitForScroll(false, true);

        // Horizontal scroll to top-left.
        scrollBy(-2500, 0);
        waitForScroll(true, true);

        // Diagonal scroll to bottom-right.
        scrollBy(2500, 2500);
        waitForScroll(false, false);
    }

    @Test
    @SmallTest
    @Feature({"Main"})
    public void testJoystickScroll() {
        scrollTo(0, 0);
        waitForScroll(true, true);

        // Scroll with X axis in deadzone and the Y axis active.
        // Only the Y axis should have an effect, arriving at lower-left.
        scrollWithJoystick(0.1f, 1f);
        waitForScroll(true, false);

        // Scroll with Y axis in deadzone and the X axis active.
        scrollWithJoystick(1f, -0.1f);
        waitForScroll(false, false);

        // Vertical scroll to upper-right.
        scrollWithJoystick(0, -0.75f);
        waitForScroll(false, true);

        // Horizontal scroll to top-left.
        scrollWithJoystick(-0.75f, 0);
        waitForScroll(true, true);

        // Diagonal scroll to bottom-right.
        scrollWithJoystick(1f, 1f);
        waitForScroll(false, false);
    }

    /**
     * To ensure the device properly responds to bounds-exceeding scrolls, e.g., overscroll
     * effects are properly initialized.
     */
    @Test
    @SmallTest
    @RerunWithUpdatedContainerView
    @Feature({"Main"})
    @RetryOnFailure
    public void testOverScroll() {
        // Overscroll lower-left.
        scrollTo(-10000, 10000);
        waitForScroll(true, false);

        // Overscroll lower-right.
        scrollTo(10000, 10000);
        waitForScroll(false, false);

        // Overscroll upper-right.
        scrollTo(10000, -10000);
        waitForScroll(false, true);

        // Overscroll top-left.
        scrollTo(-10000, -10000);
        waitForScroll(true, true);

        // Diagonal overscroll lower-right.
        scrollTo(10000, 10000);
        waitForScroll(false, false);
    }

    /**
     * To ensure the AccessibilityEvent notifications (Eg:TYPE_VIEW_SCROLLED) are being sent
     * properly on scrolling a page.
     */
    @Test
    @SmallTest
    @RerunWithUpdatedContainerView
    @Feature({"Main"})
    @RetryOnFailure
    public void testOnScrollChanged() {
        final int scrollToX = mCoordinates.getScrollXPixInt() + 2500;
        final int scrollToY = mCoordinates.getScrollYPixInt() + 2500;
        final TestInternalAccessDelegate accessDelegate = new TestInternalAccessDelegate();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getViewEventSink().setAccessDelegate(accessDelegate);
            }
        });
        scrollTo(scrollToX, scrollToY);
        waitForScroll(false, false);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return accessDelegate.isScrollChanged();
            }
        });
    }
}
