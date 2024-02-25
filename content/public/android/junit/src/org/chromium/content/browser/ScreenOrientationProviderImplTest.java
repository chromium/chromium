// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.content.pm.ActivityInfo;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link ScreenOrientationProviderImpl } */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class ScreenOrientationProviderImplTest {
    /**
     * Tests that when screen orientation requests are delayed that newer requests overwrite older
     * requests for a given activity.
     */
    @Test
    public void testDelayRequests() {
        final Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        ActivityWindowAndroid window = buildMockWindowForActivity(activity);

        // Last orientation lock request should take precedence.
        ScreenOrientationProviderImpl instance = ScreenOrientationProviderImpl.getInstance();
        instance.delayOrientationRequests(window);
        instance.lockOrientation(window, (byte) ScreenOrientationLockType.PORTRAIT_PRIMARY);
        instance.lockOrientation(window, (byte) ScreenOrientationLockType.LANDSCAPE_PRIMARY);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, activity.getRequestedOrientation());

        instance.runDelayedOrientationRequests(window);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, activity.getRequestedOrientation());

        // Lock then unlock screen orientation while requests are delayed.
        instance.delayOrientationRequests(window);
        instance.lockOrientation(window, (byte) ScreenOrientationLockType.PORTRAIT_PRIMARY);
        instance.unlockOrientation(window);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, activity.getRequestedOrientation());

        instance.runDelayedOrientationRequests(window);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_USER, activity.getRequestedOrientation());
    }

    /**
     * Tests that whether screen orientation requests are delayed can be toggled for each activity
     * independently.
     */
    @Test
    public void testDelayRequestsAppliesOnlyToActivity() {
        final Activity activity1 = Robolectric.buildActivity(Activity.class).create().get();
        ActivityWindowAndroid window1 = buildMockWindowForActivity(activity1);
        final Activity activity2 = Robolectric.buildActivity(Activity.class).create().get();
        ActivityWindowAndroid window2 = buildMockWindowForActivity(activity2);

        ScreenOrientationProviderImpl instance = ScreenOrientationProviderImpl.getInstance();
        instance.delayOrientationRequests(window1);
        instance.lockOrientation(window1, (byte) ScreenOrientationLockType.PORTRAIT_PRIMARY);
        instance.lockOrientation(window2, (byte) ScreenOrientationLockType.LANDSCAPE_PRIMARY);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, activity1.getRequestedOrientation());
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, activity2.getRequestedOrientation());

        instance.runDelayedOrientationRequests(window1);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, activity1.getRequestedOrientation());
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, activity2.getRequestedOrientation());
    }

    /**
     * Tests that removing the screen orientation request delay is a no-op if there are no pending
     * screen orientation requests.
     */
    @Test
    public void testRemoveDelayNoPendingRequests() {
        final Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        ActivityWindowAndroid window = buildMockWindowForActivity(activity);

        ScreenOrientationProviderImpl instance = ScreenOrientationProviderImpl.getInstance();
        instance.delayOrientationRequests(window);
        instance.runDelayedOrientationRequests(window);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, activity.getRequestedOrientation());
    }

    private ActivityWindowAndroid buildMockWindowForActivity(Activity activity) {
        ActivityWindowAndroid window = Mockito.mock(ActivityWindowAndroid.class);
        Mockito.when(window.getActivity()).thenReturn(new WeakReference<>(activity));
        return window;
    }
}
