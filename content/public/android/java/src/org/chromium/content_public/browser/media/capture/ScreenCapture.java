// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import androidx.activity.result.ActivityResult;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

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

    private long mNativeDesktopCapturerAndroid;

    private ScreenCapture(long nativeDesktopCapturerAndroid) {
        mNativeDesktopCapturerAndroid = nativeDesktopCapturerAndroid;
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
    void startCapture() {
        // TODO(crbug.com/352187279): Implement this.
        assert mNativeDesktopCapturerAndroid != 0;
    }

    @CalledByNative
    void destroy() {
        mNativeDesktopCapturerAndroid = 0;
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
    }
}
