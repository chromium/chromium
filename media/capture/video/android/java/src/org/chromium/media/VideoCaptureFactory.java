// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This class implements a factory of Android Video Capture objects for Chrome.
 * Cameras are identified by |id|. Video Capture objects allocated via
 * createVideoCapture() are explicitly owned by the caller. ChromiumCameraInfo
 * is an internal class with some static methods needed from the rest of the
 * class to manipulate the |id|s of devices.
 **/
@JNINamespace("media")
@SuppressWarnings("deprecation")
class VideoCaptureFactory {
    // Internal class to encapsulate camera device id manipulations.
    static class ChromiumCameraInfo {
        private static int sNumberOfSystemCameras = -1;
        private static final String TAG = "media";

        private static int getNumberOfCameras() {
            if (sNumberOfSystemCameras == -1) {
                // getNumberOfCameras() would not fail due to lack of permission, but the
                // following operations on camera would. "No permission" isn't a fatal
                // error in WebView, specially for those applications which have no purpose
                // to use a camera, but "load page" requires it. So, output a warning log
                // and carry on pretending the system has no camera(s).  This optimization
                // applies only to pre-M on Android because that is when runtime permissions
                // were introduced.
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M
                        && ContextUtils.getApplicationContext().getPackageManager().checkPermission(
                                   Manifest.permission.CAMERA,
                                   ContextUtils.getApplicationContext().getPackageName())
                                != PackageManager.PERMISSION_GRANTED) {
                    sNumberOfSystemCameras = 0;
                    Log.w(TAG, "Missing android.permission.CAMERA permission, "
                                    + "no system camera available.");
                } else {
                    sNumberOfSystemCameras = VideoCaptureCamera2.getNumberOfCameras();
                }
            }
            return sNumberOfSystemCameras;
        }
    }

    @CalledByNative
    static boolean isLegacyOrDeprecatedDevice(int index) {
        return VideoCaptureCamera2.isLegacyDevice(index);
    }

    // Factory methods.
    @CalledByNative
    static VideoCapture createVideoCapture(int index, long nativeVideoCaptureDeviceAndroid) {
        if (isLegacyOrDeprecatedDevice(index)) {
            return new VideoCaptureCamera(index, nativeVideoCaptureDeviceAndroid);
        }
        return new VideoCaptureCamera2(index, nativeVideoCaptureDeviceAndroid);
    }

    @CalledByNative
    static int getNumberOfCameras() {
        return ChromiumCameraInfo.getNumberOfCameras();
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
