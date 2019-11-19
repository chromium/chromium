// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.graphics.ImageFormat;
import android.view.Surface;
import android.view.WindowManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Video Capture Device base class, defines a set of methods that native code
 * needs to use to configure, start capture, and to be reached by callbacks and
 * provides some necessary data type(s) with accessors.
 **/
@JNINamespace("media")
public abstract class VideoCapture {
    /**
     * Common class for storing a framerate range. Values should be multiplied by 1000.
     */
    protected static class FramerateRange {
        public int min;
        public int max;

        public FramerateRange(int min, int max) {
            this.min = min;
            this.max = max;
        }
    }

    // The angle (0, 90, 180, 270) that the image needs to be rotated to show in
    // the display's native orientation.
    protected int mCameraNativeOrientation;
    // In some occasions we need to invert the device rotation readings, see the
    // individual implementations.
    protected boolean mInvertDeviceOrientationReadings;

    protected VideoCaptureFormat mCaptureFormat;
    protected final int mId;
    // Native callback context variable.
    protected final long mNativeVideoCaptureDeviceAndroid;

    protected boolean mUseBackgroundThreadForTesting;

    VideoCapture(int id, long nativeVideoCaptureDeviceAndroid) {
        mId = id;
        mNativeVideoCaptureDeviceAndroid = nativeVideoCaptureDeviceAndroid;
    }

    // Allocate necessary resources for capture.
    @CalledByNative
    public abstract boolean allocate(
            int width, int height, int frameRate, boolean enableFaceDetection);

    // Success is indicated by returning true and a callback to
    // VideoCaptureJni.get().onStarted(,  VideoCapture.this), which may occur synchronously or
    // asynchronously. Failure can be indicated by one of the following:
    // * Returning false. In this case no callback to VideoCaptureJni.get().onStarted() is made.
    // * Returning true, and asynchronously invoking VideoCaptureJni.get().onError. In this case
    //   also no callback to VideoCaptureJni.get().onStarted() is made.
    @CalledByNative
    public abstract boolean startCaptureMaybeAsync();

    // Blocks until it is guaranteed that no more frames are sent.
    @CalledByNative
    public abstract boolean stopCaptureAndBlockUntilStopped();

    // Replies by calling VideoCaptureJni.get().onGetPhotoCapabilitiesReply(). Will pass |null|
    // for parameter |result| to indicate failure.
    @CalledByNative
    public abstract void getPhotoCapabilitiesAsync(long callbackId);

    /**
     * @param zoom Zoom level, should be ignored if 0.
     * @param focusMode Focus mode following AndroidMeteringMode enum.
     * @param focusDistance Desired distance to plane of sharpest focus.
     * @param exposureMode Exposure mode following AndroidMeteringMode enum.
     * @param pointsOfInterest2D 2D normalized points of interest, marshalled with
     * x coordinate first followed by the y coordinate.
     * @param hasExposureCompensation Indicates if |exposureCompensation| is set.
     * @param exposureCompensation Adjustment to auto exposure. 0 means not adjusted.
     * @param exposureTime Duration each pixel is exposed to light (in nanoseconds).
     * @param whiteBalanceMode White Balance mode following AndroidMeteringMode enum.
     * @param iso Sensitivity to light. 0, which would be invalid, means ignore.
     * @param hasRedEyeReduction Indicates if |redEyeReduction| is set.
     * @param redEyeReduction Value of red eye reduction for the auto flash setting.
     * @param fillLightMode Flash setting, following AndroidFillLightMode enum.
     * @param colorTemperature White Balance reference temperature, valid if whiteBalanceMode is
     * manual, and its value is larger than 0.
     * @param torch Torch setting, true meaning on.
     */
    @CalledByNative
    public abstract void setPhotoOptions(double zoom, int focusMode, double focusDistance,
            int exposureMode, double width, double height, double[] pointsOfInterest2D,
            boolean hasExposureCompensation, double exposureCompensation, double exposureTime,
            int whiteBalanceMode, double iso, boolean hasRedEyeReduction, boolean redEyeReduction,
            int fillLightMode, boolean hasTorch, boolean torch, double colorTemperature);

    // Replies by calling VideoCaptureJni.get().onPhotoTaken().
    @CalledByNative
    public abstract void takePhotoAsync(long callbackId);

    @CalledByNative
    public abstract void deallocate();

    @CalledByNative
    public final int queryWidth() {
        return mCaptureFormat.mWidth;
    }

    @CalledByNative
    public final int queryHeight() {
        return mCaptureFormat.mHeight;
    }

    @CalledByNative
    public final int queryFrameRate() {
        return mCaptureFormat.mFramerate;
    }

    @CalledByNative
    public final int getColorspace() {
        switch (mCaptureFormat.mPixelFormat) {
            case ImageFormat.YV12:
                return AndroidImageFormat.YV12;
            case ImageFormat.YUV_420_888:
                return AndroidImageFormat.YUV_420_888;
            case ImageFormat.NV21:
                return AndroidImageFormat.NV21;
            case ImageFormat.UNKNOWN:
            default:
                return AndroidImageFormat.UNKNOWN;
        }
    }

    @CalledByNative
    public final void setTestMode() {
        mUseBackgroundThreadForTesting = true;
    }

    protected final int getCameraRotation() {
        int rotation = mInvertDeviceOrientationReadings ? (360 - getDeviceRotation())
                                                        : getDeviceRotation();
        return (mCameraNativeOrientation + rotation) % 360;
    }

    protected final int getDeviceRotation() {
        final int orientation;
        WindowManager wm = (WindowManager) ContextUtils.getApplicationContext().getSystemService(
                Context.WINDOW_SERVICE);
        switch (wm.getDefaultDisplay().getRotation()) {
            case Surface.ROTATION_90:
                orientation = 90;
                break;
            case Surface.ROTATION_180:
                orientation = 180;
                break;
            case Surface.ROTATION_270:
                orientation = 270;
                break;
            case Surface.ROTATION_0:
            default:
                orientation = 0;
                break;
        }
        return orientation;
    }

    // {@link VideoCaptureJni.get().onPhotoTaken()} needs to be called back if there's any
    // problem after {@link takePhotoAsync()} has returned true.
    protected void notifyTakePhotoError(long callbackId) {
        VideoCaptureJni.get().onPhotoTaken(
                mNativeVideoCaptureDeviceAndroid, VideoCapture.this, callbackId, null);
    }

    /**
     * Finds the framerate range matching |targetFramerate|. Tries to find a range with as low of a
     * minimum value as possible to allow the camera adjust based on the lighting conditions.
     * Assumes that all framerate values are multiplied by 1000.
     *
     * This code is mostly copied from WebRTC:
     * CameraEnumerationAndroid.getClosestSupportedFramerateRange
     * in webrtc/api/android/java/src/org/webrtc/CameraEnumerationAndroid.java
     */
    protected static FramerateRange getClosestFramerateRange(
            final List<FramerateRange> framerateRanges, final int targetFramerate) {
        return Collections.min(framerateRanges, new Comparator<FramerateRange>() {
            // Threshold and penalty weights if the upper bound is further away than
            // |MAX_FPS_DIFF_THRESHOLD| from requested.
            private static final int MAX_FPS_DIFF_THRESHOLD = 5000;
            private static final int MAX_FPS_LOW_DIFF_WEIGHT = 1;
            private static final int MAX_FPS_HIGH_DIFF_WEIGHT = 3;

            // Threshold and penalty weights if the lower bound is bigger than |MIN_FPS_THRESHOLD|.
            private static final int MIN_FPS_THRESHOLD = 8000;
            private static final int MIN_FPS_LOW_VALUE_WEIGHT = 1;
            private static final int MIN_FPS_HIGH_VALUE_WEIGHT = 4;

            // Use one weight for small |value| less than |threshold|, and another weight above.
            private int progressivePenalty(
                    int value, int threshold, int lowWeight, int highWeight) {
                return (value < threshold)
                        ? value * lowWeight
                        : threshold * lowWeight + (value - threshold) * highWeight;
            }

            int diff(FramerateRange range) {
                final int minFpsError = progressivePenalty(range.min, MIN_FPS_THRESHOLD,
                        MIN_FPS_LOW_VALUE_WEIGHT, MIN_FPS_HIGH_VALUE_WEIGHT);
                final int maxFpsError = progressivePenalty(Math.abs(targetFramerate - range.max),
                        MAX_FPS_DIFF_THRESHOLD, MAX_FPS_LOW_DIFF_WEIGHT, MAX_FPS_HIGH_DIFF_WEIGHT);
                return minFpsError + maxFpsError;
            }

            @Override
            public int compare(FramerateRange range1, FramerateRange range2) {
                return diff(range1) - diff(range2);
            }
        });
    }

    protected static int[] integerArrayListToArray(ArrayList<Integer> intArrayList) {
        int[] intArray = new int[intArrayList.size()];
        for (int i = 0; i < intArrayList.size(); i++) {
            intArray[i] = intArrayList.get(i).intValue();
        }
        return intArray;
    }

    @NativeMethods
    interface Natives {
        // Method for VideoCapture implementations to call back native code.
        void onFrameAvailable(long nativeVideoCaptureDeviceAndroid, VideoCapture caller,
                byte[] data, int length, int rotation);

        void onI420FrameAvailable(long nativeVideoCaptureDeviceAndroid, VideoCapture caller,
                ByteBuffer yBuffer, int yStride, ByteBuffer uBuffer, ByteBuffer vBuffer,
                int uvRowStride, int uvPixelStride, int width, int height, int rotation,
                long timestamp);
        // Method for VideoCapture implementations to signal an asynchronous error.
        void onError(long nativeVideoCaptureDeviceAndroid, VideoCapture caller,
                int androidVideoCaptureError, String message);

        // Method for VideoCapture implementations to signal that a frame was dropped.
        void onFrameDropped(long nativeVideoCaptureDeviceAndroid, VideoCapture caller,
                int androidVideoCaptureFrameDropReason);

        void onGetPhotoCapabilitiesReply(long nativeVideoCaptureDeviceAndroid, VideoCapture caller,
                long callbackId, PhotoCapabilities result);
        // Callback for calls to takePhoto(). This can indicate both success and
        // failure. Failure is indicated by |data| being null.
        void onPhotoTaken(long nativeVideoCaptureDeviceAndroid, VideoCapture caller,
                long callbackId, byte[] data);

        // Method for VideoCapture implementations to report device started event.
        void onStarted(long nativeVideoCaptureDeviceAndroid, VideoCapture caller);

        void dCheckCurrentlyOnIncomingTaskRunner(
                long nativeVideoCaptureDeviceAndroid, VideoCapture caller);
    }
}
