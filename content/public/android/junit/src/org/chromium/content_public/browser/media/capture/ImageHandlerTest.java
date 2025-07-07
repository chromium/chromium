// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

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

    @Before
    public void setUp() {
        mImageHandler =
                new ImageHandler(
                        new CaptureState(TEST_WIDTH, TEST_HEIGHT, TEST_DPI, PixelFormat.RGBA_8888),
                        mDelegate,
                        new Handler(Looper.getMainLooper()),
                        mImageReader);

        when(mImageReader.getMaxImages()).thenReturn(2);
    }

    private Image createMockImage() {
        final Image image = mock(Image.class);
        final Plane plane = mock(Plane.class);
        when(image.getPlanes()).thenReturn(new Plane[] {plane});
        when(image.getFormat()).thenReturn(PixelFormat.RGBA_8888);
        when(image.getCropRect()).thenReturn(TEST_CROP_RECT);
        when(image.getTimestamp()).thenReturn(TEST_TIMESTAMP);
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
}
