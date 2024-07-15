// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * This class implements a factory of Android Video Capture objects for Chrome. Cameras are
 * identified by |id|. Video Capture objects allocated via createVideoCapture() are explicitly owned
 * by the caller.
 */
@JNINamespace("media")
@SuppressWarnings("deprecation")
class VideoCaptureFactory {
    @CalledByNative
    static boolean isLegacyOrDeprecatedDevice(int id) {
        return VideoCaptureCamera2.isLegacyDevice(id);
    }

    // Factory methods.
    @CalledByNative
    static VideoCapture createVideoCapture(int id, long nativeVideoCaptureDeviceAndroid) {
        if (isLegacyOrDeprecatedDevice(id)) {
            return new VideoCaptureCamera(id, nativeVideoCaptureDeviceAndroid);
        }
        return new VideoCaptureCamera2(id, nativeVideoCaptureDeviceAndroid);
    }

    @CalledByNative
    static int getNumberOfCameras() {
        return VideoCaptureCamera2.getNumberOfCameras();
    }

    @CalledByNative
    static int getCaptureApiType(int index) {
        if (isLegacyOrDeprecatedDevice(index)) {
            return VideoCaptureCamera.getCaptureApiType(index);
        }
        return VideoCaptureCamera2.getCaptureApiType(index);
    }

    @CalledByNative
    static boolean isZoomSupported(int index) {
        if (isLegacyOrDeprecatedDevice(index)) {
            return VideoCaptureCamera.isZoomSupported(index);
        }
        return VideoCaptureCamera2.isZoomSupported(index);
    }

    @CalledByNative
    static int getFacingMode(int index) {
        if (isLegacyOrDeprecatedDevice(index)) {
            return VideoCaptureCamera.getFacingMode(index);
        }
        return VideoCaptureCamera2.getFacingMode(index);
    }

    @CalledByNative
    static String getDeviceId(int index) {
        if (isLegacyOrDeprecatedDevice(index)) {
            return VideoCaptureCamera.getDeviceId(index);
        }
        return VideoCaptureCamera2.getDeviceId(index);
    }

    @CalledByNative
    static String getDeviceName(int index) {
        if (isLegacyOrDeprecatedDevice(index)) {
            return VideoCaptureCamera.getName(index);
        }
        return VideoCaptureCamera2.getName(index);
    }

    @CalledByNative
    static VideoCaptureFormat[] getDeviceSupportedFormats(int index) {
        if (isLegacyOrDeprecatedDevice(index)) {
            return VideoCaptureCamera.getDeviceSupportedFormats(index);
        }
        return VideoCaptureCamera2.getDeviceSupportedFormats(index);
    }

    @CalledByNative
    static int getCaptureFormatWidth(VideoCaptureFormat format) {
        return format.getWidth();
    }

    @CalledByNative
    static int getCaptureFormatHeight(VideoCaptureFormat format) {
        return format.getHeight();
    }

    @CalledByNative
    static int getCaptureFormatFramerate(VideoCaptureFormat format) {
        return format.getFramerate();
    }

    @CalledByNative
    static int getCaptureFormatPixelFormat(VideoCaptureFormat format) {
        return format.getPixelFormat();
    }
}
