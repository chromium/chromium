// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import android.view.Surface;
import android.view.WindowManager;

import androidx.activity.result.ActivityResult;
import androidx.annotation.GuardedBy;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicReference;

/** See comments on `DesktopCapturerAndroid`. */
@NullMarked
@JNINamespace("content")
public class ScreenCapture {
    private static final String TAG = "ScreenCapture";

    private static class PickState {
        final WebContents mWebContents;
        final ActivityResult mActivityResult;

        PickState(WebContents webContents, ActivityResult activityResult) {
            mWebContents = webContents;
            mActivityResult = activityResult;
        }
    }

    private static class CaptureState {
        public final int width;
        public final int height;
        public final int dpi;
        public final int format;

        CaptureState(int width, int height, int dpi, int format) {
            this.width = width;
            this.height = height;
            this.dpi = dpi;
            this.format = format;
        }
    }

    private class ImageHandler implements ImageReader.OnImageAvailableListener {
        @GuardedBy("mBackgroundLock")
        final ImageReader mImageReader;

        @GuardedBy("mBackgroundLock")
        int mAcquiredImageCount;

        ImageHandler(CaptureState captureState) {
            mImageReader =
                    ImageReader.newInstance(
                            captureState.width,
                            captureState.height,
                            captureState.format,
                            /* maxImages= */ 2);
            mImageReader.setOnImageAvailableListener(this, mBackgroundHandler);
        }

        Surface getSurface() {
            synchronized (mBackgroundLock) {
                return mImageReader.getSurface();
            }
        }

        void close() {
            synchronized (mBackgroundLock) {
                mImageReader.close();
                mAcquiredImageCount = 0;
            }
        }

        @GuardedBy("mBackgroundLock")
        private @Nullable Image maybeAcquireImage(ImageReader reader) {
            assert mBackgroundThread.getLooper().isCurrentThread();
            assert reader == mImageReader;
            // If we have acquired the maximum number of images `acquireLatestImage`
            // will print warning level logspam, so avoid
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

            synchronized (mBackgroundLock) {
                assert reader == mImageReader;

                // If we recreate the ImageReader, we may get an old release here. The image will
                // already have been closed since the ImageReader is closed, but it's safe to call
                // close
                // again here.
                image.close();
                mAcquiredImageCount--;

                // Now that we closed an image, we may be able to acquire a new image.
                onImageAvailable(reader);
            }
        }

        @Override
        public void onImageAvailable(ImageReader reader) {
            assert mBackgroundThread.getLooper().isCurrentThread();

            synchronized (mBackgroundLock) {
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
                                        image.getTimestamp(),
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

    // Starting a MediaProjection session involves plumbing the results from the content picker,
    // which is done via ActivityResult. This class does not handle how that is achieved, but
    // requires this state to begin the session.
    private static final AtomicReference<PickState> sNextPickState = new AtomicReference<>(null);

    // Starting a MediaProjection session requires a foreground service to be running. This class
    // does not handle how that is achieved, but `sLatch` provides a way for this class to wait
    // until that foreground service is running.
    private static final ConditionVariable sLatch = new ConditionVariable(false);

    // Lock to protect access to fields that are modified mainly on the background thread. This is
    // also used to prevent destruction of the native side while JNI methods are running. See
    // comments on `DesktopCapturerAndroid` for more information.
    private final Object mBackgroundLock = new Object();
    private long mNativeDesktopCapturerAndroid;

    private final HandlerThread mBackgroundThread = new HandlerThread("ScreenCapture");
    private @Nullable Handler mBackgroundHandler;
    private final Thread mCaptureThread;

    @GuardedBy("mBackgroundLock")
    private @Nullable MediaProjection mMediaProjection;

    @GuardedBy("mBackgroundLock")
    private @Nullable VirtualDisplay mVirtualDisplay;

    @GuardedBy("mBackgroundLock")
    private @Nullable ImageHandler mImageHandler;

    @GuardedBy("mBackgroundLock")
    private @Nullable WebContents mWebContents;

    private ScreenCapture(long nativeDesktopCapturerAndroid) {
        mNativeDesktopCapturerAndroid = nativeDesktopCapturerAndroid;
        mCaptureThread = Thread.currentThread();
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
     * @param webContents The {@link WebContents} initiating the capture.
     * @param activityResult The {@link ActivityResult} from the MediaProjection API.
     */
    public static void onPick(WebContents webContents, ActivityResult activityResult) {
        final PickState oldPickState =
                sNextPickState.getAndSet(new PickState(webContents, activityResult));
        assert oldPickState == null;
    }

    @GuardedBy("mBackgroundLock")
    private @Nullable Context maybeGetContext() {
        final WindowAndroid window = assumeNonNull(mWebContents).getTopLevelNativeWindow();
        if (window == null) return null;
        return window.getContext().get();
    }

    @CalledByNative
    static ScreenCapture create(long nativeDesktopCapturerAndroid) {
        return new ScreenCapture(nativeDesktopCapturerAndroid);
    }

    @CalledByNative
    boolean startCapture() {
        assert mCaptureThread.getId() == Thread.currentThread().getId();

        final PickState pickState = sNextPickState.getAndSet(null);
        assert pickState != null;

        final ActivityResult activityResult = pickState.mActivityResult;
        assert activityResult.getData() != null;

        // We need to wait for the foreground service to start before trying to use the
        // MediaProjection API. It's okay to block here since we are on the desktop capturer thread.
        sLatch.block();

        synchronized (mBackgroundLock) {
            mWebContents = pickState.mWebContents;
            // TODO(crbug.com/352187279): Update the context if the WebContents is reparented.
            final Context context = maybeGetContext();
            if (context == null) return false;

            var manager =
                    (MediaProjectionManager)
                            context.getSystemService(Context.MEDIA_PROJECTION_SERVICE);
            if (manager == null) return false;

            mMediaProjection =
                    manager.getMediaProjection(
                            activityResult.getResultCode(), activityResult.getData());
            if (mMediaProjection == null) return false;

            mBackgroundThread.start();
            mBackgroundHandler = new Handler(mBackgroundThread.getLooper());

            // We must use a background thread and `Handler` here since the current thread
            // (DesktopCapturer thread) does not have a `Looper` set up.
            mMediaProjection.registerCallback(new MediaProjectionCallback(), mBackgroundHandler);

            var windowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
            var windowMetrics = windowManager.getMaximumWindowMetrics();
            final Rect bounds = windowMetrics.getBounds();

            recreateListener(
                    new CaptureState(
                            bounds.width(),
                            bounds.height(),
                            context.getResources().getConfiguration().densityDpi,
                            PixelFormat.RGBA_8888));
        }

        return true;
    }

    @CalledByNative
    void destroy() {
        assert mCaptureThread.getId() == Thread.currentThread().getId();

        if (mBackgroundThread != null) {
            // End the background thread before taking `mBackgroundLock` since messages run
            // on this thread may need to take `mBackgroundLock` and could deadlock
            // otherwise.
            mBackgroundThread.quit();
        }
        synchronized (mBackgroundLock) {
            if (mMediaProjection != null) {
                mMediaProjection.stop();
                mMediaProjection = null;
            }
            destroyListener();
            mNativeDesktopCapturerAndroid = 0;
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
            synchronized (mBackgroundLock) {
                if (mNativeDesktopCapturerAndroid == 0) return;
                mMediaProjection = null;
                ScreenCaptureJni.get().onStop(mNativeDesktopCapturerAndroid);
            }
        }
    }

    @GuardedBy("this.mBackgroundLock")
    private void destroyListener() {
        assert mCaptureThread.getId() == Thread.currentThread().getId();
        // Note that we can only close the ImageHandler if we are guaranteed the native side will
        // not try to access Images from it again.
        if (mImageHandler != null) {
            mImageHandler.close();
            mImageHandler = null;
        }
        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
    }

    @GuardedBy("mBackgroundLock")
    private void recreateListener(CaptureState captureState) {
        assert mCaptureThread.getId() == Thread.currentThread().getId();
        destroyListener();
        mImageHandler = new ImageHandler(captureState);

        assert mMediaProjection != null;
        mVirtualDisplay =
                mMediaProjection.createVirtualDisplay(
                        "ScreenCapture",
                        captureState.width,
                        captureState.height,
                        captureState.dpi,
                        DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                        mImageHandler.getSurface(),
                        null,
                        null);
    }

    @NativeMethods
    interface Natives {
        void onRgbaFrameAvailable(
                long nativeDesktopCapturerAndroid,
                Runnable releaseCb,
                long timestampNs,
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
