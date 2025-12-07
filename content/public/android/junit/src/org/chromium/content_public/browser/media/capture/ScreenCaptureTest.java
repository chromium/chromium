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
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.Image.Plane;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
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
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Queue;

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

    @Mock private MockWebContents mWebContents;
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
        mContext = spy(ApplicationProvider.getApplicationContext());

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
        when(imageReader.getMaxImages()).thenReturn(2);
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

    private Image createMockImage() {
        final Image image = mock(Image.class);
        final Plane plane = mock(Plane.class);
        when(image.getFormat()).thenReturn(PixelFormat.RGBA_8888);
        when(image.getPlanes()).thenReturn(new Plane[] {plane});
        when(image.getCropRect()).thenReturn(new Rect());
        return image;
    }

    private Image createMockYuvImage(
            long timestamp,
            Rect cropRect,
            int yPixelStride,
            int yRowStride,
            int uPixelStride,
            int uRowStride,
            int vPixelStride,
            int vRowStride) {
        final Image image = mock(Image.class);
        when(image.getFormat()).thenReturn(ImageFormat.YUV_420_888);
        when(image.getTimestamp()).thenReturn(timestamp);
        when(image.getCropRect()).thenReturn(cropRect);

        // Use real ByteBuffers we can't mock ByteBuffer.
        final ByteBuffer yBuffer = ByteBuffer.allocate(1);
        final ByteBuffer uBuffer = ByteBuffer.allocate(1);
        final ByteBuffer vBuffer = ByteBuffer.allocate(1);

        final Plane yPlane = mock(Plane.class);
        when(yPlane.getBuffer()).thenReturn(yBuffer);
        when(yPlane.getPixelStride()).thenReturn(yPixelStride);
        when(yPlane.getRowStride()).thenReturn(yRowStride);

        final Plane uPlane = mock(Plane.class);
        when(uPlane.getBuffer()).thenReturn(uBuffer);
        when(uPlane.getPixelStride()).thenReturn(uPixelStride);
        when(uPlane.getRowStride()).thenReturn(uRowStride);

        final Plane vPlane = mock(Plane.class);
        when(vPlane.getBuffer()).thenReturn(vBuffer);
        when(vPlane.getPixelStride()).thenReturn(vPixelStride);
        when(vPlane.getRowStride()).thenReturn(vRowStride);

        when(image.getPlanes()).thenReturn(new Plane[] {yPlane, uPlane, vPlane});
        return image;
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
        verify(mNativeMock).onStop(NATIVE_POINTER);

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

        final ArgumentCaptor<WebContentsObserver> observerCaptor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(mWebContents).addObserver(observerCaptor.capture());
        final WebContentsObserver observer = observerCaptor.getValue();
        assertNotNull(observer);

        // Simulate a window change.
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(null));
        observer.onTopLevelNativeWindowChanged(mWindowAndroid);
        shadowOf(Looper.myLooper()).idle();

        final MediaProjection.Callback callback = getMediaProjectionCallback();
        callback.onCapturedContentResize(NEW_WIDTH_PX, NEW_HEIGHT_PX);

        assertEquals(1, mImageHandlerStates.size());
        verify(mVirtualDisplay, never()).resize(anyInt(), anyInt(), anyInt());
        verify(mVirtualDisplay, never()).setSurface(any());
    }

    @Test
    public void testImageHandlerRecreatesOnFormatError() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        assertEquals(1, mImageHandlerStates.size());
        final var state0 = mImageHandlerStates.get(0);
        assertEquals(PixelFormat.RGBA_8888, state0.imageHandler.getCaptureState().format);

        // Simulate producer error causing UnsupportedOperationException on frame acquire.
        when(state0.imageReader.acquireLatestImage())
                .thenThrow(new UnsupportedOperationException());
        state0.imageHandler.onImageAvailable(state0.imageReader);

        // ImageHandler should be recreated.
        assertEquals(2, mImageHandlerStates.size());
        final var state1 = mImageHandlerStates.get(1);

        // Should be recreated in YUV.
        assertEquals(ImageFormat.YUV_420_888, state1.imageHandler.getCaptureState().format);
        final Surface surface1 = state1.imageHandler.getSurface();
        verify(mVirtualDisplay).setSurface(surface1);
    }

    @Test
    public void testImageHandlerFormatFallbackPersistsAcrossResizes() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());
        MediaProjection.Callback callback = getMediaProjectionCallback();

        final ImageReader reader0 = mImageHandlerStates.get(0).imageReader;
        final ImageHandler handler0 = mImageHandlerStates.get(0).imageHandler;
        assertEquals(PixelFormat.RGBA_8888, handler0.getCaptureState().format);

        // Trigger a fallback to YUV.
        when(reader0.acquireLatestImage()).thenThrow(new UnsupportedOperationException());
        handler0.onImageAvailable(reader0);
        assertEquals(2, mImageHandlerStates.size());
        assertEquals(
                ImageFormat.YUV_420_888,
                mImageHandlerStates.get(1).imageHandler.getCaptureState().format);

        // Trigger a resize.
        callback.onCapturedContentResize(NEW_WIDTH_PX, NEW_HEIGHT_PX);
        assertEquals(3, mImageHandlerStates.size());

        // Verify the third handler is still using YUV.
        assertEquals(
                ImageFormat.YUV_420_888,
                mImageHandlerStates.get(2).imageHandler.getCaptureState().format);
    }

    @Test
    public void testOnStopAfterDestroy() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());
        MediaProjection.Callback callback = getMediaProjectionCallback();

        mScreenCapture.destroy();
        verify(mMediaProjection).stop();
        verify(mNativeMock, never()).onStop(NATIVE_POINTER);

        // Trigger onStop after destroy and check we don't try to call the native side.
        callback.onStop();
        verify(mNativeMock, never()).onStop(NATIVE_POINTER);
    }

    @Test
    public void testImageHandlerGracefulCloseDuringResize() {
        final Queue<Runnable> pendingReleases = new ArrayDeque<>();
        doAnswer(invocation -> pendingReleases.add(invocation.getArgument(1)))
                .when(mNativeMock)
                .onRgbaFrameAvailable(
                        anyLong(),
                        any(Runnable.class),
                        anyLong(),
                        any(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt());

        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());
        final MediaProjection.Callback callback = getMediaProjectionCallback();

        // Frame arrives for handler0.
        final ImageHandler handler0 = mImageHandlerStates.get(0).imageHandler;
        final ImageReader reader0 = mImageHandlerStates.get(0).imageReader;
        final Image image0 = createMockImage();
        when(reader0.acquireLatestImage()).thenReturn(image0).thenReturn(null);
        handler0.onImageAvailable(reader0);
        assertEquals(1, pendingReleases.size());
        final Runnable releaseCb0 = pendingReleases.remove();
        assertEquals(1, handler0.getAcquiredImageCountForTesting());
        assertFalse(handler0.isClosingForTesting());

        // Resize which creates a new handler.
        callback.onCapturedContentResize(NEW_WIDTH_PX, NEW_HEIGHT_PX);
        assertEquals(2, mImageHandlerStates.size());
        final ImageHandler handler1 = mImageHandlerStates.get(1).imageHandler;
        final ImageReader reader1 = mImageHandlerStates.get(1).imageReader;
        final Image image1 = createMockImage();
        assertFalse(handler0.isClosingForTesting());
        verify(reader0, never()).close();

        // Send a frame for handler1, which should close the previous handler once image0 is
        // released.
        when(reader1.acquireLatestImage()).thenReturn(image1).thenReturn(null);
        handler1.onImageAvailable(reader1);
        assertEquals(1, pendingReleases.size());
        final Runnable releaseCb1 = pendingReleases.remove();
        assertTrue(handler0.isClosingForTesting());
        verify(reader0, never()).close();
        assertEquals(1, handler0.getAcquiredImageCountForTesting());

        // Release image0, which should close handler0.
        releaseCb0.run();
        assertEquals(0, handler0.getAcquiredImageCountForTesting());
        verify(image0).close();
        verify(reader0).close();
        verify(mScreenCapture).onClose(handler0);

        // Release image1, which should not close handler1.
        releaseCb1.run();
        assertFalse(handler1.isClosingForTesting());
        assertEquals(0, handler1.getAcquiredImageCountForTesting());
        verify(image1).close();
        verify(reader1, never()).close();
    }

    @Test
    public void testImageHandlerMaxImagesLimit() {
        final Queue<Runnable> pendingReleases = new ArrayDeque<>();
        doAnswer(invocation -> pendingReleases.add(invocation.getArgument(1)))
                .when(mNativeMock)
                .onRgbaFrameAvailable(
                        anyLong(),
                        any(Runnable.class),
                        anyLong(),
                        any(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt());

        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        final ImageHandler handler = mImageHandlerStates.get(0).imageHandler;
        final ImageReader reader = mImageHandlerStates.get(0).imageReader;

        // Push a frame.
        final Image image0 = createMockImage();
        when(reader.acquireLatestImage()).thenReturn(image0).thenReturn(null);
        handler.onImageAvailable(reader);
        final Runnable releaseCb0 = pendingReleases.remove();
        assertEquals(1, handler.getAcquiredImageCountForTesting());
        verify(reader).acquireLatestImage();

        // Push second frame.
        final Image image1 = createMockImage();
        when(reader.acquireLatestImage()).thenReturn(image1).thenReturn(null);
        handler.onImageAvailable(reader);
        final Runnable releaseCb1 = pendingReleases.remove();
        assertEquals(2, handler.getAcquiredImageCountForTesting());
        verify(reader, times(2)).acquireLatestImage();

        // Try to push a third frame, but it exceeds the maximum number of images, meaning that
        // we can't actually acquire it.
        handler.onImageAvailable(reader);
        assertEquals(2, handler.getAcquiredImageCountForTesting());
        assertTrue(pendingReleases.isEmpty());
        // We should not try to acquire it (will fail anyway).
        verify(reader, times(2)).acquireLatestImage();

        // Release the first frame, which should let us acquire the third one.
        final Image image2 = createMockImage();
        when(reader.acquireLatestImage()).thenReturn(image2).thenReturn(null);
        releaseCb0.run();
        verify(image0).close();
        assertEquals(2, handler.getAcquiredImageCountForTesting());
        assertEquals(1, pendingReleases.size());
        final Runnable releaseCb2 = pendingReleases.remove();
        verify(reader, times(3)).acquireLatestImage();

        // Clean up remaining frames.
        releaseCb1.run();
        releaseCb2.run();
        verify(image1).close();
        verify(image2).close();
    }

    @Test
    public void testImageHandlerFrameReleaseAfterDestroy() {
        final Queue<Runnable> pendingReleases = new ArrayDeque<>();
        doAnswer(invocation -> pendingReleases.add(invocation.getArgument(1)))
                .when(mNativeMock)
                .onRgbaFrameAvailable(
                        anyLong(),
                        any(Runnable.class),
                        anyLong(),
                        any(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt());

        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());

        final ImageHandler handler = mImageHandlerStates.get(0).imageHandler;
        final ImageReader reader = mImageHandlerStates.get(0).imageReader;

        // Push two frames to fill up the ImageReader.
        final Image image0 = createMockImage();
        when(reader.acquireLatestImage()).thenReturn(image0).thenReturn(null);
        handler.onImageAvailable(reader);
        final Runnable releaseCb0 = pendingReleases.remove();

        final Image image1 = createMockImage();
        when(reader.acquireLatestImage()).thenReturn(image1).thenReturn(null);
        handler.onImageAvailable(reader);
        pendingReleases.remove();
        assertEquals(2, handler.getAcquiredImageCountForTesting());
        verify(reader, times(2)).acquireLatestImage();

        // Push a third frame, which will be pending since the ImageReader is full.
        handler.onImageAvailable(reader);
        verify(reader, times(2)).acquireLatestImage();

        // Destroy ScreenCapture. This should close the ImageReader.
        mScreenCapture.destroy();
        verify(reader).close();
        assertEquals(0, handler.getAcquiredImageCountForTesting());

        // The reader is already closed, so throw.
        when(reader.acquireLatestImage()).thenThrow(new IllegalStateException());

        // The release callback would normally signal to acquire a new frame, but everything
        // is already closed. This should be able to run without crashing.
        releaseCb0.run();
    }

    @Test
    public void testMultipleOnCapturedContentResizeBeforeFrame() {
        final Queue<Runnable> pendingReleases = new ArrayDeque<>();
        doAnswer(invocation -> pendingReleases.add(invocation.getArgument(1)))
                .when(mNativeMock)
                .onRgbaFrameAvailable(
                        anyLong(),
                        any(Runnable.class),
                        anyLong(),
                        any(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt());

        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        assertTrue(mScreenCapture.startCapture());
        final MediaProjection.Callback callback = getMediaProjectionCallback();

        // Resize twice before sending any frames.
        callback.onCapturedContentResize(NEW_WIDTH_PX, NEW_HEIGHT_PX);
        callback.onCapturedContentResize(NEW_WIDTH_PX + 1, NEW_HEIGHT_PX + 1);
        assertEquals(3, mImageHandlerStates.size());

        // Push a frame for the newest handler. This should trigger the closing of the older
        // handlers. Since they have no acquired images, they should close immediately.
        final ImageHandler handler2 = mImageHandlerStates.get(2).imageHandler;
        final ImageReader reader2 = mImageHandlerStates.get(2).imageReader;
        final Image image = createMockImage();
        when(reader2.acquireLatestImage()).thenReturn(image).thenReturn(null);
        handler2.onImageAvailable(reader2);
        assertEquals(1, pendingReleases.size());

        // Verify the old handlers were closed immediately.
        final ImageHandler handler0 = mImageHandlerStates.get(0).imageHandler;
        final ImageReader reader0 = mImageHandlerStates.get(0).imageReader;
        verify(reader0).close();
        verify(mScreenCapture).onClose(handler0);

        final ImageHandler handler1 = mImageHandlerStates.get(1).imageHandler;
        final ImageReader reader1 = mImageHandlerStates.get(1).imageReader;
        verify(reader1).close();
        verify(mScreenCapture).onClose(handler1);

        // The current handler should not be closed.
        verify(reader2, never()).close();

        // Clean up the acquired frame.
        pendingReleases.remove().run();
        verify(image).close();
    }

    @Test
    public void testOnTopLevelNativeWindowChanged() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);

        assertTrue(mScreenCapture.startCapture());
        assertEquals(1, mImageHandlerStates.size());

        final ArgumentCaptor<WebContentsObserver> observerCaptor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(mWebContents).addObserver(observerCaptor.capture());
        final WebContentsObserver observer = observerCaptor.getValue();
        assertNotNull(observer);

        // Simulate a DPI change.
        final int newDpi = TEST_DPI + 100;
        updateConfiguration(mContext, TEST_WIDTH_DP, TEST_HEIGHT_DP, newDpi);

        // Simulate a window change.
        observer.onTopLevelNativeWindowChanged(mWindowAndroid);
        shadowOf(Looper.myLooper()).idle();

        // Verify a new ImageHandler was created with the new DPI.
        assertEquals(2, mImageHandlerStates.size());
        final var handler = mImageHandlerStates.get(1).imageHandler;
        assertEquals(newDpi, handler.getCaptureState().dpi);

        // Verify VirtualDisplay was updated. The width/height in pixels should not change.
        final int widthPx = mImageHandlerStates.get(0).imageHandler.getCaptureState().width;
        final int heightPx = mImageHandlerStates.get(0).imageHandler.getCaptureState().height;
        verify(mVirtualDisplay).resize(widthPx, heightPx, newDpi);
        verify(mVirtualDisplay).setSurface(handler.getSurface());
    }

    @Test
    public void testOnConfigurationChanged() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);

        assertTrue(mScreenCapture.startCapture());
        assertEquals(1, mImageHandlerStates.size());

        final ArgumentCaptor<ComponentCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(ComponentCallbacks.class);
        verify(mContext).registerComponentCallbacks(callbackCaptor.capture());
        final ComponentCallbacks callback = callbackCaptor.getValue();
        assertNotNull(callback);

        // Simulate a DPI change.
        final int newDpi = TEST_DPI + 100;
        updateConfiguration(mContext, TEST_WIDTH_DP, TEST_HEIGHT_DP, newDpi);

        // Manually trigger the callback - robolectric doesn't do this for us.
        callback.onConfigurationChanged(mContext.getResources().getConfiguration());
        shadowOf(Looper.myLooper()).idle();

        // Verify a new ImageHandler was created with the new DPI.
        assertEquals(2, mImageHandlerStates.size());
        final var handler = mImageHandlerStates.get(1).imageHandler;
        assertEquals(newDpi, handler.getCaptureState().dpi);

        // Verify VirtualDisplay was updated. The width/height in pixels should not change.
        final int widthPx = mImageHandlerStates.get(0).imageHandler.getCaptureState().width;
        final int heightPx = mImageHandlerStates.get(0).imageHandler.getCaptureState().height;
        verify(mVirtualDisplay).resize(widthPx, heightPx, newDpi);
        verify(mVirtualDisplay).setSurface(handler.getSurface());
    }

    @Test
    public void testOnI420FrameAvailable() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);

        assertTrue(mScreenCapture.startCapture());
        assertEquals(1, mImageHandlerStates.size());

        // Fallback to YUV first.
        final ImageReader reader0 = mImageHandlerStates.get(0).imageReader;
        final ImageHandler handler0 = mImageHandlerStates.get(0).imageHandler;
        when(reader0.acquireLatestImage()).thenThrow(new UnsupportedOperationException());
        handler0.onImageAvailable(reader0);

        // Verify fallback occurred and we have a YUV handler.
        assertEquals(2, mImageHandlerStates.size());
        final ImageHandler handler1 = mImageHandlerStates.get(1).imageHandler;
        final ImageReader reader1 = mImageHandlerStates.get(1).imageReader;
        assertEquals(ImageFormat.YUV_420_888, handler1.getCaptureState().format);

        // Prepare a mock YUV image.
        final int rowStride = 128;
        final Rect cropRect = new Rect(10, 20, 50, 60);
        final long timestamp = 12345L;
        final Image image =
                createMockYuvImage(
                        timestamp,
                        cropRect,
                        /* yPixelStride= */ 1,
                        /* yRowStride= */ rowStride,
                        /* uPixelStride= */ 2,
                        /* uRowStride= */ rowStride,
                        /* vPixelStride= */ 2,
                        /* vRowStride= */ rowStride);
        final Plane[] planes = image.getPlanes();

        when(reader1.acquireLatestImage()).thenReturn(image).thenReturn(null);
        handler1.onImageAvailable(reader1);

        // Verify the native JNI call (onI420FrameAvailable) and its parameters.
        final ByteBuffer buffer0 = planes[0].getBuffer();
        final ByteBuffer buffer1 = planes[1].getBuffer();
        final ByteBuffer buffer2 = planes[2].getBuffer();
        verify(mNativeMock)
                .onI420FrameAvailable(
                        eq(NATIVE_POINTER),
                        any(Runnable.class),
                        eq(timestamp),
                        eq(buffer0),
                        eq(1),
                        eq(rowStride),
                        eq(buffer1),
                        eq(2),
                        eq(rowStride),
                        eq(buffer2),
                        eq(2),
                        eq(rowStride),
                        eq(cropRect.left),
                        eq(cropRect.top),
                        eq(cropRect.right),
                        eq(cropRect.bottom));
        verify(mNativeMock, never())
                .onRgbaFrameAvailable(
                        anyLong(), any(), anyLong(), any(), anyInt(), anyInt(), anyInt(), anyInt(),
                        anyInt(), anyInt());
    }

    @Test
    public void testOnI420FrameAvailableAfterDestroy() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);

        assertTrue(mScreenCapture.startCapture());
        assertEquals(1, mImageHandlerStates.size());

        // Fallback to YUV first.
        final ImageReader reader0 = mImageHandlerStates.get(0).imageReader;
        final ImageHandler handler0 = mImageHandlerStates.get(0).imageHandler;
        when(reader0.acquireLatestImage()).thenThrow(new UnsupportedOperationException());
        handler0.onImageAvailable(reader0);

        // Destroy ScreenCapture.
        final ImageHandler handler1 = mImageHandlerStates.get(1).imageHandler;
        final ImageReader reader1 = mImageHandlerStates.get(1).imageReader;
        mScreenCapture.destroy();

        // Prepare a mock YUV image.
        final int rowStride = 128;
        final Rect cropRect = new Rect(10, 20, 50, 60);
        final long timestamp = 12345L;
        final Image image =
                createMockYuvImage(
                        timestamp,
                        cropRect,
                        /* yPixelStride= */ 1,
                        /* yRowStride= */ rowStride,
                        /* uPixelStride= */ 2,
                        /* uRowStride= */ rowStride,
                        /* vPixelStride= */ 2,
                        /* vRowStride= */ rowStride);

        // Simulate the frame arriving after destroy.
        when(reader1.acquireLatestImage()).thenReturn(image).thenReturn(null);
        handler1.onImageAvailable(reader1);

        // Verify no call to the native side.
        verify(mNativeMock, never())
                .onI420FrameAvailable(
                        anyLong(), any(), anyLong(), any(), anyInt(), anyInt(), any(), anyInt(),
                        anyInt(), any(), anyInt(), anyInt(), anyInt(), anyInt(), anyInt(),
                        anyInt());
    }
}
