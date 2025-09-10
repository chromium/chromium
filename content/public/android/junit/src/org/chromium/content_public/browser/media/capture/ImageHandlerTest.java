// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.media.Image;
import android.media.Image.Plane;
import android.media.ImageReader;
import android.os.Handler;
import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.media.capture.ScreenCapture.CaptureState;

/** Unit tests for {@link ImageHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImageHandlerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TEST_WIDTH = 100;
    private static final int TEST_HEIGHT = 200;
    private static final int TEST_DPI = 300;
    private static final Rect TEST_CROP_RECT = new Rect(0, 0, TEST_WIDTH, TEST_HEIGHT);
    private static final long TEST_TIMESTAMP = 123456789L;

    @Mock private ImageHandler.Delegate mDelegate;
    @Mock private ImageReader mImageReader;

    private ImageHandler mImageHandler;
    private Handler mHandler;

    @Before
    public void setUp() {
        mHandler = new Handler(Looper.getMainLooper());
        mImageHandler =
                new ImageHandler(
                        new CaptureState(TEST_WIDTH, TEST_HEIGHT, TEST_DPI, PixelFormat.RGBA_8888),
                        mDelegate,
                        mHandler,
                        mImageReader);

        when(mImageReader.getMaxImages()).thenReturn(2);
    }

    private Image createMockImage() {
        return createMockImage(TEST_TIMESTAMP);
    }

    private Image createMockImage(long timestamp) {
        final Image image = mock(Image.class);
        final Plane plane = mock(Plane.class);
        when(image.getPlanes()).thenReturn(new Plane[] {plane});
        when(image.getFormat()).thenReturn(PixelFormat.RGBA_8888);
        when(image.getCropRect()).thenReturn(TEST_CROP_RECT);
        when(image.getTimestamp()).thenReturn(timestamp);
        return image;
    }

    // New helper method to create a mock I420 image.
    private Image createMockI420Image(long timestamp) {
        final Image image = mock(Image.class);
        // YUV_420_888 requires 3 planes.
        when(image.getPlanes())
                .thenReturn(new Plane[] {mock(Plane.class), mock(Plane.class), mock(Plane.class)});
        when(image.getFormat()).thenReturn(ImageFormat.YUV_420_888);
        when(image.getCropRect()).thenReturn(TEST_CROP_RECT);
        when(image.getTimestamp()).thenReturn(timestamp);
        return image;
    }

    private void onImageAvailable(Image image) {
        // Return `image` once, then nothing.
        when(mImageReader.acquireLatestImage()).thenReturn(image, (Image) null);
        mImageHandler.onImageAvailable(mImageReader);
    }

    @Test
    public void testImageAcquireAndRelease() throws Exception {
        final Image image = createMockImage();
        final Plane plane = image.getPlanes()[0];

        // Acquire an `Image`.
        onImageAvailable(image);

        final ArgumentCaptor<Runnable> releaseCb = ArgumentCaptor.forClass(Runnable.class);
        verify(mDelegate)
                .onRgbaFrameAvailable(
                        eq(mImageHandler),
                        releaseCb.capture(),
                        eq(TEST_TIMESTAMP),
                        eq(plane),
                        eq(TEST_CROP_RECT));
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());

        // Run the release callback.
        releaseCb.getValue().run();

        // Image should be closed now.
        verify(image).close();
        assertEquals(0, mImageHandler.getAcquiredImageCountForTesting());
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testCloseWithNoAcquiredImagesClosesImmediately() {
        mImageHandler.close();
        verify(mImageReader).close();
        verify(mDelegate).onClose(eq(mImageHandler));
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testCloseWithAcquiredImagesClosesOnRelease() throws Exception {
        final Image image = createMockImage();
        final Plane plane = image.getPlanes()[0];

        onImageAvailable(image);

        final ArgumentCaptor<Runnable> releaseCb = ArgumentCaptor.forClass(Runnable.class);
        verify(mDelegate)
                .onRgbaFrameAvailable(
                        eq(mImageHandler),
                        releaseCb.capture(),
                        eq(TEST_TIMESTAMP),
                        eq(plane),
                        eq(TEST_CROP_RECT));
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());

        // Close while the image is still acquired.
        mImageHandler.close();

        verify(mImageReader, never()).close();
        assertTrue(mImageHandler.isClosingForTesting());
        verify(mDelegate, never()).onClose(any());

        // Release the image, we should close now.
        releaseCb.getValue().run();

        verify(image).close();
        verify(mImageReader).close();
        verify(mDelegate).onClose(eq(mImageHandler));
        assertEquals(0, mImageHandler.getAcquiredImageCountForTesting());
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testCloseWithMultipleImagesClosesOnAllReleased() throws Exception {
        final Image image1 = createMockImage(/* timestamp= */ 1L);
        final Image image2 = createMockImage(/* timestamp= */ 2L);
        final Plane plane1 = image1.getPlanes()[0];
        final Plane plane2 = image2.getPlanes()[0];
        final ArgumentCaptor<Runnable> releaseCb1 = ArgumentCaptor.forClass(Runnable.class);
        final ArgumentCaptor<Runnable> releaseCb2 = ArgumentCaptor.forClass(Runnable.class);

        // Acquire two images.
        onImageAvailable(image1);
        verify(mDelegate)
                .onRgbaFrameAvailable(
                        eq(mImageHandler), releaseCb1.capture(), eq(1L), eq(plane1), any());
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());

        onImageAvailable(image2);
        verify(mDelegate)
                .onRgbaFrameAvailable(
                        eq(mImageHandler), releaseCb2.capture(), eq(2L), eq(plane2), any());
        assertEquals(2, mImageHandler.getAcquiredImageCountForTesting());

        // Try to close the ImageHandler. We should not close until all have been released.
        mImageHandler.close();

        assertTrue(mImageHandler.isClosingForTesting());
        verify(mImageReader, never()).close();
        verify(mDelegate, never()).onClose(any());

        // Release the second image.
        releaseCb2.getValue().run();

        verify(image2).close();
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());
        assertTrue(mImageHandler.isClosingForTesting());
        verify(mImageReader, never()).close();
        verify(mDelegate, never()).onClose(any());

        // Check we didn't try to acquire more images on release (we are closing).
        verify(mImageReader, times(2)).acquireLatestImage();

        // Release the first image.
        releaseCb1.getValue().run();

        // We should close the ImageHandler now.
        verify(image1).close();
        assertEquals(0, mImageHandler.getAcquiredImageCountForTesting());
        verify(mImageReader).close();
        verify(mDelegate).onClose(eq(mImageHandler));
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testOnImageAvailableWhileClosingCanStillAcquire() throws Exception {
        final Image image1 = createMockImage(/* timestamp= */ 1L);
        final Image image2 = createMockImage(/* timestamp= */ 2L);
        final Plane plane1 = image1.getPlanes()[0];
        final Plane plane2 = image2.getPlanes()[0];
        final ArgumentCaptor<Runnable> releaseCb1 = ArgumentCaptor.forClass(Runnable.class);
        final ArgumentCaptor<Runnable> releaseCb2 = ArgumentCaptor.forClass(Runnable.class);

        // Acquire one image.
        onImageAvailable(image1);
        verify(mDelegate)
                .onRgbaFrameAvailable(
                        eq(mImageHandler), releaseCb1.capture(), eq(1L), eq(plane1), any());
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());

        // Close the ImageHandler.
        mImageHandler.close();
        assertTrue(mImageHandler.isClosingForTesting());
        verify(mImageReader, never()).close();

        // We should be able to acquire the next image. This is because we want to keep providing
        // frames if the producer keeps producing them. When we recreate the ImageHandler we may get
        // a few extra frames from the producer until it switches over to using the new Surface.
        onImageAvailable(image2);
        verify(mDelegate)
                .onRgbaFrameAvailable(
                        eq(mImageHandler), releaseCb2.capture(), eq(2L), eq(plane2), any());
        assertEquals(2, mImageHandler.getAcquiredImageCountForTesting());

        // We should still be closing.
        assertTrue(mImageHandler.isClosingForTesting());
        verify(mImageReader, never()).close();

        // Release the first image.
        releaseCb1.getValue().run();
        verify(image1).close();
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());
        verify(mImageReader, never()).close();

        // Release the second image and the ImageHandler should close.
        releaseCb2.getValue().run();
        verify(image2).close();
        verify(mImageReader).close();
        verify(mDelegate).onClose(eq(mImageHandler));
        assertEquals(0, mImageHandler.getAcquiredImageCountForTesting());
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testCloseNowClosesImmediately() throws Exception {
        final Image image = createMockImage();
        final Plane plane = image.getPlanes()[0];

        // Acquire an `Image`.
        onImageAvailable(image);

        final ArgumentCaptor<Runnable> releaseCb = ArgumentCaptor.forClass(Runnable.class);
        verify(mDelegate)
                .onRgbaFrameAvailable(
                        eq(mImageHandler),
                        releaseCb.capture(),
                        eq(TEST_TIMESTAMP),
                        eq(plane),
                        eq(TEST_CROP_RECT));
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());

        // Closing should close the underlying `ImageReader`, even though we have not released
        // `image`.
        mImageHandler.closeNow();

        verify(mImageReader).close();
        verify(mDelegate).onClose(eq(mImageHandler));
        assertEquals(0, mImageHandler.getAcquiredImageCountForTesting());
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testAcquireRecreatesWithYuv() throws Exception {
        // `UnsupportedOperationException` indicates the image format is wrong.
        when(mImageReader.acquireLatestImage()).thenThrow(new UnsupportedOperationException());

        // Tell `ImageHandler` it has an image.
        mImageHandler.onImageAvailable(mImageReader);

        // We should re-create in YUV now.
        final ArgumentCaptor<CaptureState> stateCaptor =
                ArgumentCaptor.forClass(CaptureState.class);
        verify(mDelegate).recreateImageHandler(stateCaptor.capture());

        // New state should be the same except in YUV.
        assertEquals(
                new CaptureState(TEST_WIDTH, TEST_HEIGHT, TEST_DPI, ImageFormat.YUV_420_888),
                stateCaptor.getValue());

        verify(mDelegate, never()).onRgbaFrameAvailable(any(), any(), anyLong(), any(), any());
        verifyNoMoreInteractions(mDelegate);
    }

    @Test(expected = IllegalStateException.class)
    public void testAcquireThrowsWhenYuvFails() throws Exception {
        // Create an `ImageHandler` already on YUV.
        mImageHandler =
                new ImageHandler(
                        new CaptureState(
                                TEST_WIDTH, TEST_HEIGHT, TEST_DPI, ImageFormat.YUV_420_888),
                        mDelegate,
                        mHandler,
                        mImageReader);
        when(mImageReader.acquireLatestImage()).thenThrow(new UnsupportedOperationException());

        // This should now throw because there's no further fallback.
        mImageHandler.onImageAvailable(mImageReader);
    }

    @Test
    public void testAcquireAtMaxImagesDoesNotAcquire() throws Exception {
        final Image image1 = createMockImage(/* timestamp= */ 1L);
        final Image image2 = createMockImage(/* timestamp= */ 2L);
        final Image image3 = createMockImage(/* timestamp= */ 3L);

        // Acquire two images.
        onImageAvailable(image1);
        onImageAvailable(image2);
        assertEquals(2, mImageHandler.getAcquiredImageCountForTesting());

        // Try to acquire a third. It should skip asking the `ImageReader` for the new image.
        onImageAvailable(image3);
        assertEquals(2, mImageHandler.getAcquiredImageCountForTesting());
        verify(mImageReader, times(2)).acquireLatestImage();
        verify(mDelegate, times(2)).onRgbaFrameAvailable(any(), any(), anyLong(), any(), any());
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testFailedAcquireDoesNotCrash() throws Exception {
        when(mImageReader.acquireLatestImage()).thenThrow(new IllegalStateException());

        mImageHandler.onImageAvailable(mImageReader);

        assertEquals(0, mImageHandler.getAcquiredImageCountForTesting());
        verify(mDelegate, never()).onRgbaFrameAvailable(any(), any(), anyLong(), any(), any());
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testReleaseRetriesAcquire() throws Exception {
        final Image image1 = createMockImage(/* timestamp= */ 1L);
        final Image image2 = createMockImage(/* timestamp= */ 2L);
        final Plane plane1 = image1.getPlanes()[0];
        final Plane plane2 = image2.getPlanes()[0];

        when(mImageReader.acquireLatestImage()).thenReturn(image1, image2);

        // Acquire the first image.
        mImageHandler.onImageAvailable(mImageReader);

        final ArgumentCaptor<Runnable> releaseCb = ArgumentCaptor.forClass(Runnable.class);
        verify(mDelegate)
                .onRgbaFrameAvailable(
                        eq(mImageHandler),
                        releaseCb.capture(),
                        eq(1L),
                        eq(plane1),
                        eq(TEST_CROP_RECT));
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());

        // Release. This should trigger an attempt to acquire another image.
        releaseCb.getValue().run();

        verify(image1).close();
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());
        verify(mDelegate)
                .onRgbaFrameAvailable(
                        eq(mImageHandler), any(), eq(2L), eq(plane2), eq(TEST_CROP_RECT));
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testI420ImageAcquireAndRelease() throws Exception {
        // Set up ImageHandler specifically for YUV format.
        mImageHandler =
                new ImageHandler(
                        new CaptureState(
                                TEST_WIDTH, TEST_HEIGHT, TEST_DPI, ImageFormat.YUV_420_888),
                        mDelegate,
                        mHandler,
                        mImageReader);

        final Image image = createMockI420Image(TEST_TIMESTAMP);
        final Plane[] planes = image.getPlanes();

        onImageAvailable(image);

        final ArgumentCaptor<Runnable> releaseCb = ArgumentCaptor.forClass(Runnable.class);
        verify(mDelegate)
                .onI420FrameAvailable(
                        eq(mImageHandler),
                        releaseCb.capture(),
                        eq(TEST_TIMESTAMP),
                        eq(planes),
                        eq(TEST_CROP_RECT));
        verify(mDelegate, never()).onRgbaFrameAvailable(any(), any(), anyLong(), any(), any());
        assertEquals(1, mImageHandler.getAcquiredImageCountForTesting());

        releaseCb.getValue().run();

        verify(image).close();
        assertEquals(0, mImageHandler.getAcquiredImageCountForTesting());
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testRgbaHandlerProcessesYuvImage() {
        final Image image = createMockI420Image(TEST_TIMESTAMP);
        final Plane[] planes = image.getPlanes();

        onImageAvailable(image);

        verify(mDelegate)
                .onI420FrameAvailable(
                        eq(mImageHandler),
                        any(Runnable.class),
                        eq(TEST_TIMESTAMP),
                        eq(planes),
                        eq(TEST_CROP_RECT));
        verifyNoMoreInteractions(mDelegate);
    }

    @Test(expected = AssertionError.class)
    public void testInvalidYuvImageThrowsAssertionError() throws Exception {
        final Image image = mock(Image.class);
        final Plane plane = mock(Plane.class);
        when(image.getPlanes()).thenReturn(new Plane[] {plane});
        when(image.getFormat()).thenReturn(ImageFormat.YUV_420_888);

        // This should throw an AssertionError because the plane count is not 3.
        onImageAvailable(image);
    }

    @Test(expected = IllegalStateException.class)
    public void testUnexpectedImageFormatThrowsIllegalStateException() {
        final Image image = mock(Image.class);
        when(image.getFormat()).thenReturn(ImageFormat.JPEG);
        onImageAvailable(image);
    }
}
