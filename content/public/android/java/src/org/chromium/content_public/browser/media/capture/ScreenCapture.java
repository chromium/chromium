// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

import java.nio.ByteBuffer;

/** See comments on `DesktopCapturerAndroid`. */
@NullMarked
@JNINamespace("content")
public class ScreenCapture {
    private static final String TAG = "ScreenCapture";

    private long mNativeDesktopCapturerAndroid;

    private ScreenCapture(long nativeDesktopCapturerAndroid) {
        mNativeDesktopCapturerAndroid = nativeDesktopCapturerAndroid;
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
