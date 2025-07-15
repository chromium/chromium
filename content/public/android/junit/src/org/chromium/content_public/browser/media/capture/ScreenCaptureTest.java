// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.Surface;
import android.view.WindowMetrics;

import androidx.activity.result.ActivityResult;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.ArrayList;

/** Unit tests for {@link ScreenCapture}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE,
        shadows = {
            ScreenCaptureTest.ShadowMediaProjectionManager.class,
            ScreenCaptureTest.ShadowWindowManagerImpl.class
        })
public class ScreenCaptureTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TEST_WIDTH_DP = 100;
    private static final int TEST_HEIGHT_DP = 200;
    private static final int TEST_DPI = 300;
    private static final int NEW_WIDTH_PX = 400;
    private static final int NEW_HEIGHT_PX = 500;
    private static final long NATIVE_POINTER = 1L;

    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private MediaProjection mMediaProjection;
    @Mock private VirtualDisplay mVirtualDisplay;
    @Mock private ScreenCapture.Natives mNativeMock;

    private static class ImageHandlerState {
        public final ImageHandler imageHandler;
        public final ImageReader imageReader;

        ImageHandlerState(ImageHandler imageHandler, ImageReader imageReader) {
            this.imageHandler = imageHandler;
            this.imageReader = imageReader;
        }
    }

    private final ArrayList<ImageHandlerState> mImageHandlerStates = new ArrayList<>();
    private ScreenCapture mScreenCapture;
    private Context mContext;

    @Implements(MediaProjectionManager.class)
    public static class ShadowMediaProjectionManager {
        private static MediaProjection sMediaProjection;

        static void setMediaProjection(MediaProjection projection) {
            sMediaProjection = projection;
        }

        @Implementation
        protected MediaProjection getMediaProjection(int resultCode, Intent resultData) {
            return resultCode == Activity.RESULT_OK ? sMediaProjection : null;
        }

        @Resetter
        public static void reset() {
            sMediaProjection = null;
        }
    }

    @Implements(className = "android.view.WindowManagerImpl")
    public static class ShadowWindowManagerImpl {
        private static WindowMetrics sWindowMetrics;

        static void setWindowBounds(Rect bounds) {
            sWindowMetrics = mock(WindowMetrics.class);
            when(sWindowMetrics.getBounds()).thenReturn(bounds);
        }

        @Implementation
        protected WindowMetrics getMaximumWindowMetrics() {
            return sWindowMetrics;
        }

        @Implementation
        protected WindowMetrics getCurrentWindowMetrics() {
            return sWindowMetrics;
        }

        @Resetter
        public static void reset() {
            sWindowMetrics = null;
        }
    }

    private void updateConfiguration(Context context, int widthDp, int heightDp, int dpi) {
        final Resources resources = context.getResources();
        final DisplayMetrics displayMetrics = resources.getDisplayMetrics();
        displayMetrics.widthPixels = widthDp * dpi / DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.heightPixels = heightDp * dpi / DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.densityDpi = dpi;
        displayMetrics.density = (float) dpi / DisplayMetrics.DENSITY_DEFAULT;

        final Configuration configuration = resources.getConfiguration();
        configuration.screenWidthDp = widthDp;
        configuration.screenHeightDp = heightDp;
        configuration.densityDpi = dpi;

        ShadowWindowManagerImpl.setWindowBounds(
                new Rect(0, 0, displayMetrics.widthPixels, displayMetrics.heightPixels));
        resources.updateConfiguration(configuration, displayMetrics);
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        mScreenCapture = spy(new ScreenCapture(NATIVE_POINTER, this::createTestImageHandler));
        ScreenCaptureJni.setInstanceForTesting(mNativeMock);

        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mContext));

        updateConfiguration(mContext, TEST_WIDTH_DP, TEST_HEIGHT_DP, TEST_DPI);

        ShadowMediaProjectionManager.setMediaProjection(mMediaProjection);

        when(mMediaProjection.createVirtualDisplay(
                        any(), anyInt(), anyInt(), anyInt(), anyInt(), any(), any(), any()))
                .thenReturn(mVirtualDisplay);
    }

    @After
    public void tearDown() {
        ScreenCapture.resetStaticStateForTesting();
        ScreenCaptureJni.setInstanceForTesting(null);
    }

    private ImageHandler createTestImageHandler(
            ScreenCapture.CaptureState captureState,
            ImageHandler.Delegate delegate,
            Handler handler) {
        final var imageReader = mock(ImageReader.class);
        final var surface = mock(Surface.class);
        when(imageReader.getSurface()).thenReturn(surface);

        final var imageHandler = new ImageHandler(captureState, delegate, handler, imageReader);
        final var state = new ImageHandlerState(imageHandler, imageReader);
        mImageHandlerStates.add(state);

        return imageHandler;
    }

    private MediaProjection.Callback getMediaProjectionCallback() {
        final ArgumentCaptor<MediaProjection.Callback> cb =
                ArgumentCaptor.forClass(MediaProjection.Callback.class);
        verify(mMediaProjection).registerCallback(cb.capture(), any(Handler.class));
        final MediaProjection.Callback callback = cb.getValue();
        assertNotNull("MediaProjection.Callback was not registered.", callback);
        return callback;
    }

    @Test
    public void testStartCapture() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);

        assertTrue(mScreenCapture.startCapture());
        assertEquals(1, mImageHandlerStates.size());

        final Surface surface = mImageHandlerStates.get(0).imageHandler.getSurface();
        verify(mMediaProjection)
                .createVirtualDisplay(
                        eq("ScreenCapture"),
                        eq(TEST_WIDTH_DP * TEST_DPI / DisplayMetrics.DENSITY_DEFAULT),
                        eq(TEST_HEIGHT_DP * TEST_DPI / DisplayMetrics.DENSITY_DEFAULT),
                        eq(TEST_DPI),
                        eq(DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR),
                        eq(surface),
                        eq(null),
                        eq(null));
    }

    @Test
    public void testStartCaptureThrowsIfNoPickResult() {
        ScreenCapture.onForegroundServiceRunning(true);
        assertThrows(AssertionError.class, () -> mScreenCapture.startCapture());
        verify(mMediaProjection, never())
                .createVirtualDisplay(
                        any(), anyInt(), anyInt(), anyInt(), anyInt(), any(), any(), any());
    }

    @Test
    public void testStartCaptureFailsIfResultCanceled() {
        final ActivityResult canceledResult =
                new ActivityResult(Activity.RESULT_CANCELED, new Intent());
        ScreenCapture.onPick(mWebContents, canceledResult);
        ScreenCapture.onForegroundServiceRunning(true);

        assertFalse(mScreenCapture.startCapture());
        assertTrue(mImageHandlerStates.isEmpty());
        verify(mMediaProjection, never())
                .createVirtualDisplay(
                        any(), anyInt(), anyInt(), anyInt(), anyInt(), any(), any(), any());
    }

    @Test
    public void testStartCaptureFailsIfNoMediaProjection() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ShadowMediaProjectionManager.setMediaProjection(null);
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);

        assertFalse(mScreenCapture.startCapture());
        assertTrue(mImageHandlerStates.isEmpty());
    }

    @Test
    public void testOnPickThrowsIfCalledTwice() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        assertThrows(
                AssertionError.class, () -> ScreenCapture.onPick(mWebContents, activityResult));
    }

    @Test
    public void testDestroy() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());
        mScreenCapture.destroy();

        assertEquals(1, mImageHandlerStates.size());
        verify(mImageHandlerStates.get(0).imageReader).close();
        verify(mMediaProjection).stop();
        verify(mVirtualDisplay).release();
    }

    @Test
    public void testMediaProjectionOnStopAndDestroy() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        final MediaProjection.Callback callback = getMediaProjectionCallback();
        callback.onStop();
        verify(mNativeMock, times(1)).onStop(NATIVE_POINTER);

        mScreenCapture.destroy();
        assertEquals(1, mImageHandlerStates.size());
        verify(mImageHandlerStates.get(0).imageReader).close();
        verify(mVirtualDisplay).release();
    }

    @Test
    public void testMediaProjectionOnStopDoesNotStopNative() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        MediaProjection.Callback callback = getMediaProjectionCallback();
        mScreenCapture.destroy();
        callback.onStop();

        verify(mNativeMock, never()).onStop(NATIVE_POINTER);
    }

    @Test
    public void testDestroyWithoutStartCapture() {
        mScreenCapture.destroy();
        assertTrue(mImageHandlerStates.isEmpty());
        verifyNoMoreInteractions(mMediaProjection, mVirtualDisplay);
    }

    @Test
    public void destroyClosesAllImageHandlers() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        MediaProjection.Callback callback = getMediaProjectionCallback();
        callback.onCapturedContentResize(NEW_WIDTH_PX, NEW_HEIGHT_PX);
        assertEquals(2, mImageHandlerStates.size());

        mScreenCapture.destroy();

        verify(mImageHandlerStates.get(0).imageReader).close();
        verify(mImageHandlerStates.get(1).imageReader).close();
    }

    @Test
    public void testOnCapturedContentResizeDoesNothingAfterDestroy() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        MediaProjection.Callback callback = getMediaProjectionCallback();
        mScreenCapture.destroy();
        callback.onCapturedContentResize(NEW_WIDTH_PX, NEW_HEIGHT_PX);

        verify(mVirtualDisplay, never()).resize(anyInt(), anyInt(), anyInt());
        verify(mVirtualDisplay, never()).setSurface(any());
    }

    @Test
    public void testOnCapturedContentResizeRecreatesHandlerAndUpdatesVirtualDisplay() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        assertEquals(1, mImageHandlerStates.size());
        final MediaProjection.Callback callback = getMediaProjectionCallback();
        callback.onCapturedContentResize(NEW_WIDTH_PX, NEW_HEIGHT_PX);

        assertEquals(2, mImageHandlerStates.size());
        final var handler = mImageHandlerStates.get(1).imageHandler;
        assertEquals(NEW_WIDTH_PX, handler.getCaptureState().width);
        assertEquals(NEW_HEIGHT_PX, handler.getCaptureState().height);

        verify(mVirtualDisplay).resize(NEW_WIDTH_PX, NEW_HEIGHT_PX, TEST_DPI);
        verify(mVirtualDisplay).setSurface(handler.getSurface());
    }

    @Test
    public void testOnCapturedContentResizeDoesNothingIfSizeIsUnchanged() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        assertEquals(1, mImageHandlerStates.size());
        final MediaProjection.Callback callback = getMediaProjectionCallback();

        final int widthPx = TEST_WIDTH_DP * TEST_DPI / DisplayMetrics.DENSITY_DEFAULT;
        final int heightPx = TEST_HEIGHT_DP * TEST_DPI / DisplayMetrics.DENSITY_DEFAULT;
        callback.onCapturedContentResize(widthPx, heightPx);

        assertEquals(1, mImageHandlerStates.size());
        verify(mVirtualDisplay, never()).resize(anyInt(), anyInt(), anyInt());
        verify(mVirtualDisplay, never()).setSurface(any());
    }

    @Test
    public void testOnCapturedContentResizeDoesNothingIfContextIsMissing() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        final MediaProjection.Callback callback = getMediaProjectionCallback();
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(null));

        callback.onCapturedContentResize(NEW_WIDTH_PX, NEW_HEIGHT_PX);

        assertEquals(1, mImageHandlerStates.size());
        verify(mVirtualDisplay, never()).resize(anyInt(), anyInt(), anyInt());
        verify(mVirtualDisplay, never()).setSurface(any());
    }
}
