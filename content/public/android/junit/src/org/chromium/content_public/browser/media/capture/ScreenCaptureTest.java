// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.view.Surface;

import androidx.activity.result.ActivityResult;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link ScreenCapture}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        sdk = Build.VERSION_CODES.R,
        shadows = {ScreenCaptureTest.ShadowMediaProjectionManager.class})
public class ScreenCaptureTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TEST_WIDTH_DP = 100;
    private static final int TEST_HEIGHT_DP = 200;
    private static final int TEST_DPI = 300;

    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private MediaProjection mMediaProjection;
    @Mock private VirtualDisplay mVirtualDisplay;
    @Mock private Surface mSurface;
    @Mock private ScreenCapture.Natives mNativeMock;

    private ImageReader mImageReader;
    private ScreenCapture mScreenCapture;
    private Context mContext;

    @Implements(MediaProjectionManager.class)
    public static class ShadowMediaProjectionManager {
        private static MediaProjection sMediaProjection;

        public static void setMediaProjection(MediaProjection projection) {
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

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mScreenCapture =
                new ScreenCapture(
                        /* nativeDesktopCapturerAndroid= */ 1, this::createTestImageHandler);

        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mContext));

        RuntimeEnvironment.setQualifiers(
                String.format("w%ddp-h%ddp-%ddpi", TEST_WIDTH_DP, TEST_HEIGHT_DP, TEST_DPI));

        ShadowMediaProjectionManager.setMediaProjection(mMediaProjection);

        when(mMediaProjection.createVirtualDisplay(
                        any(), anyInt(), anyInt(), anyInt(), anyInt(), any(), any(), any()))
                .thenReturn(mVirtualDisplay);
    }

    @After
    public void tearDown() {
        // Clean up static state to avoid interference between tests.
        ScreenCapture.onForegroundServiceRunning(false);
    }

    private ImageHandler createTestImageHandler(
            ScreenCapture.CaptureState captureState,
            ImageHandler.Delegate delegate,
            Handler handler) {
        assertNull(mImageReader);
        mImageReader = mock(ImageReader.class);
        when(mImageReader.getSurface()).thenReturn(mSurface);
        return new ImageHandler(captureState, delegate, handler, mImageReader);
    }

    @Test
    public void testStartCapture() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);

        assertTrue(mScreenCapture.startCapture());
        assertNotNull(mImageReader);

        verify(mMediaProjection)
                .createVirtualDisplay(
                        eq("ScreenCapture"),
                        eq(TEST_WIDTH_DP * TEST_DPI / 160),
                        eq(TEST_HEIGHT_DP * TEST_DPI / 160),
                        eq(TEST_DPI),
                        eq(DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR),
                        eq(mSurface),
                        eq(null),
                        eq(null));
    }

    @Test
    public void testDestroy() {
        final ActivityResult activityResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        ScreenCapture.onPick(mWebContents, activityResult);
        ScreenCapture.onForegroundServiceRunning(true);
        mScreenCapture.startCapture();
        mScreenCapture.destroy();

        assertNotNull(mImageReader);
        verify(mImageReader).close();
        verify(mMediaProjection).stop();
        verify(mVirtualDisplay).release();
    }
}
