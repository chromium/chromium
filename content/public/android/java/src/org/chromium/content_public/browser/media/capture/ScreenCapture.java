// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import android.content.Context;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.Image.Plane;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.WindowManager;

import androidx.activity.result.ActivityResult;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicReference;

/** See comments on `DesktopCapturerAndroid`. */
@NullMarked
@JNINamespace("content")
public class ScreenCapture {
    private static final String TAG = "ScreenCapture";

    // Starting a MediaProjection session involves plumbing the results from the content picker,
    // which is done via ActivityResult. This class does not handle how that is achieved, but
    // requires the ActivityResult to begin the session.
    private static final AtomicReference<ActivityResult> sNextResult = new AtomicReference(null);

    // Starting a MediaProjection session requires a foreground service to be running. This class
    // does not handle how that is achieved, but `sLatch` provides a way for this class to wait
    // until that foreground service is running.
    private static final ConditionVariable sLatch = new ConditionVariable(false);

    // Since we run processing in a background thread, we need to prevent the native side from
    // being destructed sometimes. See comments on `DesktopCapturerAndroid` for more information.
    private final Object mNativeDestructionLock = new Object();
    private long mNativeDesktopCapturerAndroid;

    private final HandlerThread mBackgroundThread = new HandlerThread("ScreenCapture");
    private @Nullable Handler mBackgroundHandler;
    private @Nullable MediaProjection mMediaProjection;

    // While capture is running these references should only be modified on the background thread.
    private @Nullable VirtualDisplay mVirtualDisplay;
    private @Nullable ImageReader mImageReader;
    private int mAcquiredImageCount;

    private ScreenCapture(long nativeDesktopCapturerAndroid) {
        mNativeDesktopCapturerAndroid = nativeDesktopCapturerAndroid;
    }

    public static void onForegroundServiceRunning(boolean running) {
        if (running) {
            sLatch.open();
        } else {
            sLatch.close();
        }
    }

    /**
     * Called before attempting to start a ScreenCapture session.
     *
     * <p>The {@link ActivityResult} is consumed by a subsequent call to {@link #startCapture()}.
     *
     * @param nextResult The {@link ActivityResult} from the MediaProjection API.
     */
    public static void onPick(ActivityResult nextResult) {
        var oldResult = sNextResult.getAndSet(nextResult);
        assert oldResult == null;
    }

    @CalledByNative
    static ScreenCapture create(long nativeDesktopCapturerAndroid) {
        return new ScreenCapture(nativeDesktopCapturerAndroid);
    }

    @CalledByNative
    boolean startCapture() {
        var nextResult = sNextResult.getAndSet(null);
        assert nextResult != null;
        assert nextResult.getData() != null;

        // We need to wait for the foreground service to start before trying to use the
        // MediaProjection API. It's okay to block here since we are on the desktop capturer thread.
        sLatch.block();

        // TODO(crbug.com/352187279): Use the specific activity context for the captured target
        // here.
        final Context context = ContextUtils.getApplicationContext();

        var manager =
                (MediaProjectionManager) context.getSystemService(Context.MEDIA_PROJECTION_SERVICE);
        if (manager == null) return false;

        mMediaProjection =
                manager.getMediaProjection(nextResult.getResultCode(), nextResult.getData());
        if (mMediaProjection == null) return false;

        mBackgroundThread.start();
        mBackgroundHandler = new Handler(mBackgroundThread.getLooper());

        // Take the MediaProjection callbacks on the background thread as we may call JNI methods or
        // update references accessed by the ImageReader handling code.
        mMediaProjection.registerCallback(new MediaProjectionCallback(), mBackgroundHandler);

        var windowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        var windowMetrics = windowManager.getMaximumWindowMetrics();
        final Rect bounds = windowMetrics.getBounds();
        int width = bounds.width();
        int height = bounds.height();
        int dpi = context.getResources().getConfiguration().densityDpi;

        recreateListener(width, height, PixelFormat.RGBA_8888, dpi);
        return true;
    }

    @CalledByNative
    void destroy() {
        if (mBackgroundThread != null) {
            // End the background thread before taking `mNativeDestructionLock` since messages run
            // on this thread may need to take `mNativeDestructionLock` and could deadlock
            // otherwise.
            mBackgroundThread.quit();
        }
        synchronized (mNativeDestructionLock) {
            if (mMediaProjection != null) {
                mMediaProjection.stop();
                mMediaProjection = null;
            }
            destroyListener();
            mNativeDesktopCapturerAndroid = 0;
        }
    }

    private class ImageListener implements ImageReader.OnImageAvailableListener {
        private @Nullable Image maybeAcquireImage(ImageReader reader) {
            assert mBackgroundThread.getLooper().isCurrentThread();
            // If we have acquired the maximum number of images `acquireLatestImage`
            // will print warning level logspam, so avoid this.
            if (mAcquiredImageCount >= reader.getMaxImages()) return null;

            try {
                Image image = reader.acquireLatestImage();
                if (image != null) mAcquiredImageCount++;
                return image;
            } catch (IllegalStateException ex) {
                // This happens if we have acquired the maximum number of images without closing
                // them. We will eventually close the images so this is not an error condition.
            } catch (UnsupportedOperationException ex) {
                // TODO(crbug.com/352187279): This can happen if the `PixelFormat` does not match.
                // We should recreate the `ImageReader` with the correct `PixqelFormat` in this
                // case.
                throw ex;
            }
            return null;
        }

        private void releaseImage(ImageReader reader, Image image) {
            assert mBackgroundThread.getLooper().isCurrentThread();
            // If we recreate the ImageReader, we may get an old release here. The image will
            // already have been closed since the ImageReader is closed, but it's safe to call close
            // again here.
            image.close();

            // `mAcquiredImageCount` is only for the current ImageReader, so don't incorrectly
            // decrement it for an old ImageReader.
            if (reader == mImageReader) mAcquiredImageCount--;

            // Now that we closed an image, we may be able to acquire a new image.
            onImageAvailable(reader);
        }

        @Override
        public void onImageAvailable(ImageReader reader) {
            assert mBackgroundThread.getLooper().isCurrentThread();

            // If we recreate the ImageReader we may get a call with the old reader here. Skip this
            // case.
            if (reader != mImageReader) return;

            // Prevent native destruction until JNI methods are done.
            synchronized (mNativeDestructionLock) {
                // If the native side was destroyed, then exit without calling JNI methods.
                if (mNativeDesktopCapturerAndroid == 0) return;

                // Note that we can't use `acquireLatestImage` here because we can't close older
                // images until the C++ side is finished using them.
                final Image image = maybeAcquireImage(reader);

                // If we have not yet closed images, this may return null. We need to retry
                // after closing an image.
                if (image == null) return;

                switch (image.getFormat()) {
                    case PixelFormat.RGBA_8888:
                        assert image.getPlanes().length == 1;
                        final Plane plane = image.getPlanes()[0];
                        final Rect cropRect = image.getCropRect();
                        ScreenCaptureJni.get()
                                .onRgbaFrameAvailable(
                                        mNativeDesktopCapturerAndroid,
                                        () -> {
                                            assert mBackgroundHandler != null;
                                            mBackgroundHandler.post(
                                                    () -> releaseImage(reader, image));
                                        },
                                        plane.getBuffer(),
                                        plane.getPixelStride(),
                                        plane.getRowStride(),
                                        cropRect.left,
                                        cropRect.top,
                                        cropRect.right,
                                        cropRect.bottom);
                        break;
                    default:
                        throw new IllegalStateException(
                                "Unexpected image format: " + image.getFormat());
                }
            }
        }
    }

    private class MediaProjectionCallback extends MediaProjection.Callback {
        @Override
        public void onCapturedContentResize(int width, int height) {
            // TODO(crbug.com/352187279): Handle content resize and rotate.
            assert mBackgroundThread.getLooper().isCurrentThread();
        }

        @Override
        public void onCapturedContentVisibilityChanged(boolean isVisible) {
            // If the captured content is not visible we don't do anything special.
            assert mBackgroundThread.getLooper().isCurrentThread();
        }

        @Override
        public void onStop() {
            assert mBackgroundThread.getLooper().isCurrentThread();
            synchronized (mNativeDestructionLock) {
                if (mNativeDesktopCapturerAndroid == 0) return;
                mMediaProjection = null;
                ScreenCaptureJni.get().onStop(mNativeDesktopCapturerAndroid);
            }
        }
    }

    private void destroyListener() {
        if (mImageReader != null) {
            mImageReader.close();
            mImageReader = null;
            mAcquiredImageCount = 0;
        }
        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
    }

    private void recreateListener(int width, int height, int format, int dpi) {
        destroyListener();
        mImageReader = ImageReader.newInstance(width, height, format, /* maxImages= */ 2);
        mImageReader.setOnImageAvailableListener(new ImageListener(), mBackgroundHandler);

        assert mMediaProjection != null;
        mVirtualDisplay =
                mMediaProjection.createVirtualDisplay(
                        "ScreenCapture",
                        width,
                        height,
                        dpi,
                        DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                        mImageReader.getSurface(),
                        /* callback= */ null,
                        /* handler= */ null);
    }

    @NativeMethods
    interface Natives {
        void onRgbaFrameAvailable(
                long nativeDesktopCapturerAndroid,
                Runnable releaseCb,
                ByteBuffer buf,
                int pixelStride,
                int rowStride,
                int left,
                int top,
                int width,
                int height);

        void onStop(long nativeDesktopCapturerAndroid);
    }
}
