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
import android.media.Image.Plane;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.view.WindowManager;

import androidx.activity.result.ActivityResult;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicReference;

/** See comments on `DesktopCapturerAndroid`. */
@NullMarked
@JNINamespace("content")
public class ScreenCapture implements ImageHandler.Delegate {
    private static final String TAG = "ScreenCapture";

    private static class PickState {
        final WebContents mWebContents;
        final ActivityResult mActivityResult;

        PickState(WebContents webContents, ActivityResult activityResult) {
            mWebContents = webContents;
            mActivityResult = activityResult;
        }
    }

    /** Holds the state required for screen capture. */
    static class CaptureState {
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

    // Starting a MediaProjection session involves plumbing the results from the content picker,
    // which is done via ActivityResult. This class does not handle how that is achieved, but
    // requires this state to begin the session.
    private static final AtomicReference<PickState> sNextPickState = new AtomicReference<>(null);

    // Starting a MediaProjection session requires a foreground service to be running. This class
    // does not handle how that is achieved, but `sLatch` provides a way for this class to wait
    // until that foreground service is running.
    private static final ConditionVariable sLatch = new ConditionVariable(false);

    // Holds the pointer to the C++ side object. The C++ side has the ownership of the Java side.
    private long mNativeDesktopCapturerAndroid;

    private final Handler mHandler;

    private @Nullable MediaProjection mMediaProjection;

    private @Nullable VirtualDisplay mVirtualDisplay;

    // We need to store multiple ImageHandlers here because the native side may still be using
    // Images from the previous ImageHandler when we do a resize. Once all the Images are no longer
    // in use (the release callback is called), then we can close the ImageHandler.
    private final ArrayList<ImageHandler> mImageHandlerQueue = new ArrayList<>();

    private @Nullable WebContents mWebContents;

    private ScreenCapture(long nativeDesktopCapturerAndroid) {
        mNativeDesktopCapturerAndroid = nativeDesktopCapturerAndroid;
        mHandler = new Handler(assumeNonNull(Looper.myLooper()));
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
        final PickState pickState = sNextPickState.getAndSet(null);
        assert pickState != null;

        final ActivityResult activityResult = pickState.mActivityResult;
        assert activityResult.getData() != null;

        // We need to wait for the foreground service to start before trying to use the
        // MediaProjection API. It's okay to block here since we are on the desktop capturer thread.
        sLatch.block();

        mWebContents = pickState.mWebContents;
        // TODO(crbug.com/352187279): Update the context if the WebContents is reparented.
        final Context context = maybeGetContext();
        if (context == null) return false;

        final var manager =
                (MediaProjectionManager) context.getSystemService(Context.MEDIA_PROJECTION_SERVICE);
        if (manager == null) return false;

        mMediaProjection =
                manager.getMediaProjection(
                        activityResult.getResultCode(), activityResult.getData());
        if (mMediaProjection == null) return false;

        mMediaProjection.registerCallback(new MediaProjectionCallback(), mHandler);

        final var windowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        final var windowMetrics = windowManager.getMaximumWindowMetrics();
        final Rect bounds = windowMetrics.getBounds();

        createListener(
                new CaptureState(
                        bounds.width(),
                        bounds.height(),
                        context.getResources().getConfiguration().densityDpi,
                        PixelFormat.RGBA_8888));

        return true;
    }

    @CalledByNative
    void destroy() {
        mNativeDesktopCapturerAndroid = 0;

        if (mMediaProjection != null) {
            mMediaProjection.stop();
            mMediaProjection = null;
        }
        // Iterate backwards since closeNow() will cause the `ImageHandler` to be removed from
        // the queue.
        for (int i = mImageHandlerQueue.size() - 1; i >= 0; i--) {
            // Note that we can only immediately close the ImageHandler if we are guaranteed the
            // native side will not try to access Images from it again. We are guaranteed this
            // here since the native side is being destroyed.
            mImageHandlerQueue.get(i).closeNow();
        }
        assert mImageHandlerQueue.isEmpty();
        mImageHandlerQueue.clear();

        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
    }

    private class MediaProjectionCallback extends MediaProjection.Callback {
        @Override
        public void onCapturedContentResize(int width, int height) {
            if (mNativeDesktopCapturerAndroid == 0) return;

            final Context context = maybeGetContext();
            if (context == null) return;

            final int format =
                    mImageHandlerQueue.get(mImageHandlerQueue.size() - 1).getCaptureState().format;
            recreateListener(
                    new CaptureState(
                            width,
                            height,
                            context.getResources().getConfiguration().densityDpi,
                            format));
        }

        @Override
        public void onCapturedContentVisibilityChanged(boolean isVisible) {
            // If the captured content is not visible we don't do anything special.
        }

        @Override
        public void onStop() {
            if (mNativeDesktopCapturerAndroid == 0) return;
            mMediaProjection = null;
            ScreenCaptureJni.get().onStop(mNativeDesktopCapturerAndroid);
        }
    }

    private ImageHandler createImageHandler(CaptureState captureState) {
        final var imageHandler = new ImageHandler(captureState, this, mHandler);
        mImageHandlerQueue.add(imageHandler);
        return imageHandler;
    }

    private void closeImageHandlersBefore(ImageHandler imageHandler) {
        final int idx = mImageHandlerQueue.indexOf(imageHandler);
        assert idx != -1;
        // Iterate backwards since `close` can cause `ImageHandler` to be removed from the queue.
        for (int i = idx - 1; i >= 0; i--) {
            mImageHandlerQueue.get(i).close();
        }
    }

    private void recreateListener(CaptureState captureState) {
        final var imageHandler = createImageHandler(captureState);

        assert mVirtualDisplay != null;
        mVirtualDisplay.resize(captureState.width, captureState.height, captureState.dpi);
        mVirtualDisplay.setSurface(imageHandler.getSurface());
    }

    private void createListener(CaptureState captureState) {
        final var imageHandler = createImageHandler(captureState);

        assert mMediaProjection != null;
        assert mVirtualDisplay == null;
        mVirtualDisplay =
                mMediaProjection.createVirtualDisplay(
                        "ScreenCapture",
                        captureState.width,
                        captureState.height,
                        captureState.dpi,
                        DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                        imageHandler.getSurface(),
                        null,
                        null);
    }

    // ImageHandler.Delegate
    @Override
    public void onRgbaFrameAvailable(
            ImageHandler imageHandler,
            Runnable releaseCb,
            long timestampNs,
            Plane plane,
            Rect cropRect) {
        // If the native side was destroyed, then exit without calling JNI methods.
        if (mNativeDesktopCapturerAndroid == 0) return;

        // Don't close old `ImageHandler`s until we have a Image written to the new
        // Image handler. This is to make sure that if the OS is still trying to write
        // to an older Surface from `ImageReader` it can.
        closeImageHandlersBefore(imageHandler);

        ScreenCaptureJni.get()
                .onRgbaFrameAvailable(
                        mNativeDesktopCapturerAndroid,
                        releaseCb,
                        timestampNs,
                        plane.getBuffer(),
                        plane.getPixelStride(),
                        plane.getRowStride(),
                        cropRect.left,
                        cropRect.top,
                        cropRect.right,
                        cropRect.bottom);
    }

    @Override
    public void onClose(ImageHandler imageHandler) {
        final boolean removed = mImageHandlerQueue.remove(imageHandler);
        assert removed;
    }

    @Override
    public void recreateImageHandler(CaptureState captureState) {
        // If the native side was destroyed, then don't bother recreating the ImageHandler.
        if (mNativeDesktopCapturerAndroid == 0) return;

        recreateListener(captureState);
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
                int right,
                int bottom);

        void onStop(long nativeDesktopCapturerAndroid);
    }
}
