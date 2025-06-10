// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import android.content.Context;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.HandlerThread;

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

        mMediaProjection.registerCallback(new MediaProjectionCallback(), mBackgroundHandler);

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
            synchronized (mNativeDestructionLock) {
                if (mNativeDesktopCapturerAndroid == 0) return;
                mMediaProjection = null;
                ScreenCaptureJni.get().onStop(mNativeDesktopCapturerAndroid);
            }
        }
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
