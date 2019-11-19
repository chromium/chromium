// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.pm.ActivityInfo;
import android.support.test.filters.MediumTest;
import android.view.Surface;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;

import java.util.concurrent.Callable;

/**
 * Tests for ScreenOrientationListener and its implementations.
 *
 * rotation: Surface.ROTATION_*
 * orientation: ActivityInfo.SCREEN_ORIENTATION_*
 * orientation value: ScreenOrientationValues.*
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ScreenOrientationListenerTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static class OrientationChangeCallbackHelper
            extends CallbackHelper implements DisplayAndroidObserver {
        private int mLastOrientation;

        @Override
        public void onRotationChanged(int rotation) {
            mLastOrientation = rotation;
            notifyCalled();
        }

        @Override
        public void onDIPScaleChanged(float dipScale) {}

        public int getLastRotation() {
            return mLastOrientation;
        }
    }

    private OrientationChangeCallbackHelper mCallbackHelper;
    private DisplayAndroid mDisplayAndroid;
    private int mNaturalOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchContentShellWithUrl("about:blank");
        mCallbackHelper = new OrientationChangeCallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDisplayAndroid =
                    mActivityTestRule.getWebContents().getTopLevelNativeWindow().getDisplay();
            mDisplayAndroid.addObserver(mCallbackHelper);
        });

        // Calculate device natural orientation, as mObserver.mOrientation
        // is difference between current and natural orientation in degrees.
        mNaturalOrientation = getNaturalOrientation(mDisplayAndroid);

        // Make sure we start all the tests with the same orientation.
        lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDisplayAndroid.removeObserver(mCallbackHelper);
            mDisplayAndroid = null;
            mActivityTestRule.getActivity().setRequestedOrientation(
                    ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        });

        mCallbackHelper = null;
    }

    private static int getNaturalOrientation(DisplayAndroid display) {
        int rotation = display.getRotation();
        if (rotation == Surface.ROTATION_0 || rotation == Surface.ROTATION_180) {
            if (display.getDisplayHeight() >= display.getDisplayWidth()) {
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            }
            return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        } else {
            if (display.getDisplayHeight() < display.getDisplayWidth()) {
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            }
            return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        }
    }

    private int orientationToRotation(int orientation) {
        if (mNaturalOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT) {
            switch (orientation) {
                case ActivityInfo.SCREEN_ORIENTATION_PORTRAIT:
                    return Surface.ROTATION_0;
                case ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE:
                    return Surface.ROTATION_90;
                case ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT:
                    return Surface.ROTATION_180;
                case ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE:
                    return Surface.ROTATION_270;
                default:
                    Assert.fail("Should not be there!");
                    return Surface.ROTATION_0;
            }
        } else { // mNaturalOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            switch (orientation) {
                case ActivityInfo.SCREEN_ORIENTATION_PORTRAIT:
                    return Surface.ROTATION_270;
                case ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE:
                    return Surface.ROTATION_0;
                case ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT:
                    return Surface.ROTATION_90;
                case ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE:
                    return Surface.ROTATION_180;
                default:
                    Assert.fail("Should not be there!");
                    return Surface.ROTATION_0;
            }
        }
    }

    private int getCurrentRotation() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() {
                return mDisplayAndroid.getRotation();
            }
        });
    }

    // Returns the rotation observed.
    private int lockOrientationAndWait(final int orientation) throws Exception {
        int expectedRotation = orientationToRotation(orientation);
        int currentRotation = getCurrentRotation();
        if (expectedRotation == currentRotation) return expectedRotation;

        int callCount = mCallbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().setRequestedOrientation(orientation); });
        mCallbackHelper.waitForCallback(callCount);
        return mCallbackHelper.getLastRotation();
    }

    @Test
    @MediumTest
    @Feature({"ScreenOrientation"})
    @DisabledTest
    public void testOrientationChanges() throws Exception {
        int rotation = lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        Assert.assertEquals(
                orientationToRotation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE), rotation);

        rotation = lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        Assert.assertEquals(
                orientationToRotation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT), rotation);

        rotation = lockOrientationAndWait(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        Assert.assertEquals(
                orientationToRotation(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE), rotation);

        // Note: REVERSE_PORTRAIT does not work when device orientation is locked by user (eg from
        // the notification shade). Bots on the commit queue are all locked, so don't bother testing
        // REVERSE_PORTRAIT.
    }

    private int orientationValueToRotation(int orientationValue) {
        if (mNaturalOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT) {
            switch (orientationValue) {
                case ScreenOrientationValues.PORTRAIT_PRIMARY:
                    return Surface.ROTATION_0;
                case ScreenOrientationValues.LANDSCAPE_PRIMARY:
                    return Surface.ROTATION_90;
                case ScreenOrientationValues.PORTRAIT:
                    return Surface.ROTATION_0;
                case ScreenOrientationValues.LANDSCAPE:
                    return Surface.ROTATION_90;
                case ScreenOrientationValues.LANDSCAPE_SECONDARY:
                    return Surface.ROTATION_270;
                case ScreenOrientationValues.PORTRAIT_SECONDARY:
                    return Surface.ROTATION_180;
                default:
                    Assert.fail("Can't requiest this orientation value " + orientationValue);
                    return 0;
            }
        } else { // mNaturalOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            switch (orientationValue) {
                case ScreenOrientationValues.PORTRAIT_PRIMARY:
                    return Surface.ROTATION_270;
                case ScreenOrientationValues.LANDSCAPE_PRIMARY:
                    return Surface.ROTATION_0;
                case ScreenOrientationValues.PORTRAIT:
                    return Surface.ROTATION_90;
                case ScreenOrientationValues.LANDSCAPE:
                    return Surface.ROTATION_0;
                case ScreenOrientationValues.LANDSCAPE_SECONDARY:
                    return Surface.ROTATION_180;
                case ScreenOrientationValues.PORTRAIT_SECONDARY:
                    return Surface.ROTATION_90;
                default:
                    Assert.fail("Can't requiest this orientation value " + orientationValue);
                    return 0;
            }
        }
    }

    // Returns the rotation observed.
    private int lockOrientationValueAndWait(final int orientationValue) throws Exception {
        int expectedRotation = orientationValueToRotation(orientationValue);
        int currentRotation = getCurrentRotation();
        if (expectedRotation == currentRotation) return expectedRotation;

        int callCount = mCallbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ScreenOrientationProvider.getInstance().lockOrientation(
                    mActivityTestRule.getWebContents().getTopLevelNativeWindow(),
                    (byte) orientationValue);
        });
        mCallbackHelper.waitForCallback(callCount);
        return mCallbackHelper.getLastRotation();
    }

    @Test
    @MediumTest
    @Feature({"ScreenOrientation"})
    @DisabledTest(message = "crbug.com/807356")
    public void testBasicValues() throws Exception {
        int rotation = lockOrientationValueAndWait(ScreenOrientationValues.LANDSCAPE_PRIMARY);
        Assert.assertEquals(
                orientationValueToRotation(ScreenOrientationValues.LANDSCAPE_PRIMARY), rotation);

        rotation = lockOrientationValueAndWait(ScreenOrientationValues.PORTRAIT_PRIMARY);
        Assert.assertEquals(
                orientationValueToRotation(ScreenOrientationValues.PORTRAIT_PRIMARY), rotation);

        rotation = lockOrientationValueAndWait(ScreenOrientationValues.LANDSCAPE_SECONDARY);
        Assert.assertEquals(
                orientationValueToRotation(ScreenOrientationValues.LANDSCAPE_SECONDARY), rotation);

        // The note in testOrientationChanges about REVERSE_PORTRAIT applies to PORTRAIT_SECONDARY.
    }
}
