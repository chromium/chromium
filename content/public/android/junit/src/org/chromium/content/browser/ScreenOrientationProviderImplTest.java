// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.EventForwarder.TouchSequenceObserver;
import org.chromium.ui.base.ViewAndroidDelegate;

import java.lang.ref.WeakReference;

/** Unit tests for {@link ScreenOrientationProviderImpl } */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ContentFeatures.RESTRICT_INTERACTIONS_IN_SWIPE_REGION_ON_FULLSCREEN)
public final class ScreenOrientationProviderImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    @Mock private ViewGroup mContainerView;
    @Mock private EventForwarder mEventForwarder;

    private Activity mActivity;
    private ActivityWindowAndroid mWindow;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mWindow = buildMockWindowForActivity(mActivity);
        ViewAndroidDelegate viewAndroidDelegate =
                ViewAndroidDelegate.createBasicDelegate(mContainerView);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(viewAndroidDelegate);
        when(mWebContents.getEventForwarder()).thenReturn(mEventForwarder);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
    }

    /**
     * Tests that when screen orientation requests are delayed that newer requests overwrite older
     * requests for a given activity.
     */
    @Test
    public void testDelayRequests() {
        // Last orientation lock request should take precedence.
        ScreenOrientationProviderImpl instance = ScreenOrientationProviderImpl.getInstance();
        instance.delayOrientationRequests(mWindow);
        instance.lockOrientation(mWindow, (byte) ScreenOrientationLockType.PORTRAIT_PRIMARY);
        instance.lockOrientation(mWindow, (byte) ScreenOrientationLockType.LANDSCAPE_PRIMARY);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, mActivity.getRequestedOrientation());

        instance.runDelayedOrientationRequests(mWindow);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivity.getRequestedOrientation());

        // Lock then unlock screen orientation while requests are delayed.
        instance.delayOrientationRequests(mWindow);
        instance.lockOrientation(mWindow, (byte) ScreenOrientationLockType.PORTRAIT_PRIMARY);
        instance.unlockOrientation(mWindow);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivity.getRequestedOrientation());

        instance.runDelayedOrientationRequests(mWindow);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_USER, mActivity.getRequestedOrientation());
    }

    /**
     * Tests that whether screen orientation requests are delayed can be toggled for each activity
     * independently.
     */
    @Test
    public void testDelayRequestsAppliesOnlyToActivity() {
        final Activity activity2 = Robolectric.buildActivity(Activity.class).create().get();
        ActivityWindowAndroid window2 = buildMockWindowForActivity(activity2);

        ScreenOrientationProviderImpl instance = ScreenOrientationProviderImpl.getInstance();
        instance.delayOrientationRequests(mWindow);
        instance.lockOrientation(mWindow, (byte) ScreenOrientationLockType.PORTRAIT_PRIMARY);
        instance.lockOrientation(window2, (byte) ScreenOrientationLockType.LANDSCAPE_PRIMARY);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, mActivity.getRequestedOrientation());
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, activity2.getRequestedOrientation());

        instance.runDelayedOrientationRequests(mWindow);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, mActivity.getRequestedOrientation());
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, activity2.getRequestedOrientation());
    }

    /**
     * Tests that removing the screen orientation request delay is a no-op if there are no pending
     * screen orientation requests.
     */
    @Test
    public void testRemoveDelayNoPendingRequests() {
        ScreenOrientationProviderImpl instance = ScreenOrientationProviderImpl.getInstance();
        instance.delayOrientationRequests(mWindow);
        instance.runDelayedOrientationRequests(mWindow);
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, mActivity.getRequestedOrientation());
    }

    @Test
    public void testLockOrientationInFullscreen_TouchInGestureInsets_DeferredUntilTapRelease() {
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(true);
        when(mEventForwarder.hasTouchOriginatingInGestureInsets(mContainerView)).thenReturn(true);
        when(mEventForwarder.hasCurrentTouchExceededTouchSlop()).thenReturn(false);

        ScreenOrientationProviderImpl instance = ScreenOrientationProviderImpl.getInstance();
        instance.lockOrientationForWebContents(
                mWebContents, (byte) ScreenOrientationLockType.LANDSCAPE_PRIMARY);

        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, mActivity.getRequestedOrientation());
        ArgumentCaptor<TouchSequenceObserver> observerCaptor =
                ArgumentCaptor.forClass(TouchSequenceObserver.class);
        verify(mEventForwarder).addTouchSequenceObserver(observerCaptor.capture());

        observerCaptor.getValue().onTouchSequenceEnded(/* hasExceededTouchSlop= */ false);
        verify(mEventForwarder).removeTouchSequenceObserver(observerCaptor.getValue());
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivity.getRequestedOrientation());
    }

    @Test
    public void testLockOrientationInFullscreen_TouchInGestureInsets_SuppressedOnSwipe() {
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(true);
        when(mEventForwarder.hasTouchOriginatingInGestureInsets(mContainerView)).thenReturn(true);
        when(mEventForwarder.hasCurrentTouchExceededTouchSlop()).thenReturn(false);

        ScreenOrientationProviderImpl instance = ScreenOrientationProviderImpl.getInstance();
        instance.lockOrientationForWebContents(
                mWebContents, (byte) ScreenOrientationLockType.LANDSCAPE_PRIMARY);

        ArgumentCaptor<TouchSequenceObserver> observerCaptor =
                ArgumentCaptor.forClass(TouchSequenceObserver.class);
        verify(mEventForwarder).addTouchSequenceObserver(observerCaptor.capture());

        observerCaptor.getValue().onTouchSequenceEnded(/* hasExceededTouchSlop= */ true);
        verify(mEventForwarder).removeTouchSequenceObserver(observerCaptor.getValue());
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, mActivity.getRequestedOrientation());

        // If touch slop is already exceeded when lockOrientationForWebContents is called, drop
        // immediately without registering an observer.
        Mockito.clearInvocations(mEventForwarder);
        when(mEventForwarder.hasCurrentTouchExceededTouchSlop()).thenReturn(true);
        instance.lockOrientationForWebContents(
                mWebContents, (byte) ScreenOrientationLockType.LANDSCAPE_PRIMARY);
        verify(mEventForwarder, never()).addTouchSequenceObserver(any());
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, mActivity.getRequestedOrientation());
    }

    @Test
    public void testLockOrientationInFullscreen_TouchOutsideGestureInsets_AppliedImmediately() {
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(true);
        when(mEventForwarder.hasTouchOriginatingInGestureInsets(mContainerView)).thenReturn(false);

        ScreenOrientationProviderImpl instance = ScreenOrientationProviderImpl.getInstance();
        instance.lockOrientationForWebContents(
                mWebContents, (byte) ScreenOrientationLockType.LANDSCAPE_PRIMARY);

        verify(mEventForwarder, never()).addTouchSequenceObserver(any());
        Assert.assertEquals(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivity.getRequestedOrientation());
    }

    private ActivityWindowAndroid buildMockWindowForActivity(Activity activity) {
        ActivityWindowAndroid window = Mockito.mock(ActivityWindowAndroid.class);
        Mockito.when(window.getActivity()).thenReturn(new WeakReference<>(activity));
        return window;
    }
}
