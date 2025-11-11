// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image.Plane;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.view.WindowManager;

import androidx.activity.result.ActivityResult;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Objects;
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

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o instanceof CaptureState that) {
                return width == that.width
                        && height == that.height
                        && dpi == that.dpi
                        && format == that.format;
            }
            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(width, height, dpi, format);
        }
    }

    @VisibleForTesting
    interface ImageHandlerFactory {
        ImageHandler create(
                CaptureState captureState, ImageHandler.Delegate delegate, Handler handler);
    }

    // Starting a MediaProjection session involves plumbing the results from the content picker,
    // which is done via ActivityResult. This class does not handle how that is achieved, but
    // requires this state to begin the session.
    private static final AtomicReference<@Nullable PickState> sNextPickState =
            new AtomicReference<>();

    // Starting a MediaProjection session requires a foreground service to be running. This class
    // does not handle how that is achieved, but `sLatch` provides a way for this class to wait
    // until that foreground service is running.
    private static final ConditionVariable sLatch = new ConditionVariable(false);

    // Holds the pointer to the C++ side object. The C++ side has the ownership of the Java side.
    private volatile long mNativeDesktopCapturerAndroid;

    private final Handler mHandler;
    private final ImageHandlerFactory mImageHandlerFactory;

    private @Nullable MediaProjection mMediaProjection;

    private @Nullable VirtualDisplay mVirtualDisplay;

    // We need to store multiple ImageHandlers here because the native side may still be using
    // Images from the previous ImageHandler when we do a resize. Once all the Images are no longer
    // in use (the release callback is called), then we can close the ImageHandler.
    private final ArrayList<ImageHandler> mImageHandlerQueue = new ArrayList<>();

    private @Nullable WebContents mWebContents;
    private volatile @Nullable WebContentsObserver mWebContentsObserver;
    private volatile @Nullable Context mContext;
    private volatile @Nullable ComponentCallbacks mComponentCallbacks;

    private ScreenCapture(long nativeDesktopCapturerAndroid) {
        this(nativeDesktopCapturerAndroid, ImageHandler::new);
    }

    @VisibleForTesting
    ScreenCapture(long nativeDesktopCapturerAndroid, ImageHandlerFactory imageHandlerFactory) {
        mNativeDesktopCapturerAndroid = nativeDesktopCapturerAndroid;
        mHandler = new Handler(assumeNonNull(Looper.myLooper()));
        mImageHandlerFactory = imageHandlerFactory;
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

    static void resetStaticStateForTesting() {
        sLatch.close();
        sNextPickState.set(null);
    }

    /**
     * Gets the `Context` for the `Activity` the given `WebContents` is associated with.
     *
     * <p>This can be called on either the UI thread or the desktop capture thread. If called on the
     * desktop capture thread, callers must only read values from the Context and not perform any
     * modifications. Reads may theoretically return stale values, so callers on the desktop capture
     * thread must be ok with receiving potentially old values.
     */
    private static @Nullable Context maybeGetContext(@Nullable WebContents webContents) {
        if (webContents == null) return null;
        final WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        return window.getContext().get();
    }

    @CalledByNative
    static ScreenCapture create(long nativeDesktopCapturerAndroid) {
        return new ScreenCapture(nativeDesktopCapturerAndroid);
    }

    @RequiresApi(Build.VERSION_CODES.R)
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
        mContext = maybeGetContext(mWebContents);
        if (mContext == null) return false;

        // `WebContentsObserver` modifies `WebContents` by adding an observer, and that observer
        // list is not thread safe to use. So, we do this on the UI thread. We also run this
        // as blocking so we know that we are observing before creating the listener - this
        // guarantees that we won't miss any updates.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWebContentsObserver =
                            new WebContentsObserver(mWebContents) {
                                @Override
                                public void onTopLevelNativeWindowChanged(
                                        @Nullable WindowAndroid windowAndroid) {
                                    updateContext();
                                }
                            };
                    registerComponentCallbacks();
                });

        final var manager =
                (MediaProjectionManager)
                        mContext.getSystemService(Context.MEDIA_PROJECTION_SERVICE);
        if (manager == null) return false;

        mMediaProjection =
                manager.getMediaProjection(
                        activityResult.getResultCode(), activityResult.getData());
        if (mMediaProjection == null) return false;

        mMediaProjection.registerCallback(new MediaProjectionCallback(), mHandler);

        final var windowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        final var windowMetrics = windowManager.getMaximumWindowMetrics();
        final Rect bounds = windowMetrics.getBounds();

        createListener(
                new CaptureState(
                        bounds.width(),
                        bounds.height(),
                        mContext.getResources().getConfiguration().densityDpi,
                        PixelFormat.RGBA_8888));

        return true;
    }

    @CalledByNative
    void destroy() {
        mNativeDesktopCapturerAndroid = 0;

        // No need to block here since `updateContext` and `onConfigurationChanged` will early exit
        // since `mNativeDesktopCapturerAndroid` is now 0.
        ThreadUtils.runOnUiThread(
                () -> {
                    unregisterComponentCallbacks();
                    mContext = null;
                    if (mWebContentsObserver != null) {
                        mWebContentsObserver.observe(null);
                        mWebContentsObserver = null;
                    }
                });

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

    private void updateContext() {
        ThreadUtils.assertOnUiThread();
        unregisterComponentCallbacks();

        mContext = maybeGetContext(mWebContents);
        if (mContext != null) {
            registerComponentCallbacks();
            final Configuration config = mContext.getResources().getConfiguration();
            mHandler.post(() -> onConfigurationChanged(config));
        }
    }

    private void onConfigurationChanged(Configuration config) {
        assert mHandler.getLooper().isCurrentThread();
        if (mNativeDesktopCapturerAndroid == 0) return;
        if (mImageHandlerQueue.isEmpty()) return;

        final CaptureState captureState =
                mImageHandlerQueue.get(mImageHandlerQueue.size() - 1).getCaptureState();
        recreateListener(
                new CaptureState(
                        captureState.width,
                        captureState.height,
                        config.densityDpi,
                        captureState.format));
    }

    private void registerComponentCallbacks() {
        ThreadUtils.assertOnUiThread();
        assert mContext != null;
        mComponentCallbacks =
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration newConfig) {
                        mHandler.post(() -> ScreenCapture.this.onConfigurationChanged(newConfig));
                    }

                    @Override
                    public void onLowMemory() {}
                };
        mContext.registerComponentCallbacks(mComponentCallbacks);
    }

    private void unregisterComponentCallbacks() {
        ThreadUtils.assertOnUiThread();
        if (mComponentCallbacks != null) {
            assert mContext != null;
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
            mComponentCallbacks = null;
        }
    }

    private class MediaProjectionCallback extends MediaProjection.Callback {
        @Override
        public void onCapturedContentResize(int width, int height) {
            if (mNativeDesktopCapturerAndroid == 0) return;
            if (mContext == null) return;

            final int format =
                    mImageHandlerQueue.get(mImageHandlerQueue.size() - 1).getCaptureState().format;
            recreateListener(
                    new CaptureState(
                            width,
                            height,
                            mContext.getResources().getConfiguration().densityDpi,
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
        final var imageHandler = mImageHandlerFactory.create(captureState, this, mHandler);
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
        assert !mImageHandlerQueue.isEmpty();
        // Don't recreate if the state hasn't changed.
        if (mImageHandlerQueue
                .get(mImageHandlerQueue.size() - 1)
                .getCaptureState()
                .equals(captureState)) {
            return;
        }

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
    public void onI420FrameAvailable(
            ImageHandler imageHandler,
            Runnable releaseCb,
            long timestampNs,
            Plane[] planes,
            Rect cropRect) {
        // If the native side was destroyed, then exit without calling JNI methods.
        if (mNativeDesktopCapturerAndroid == 0) return;

        // Don't close old `ImageHandler`s until we have a Image written to the new
        // Image handler. This is to make sure that if the OS is still trying to write
        // to an older Surface from `ImageReader` it can.
        closeImageHandlersBefore(imageHandler);

        ScreenCaptureJni.get()
                .onI420FrameAvailable(
                        mNativeDesktopCapturerAndroid,
                        releaseCb,
                        timestampNs,
                        planes[0].getBuffer(),
                        planes[0].getPixelStride(),
                        planes[0].getRowStride(),
                        planes[1].getBuffer(),
                        planes[1].getPixelStride(),
                        planes[1].getRowStride(),
                        planes[2].getBuffer(),
                        planes[2].getPixelStride(),
                        planes[2].getRowStride(),
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

        void onI420FrameAvailable(
                long nativeDesktopCapturerAndroid,
                Runnable releaseCb,
                long timestampNs,
                ByteBuffer yBuffer,
                int yPixelStride,
                int yRowStride,
                ByteBuffer uBuffer,
                int uPixelStride,
                int uRowStride,
                ByteBuffer vBuffer,
                int vPixelStride,
                int vRowStride,
                int left,
                int top,
                int right,
                int bottom);

        void onStop(long nativeDesktopCapturerAndroid);
    }
}
