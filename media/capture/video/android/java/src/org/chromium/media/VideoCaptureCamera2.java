// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.MeteringRectangle;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Range;
import android.util.Size;
import android.util.SparseIntArray;
import android.view.Surface;

import androidx.annotation.IntDef;

import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * This class implements Video Capture using Camera2 API, introduced in Android
 * API 21 (L Release). Capture takes place in the current Looper, while pixel
 * download takes place in another thread used by ImageReader. A number of
 * static methods are provided to retrieve information on current system cameras
 * and their capabilities, using android.hardware.camera2.CameraManager.
 **/
@JNINamespace("media")
public class VideoCaptureCamera2 extends VideoCapture {
    // Inner class to extend a CameraDevice state change listener.
    private class CrStateListener extends CameraDevice.StateCallback {
        @Override
        public void onOpened(CameraDevice cameraDevice) {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

            Log.e(TAG, "CameraDevice.StateCallback onOpened");
            mCameraDevice = cameraDevice;
            mWaitForDeviceClosedConditionVariable.close();
            changeCameraStateAndNotify(CameraState.CONFIGURING);
            createPreviewObjectsAndStartPreviewOrFailWith(
                    AndroidVideoCaptureError.ANDROID_API_2_ERROR_CONFIGURING_CAMERA);
        }

        @Override
        public void onDisconnected(CameraDevice cameraDevice) {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";
            Log.e(TAG, "cameraDevice was closed unexpectedly");

            cameraDevice.close();
            mCameraDevice = null;
            changeCameraStateAndNotify(CameraState.STOPPED);
        }

        @Override
        public void onError(CameraDevice cameraDevice, int error) {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";
            Log.e(TAG, "cameraDevice encountered an error");

            cameraDevice.close();
            mCameraDevice = null;
            changeCameraStateAndNotify(CameraState.STOPPED);
            VideoCaptureCamera2.this.onError(
                    VideoCaptureCamera2.this,
                    AndroidVideoCaptureError.ANDROID_API_2_CAMERA_DEVICE_ERROR_RECEIVED,
                    "Camera device error " + Integer.toString(error));
        }

        @Override
        public void onClosed(CameraDevice camera) {
            Log.d(TAG, "cameraDevice closed");
            // If we called mCameraDevice.close() while mPreviewSession was running,
            // mPreviewSession will get closed, but the corresponding CrPreviewSessionListener
            // will not receive a callback to onClosed(). Therefore we have to clean up
            // the reference to mPreviewSession here.
            if (mPreviewSession != null) {
                mPreviewSession = null;
            }

            mWaitForDeviceClosedConditionVariable.open();
        }
    }
    ;

    // Inner class to extend a Capture Session state change listener.
    private class CrPreviewSessionListener extends CameraCaptureSession.StateCallback {
        private final CaptureRequest mPreviewRequest;

        CrPreviewSessionListener(CaptureRequest previewRequest) {
            mPreviewRequest = previewRequest;
        }

        @Override
        public void onConfigured(CameraCaptureSession cameraCaptureSession) {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

            Log.d(TAG, "CrPreviewSessionListener.onConfigured");
            mPreviewSession = cameraCaptureSession;
            try {
                // This line triggers the preview. A |listener| is registered to receive the actual
                // capture result details. A CrImageReaderListener will be triggered every time a
                // downloaded image is ready. Since |handler| is null, we'll work on the current
                // Thread Looper.
                mPreviewSession.setRepeatingRequest(
                        mPreviewRequest,
                        new CameraCaptureSession.CaptureCallback() {
                            @Override
                            public void onCaptureCompleted(
                                    CameraCaptureSession session,
                                    CaptureRequest request,
                                    TotalCaptureResult result) {
                                // Since |result| is not guaranteed to contain a value for
                                // key |SENSOR_EXPOSURE_TIME| we have to check for null.
                                Long exposure_time_value =
                                        result.get(CaptureResult.SENSOR_EXPOSURE_TIME);
                                if (exposure_time_value == null) return;
                                mLastExposureTimeNs = exposure_time_value;
                            }
                        },
                        null);

            } catch (CameraAccessException
                    | SecurityException
                    | IllegalStateException
                    | IllegalArgumentException ex) {
                Log.e(TAG, "setRepeatingRequest: ", ex);
                return;
            }

            changeCameraStateAndNotify(CameraState.STARTED);
            onStarted(VideoCaptureCamera2.this);

            // Frames will be arriving at CrPreviewReaderListener.onImageAvailable();
        }

        @Override
        public void onConfigureFailed(CameraCaptureSession cameraCaptureSession) {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";
            Log.d(TAG, "CrPreviewSessionListener.onConfigureFailed");

            // TODO(mcasas): When signalling error, C++ will tear us down. Is there need for
            // cleanup?
            changeCameraStateAndNotify(CameraState.STOPPED);
            mPreviewSession = null;
            onError(
                    VideoCaptureCamera2.this,
                    AndroidVideoCaptureError.ANDROID_API_2_CAPTURE_SESSION_CONFIGURE_FAILED,
                    "Camera session configuration error");
        }

        @Override
        public void onClosed(CameraCaptureSession cameraCaptureSession) {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";
            Log.d(TAG, "CrPreviewSessionListener.onClosed");

            // The preview session gets closed temporarily when a takePhoto
            // request is being processed. A new preview session will be
            // started after that.
            mPreviewSession = null;
        }
    }
    ;

    // Internal class implementing an ImageReader listener for Preview frames. Gets pinged when a
    // new frame is been captured and downloads it to memory-backed buffers.
    private class CrPreviewReaderListener implements ImageReader.OnImageAvailableListener {
        @Override
        public void onImageAvailable(ImageReader reader) {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

            try (Image image = reader.acquireLatestImage()) {
                if (image == null) {
                    onFrameDropped(
                            VideoCaptureCamera2.this,
                            AndroidVideoCaptureFrameDropReason
                                    .ANDROID_API_2_ACQUIRED_IMAGE_IS_NULL);
                    return;
                }

                if (image.getFormat() != ImageFormat.YUV_420_888 || image.getPlanes().length != 3) {
                    onError(
                            VideoCaptureCamera2.this,
                            AndroidVideoCaptureError
                                    .ANDROID_API_2_IMAGE_READER_UNEXPECTED_IMAGE_FORMAT,
                            "Unexpected image format: "
                                    + image.getFormat()
                                    + " or #planes: "
                                    + image.getPlanes().length);
                    throw new IllegalStateException();
                }

                if (reader.getWidth() != image.getWidth()
                        || reader.getHeight() != image.getHeight()) {
                    onError(
                            VideoCaptureCamera2.this,
                            AndroidVideoCaptureError
                                    .ANDROID_API_2_IMAGE_READER_SIZE_DID_NOT_MATCH_IMAGE_SIZE,
                            "ImageReader size ("
                                    + reader.getWidth()
                                    + "x"
                                    + reader.getHeight()
                                    + ") did not match Image size ("
                                    + image.getWidth()
                                    + "x"
                                    + image.getHeight()
                                    + ")");
                    throw new IllegalStateException();
                }

                onI420FrameAvailable(
                        VideoCaptureCamera2.this,
                        image.getPlanes()[0].getBuffer(),
                        image.getPlanes()[0].getRowStride(),
                        image.getPlanes()[1].getBuffer(),
                        image.getPlanes()[2].getBuffer(),
                        image.getPlanes()[1].getRowStride(),
                        image.getPlanes()[1].getPixelStride(),
                        image.getWidth(),
                        image.getHeight(),
                        getCameraRotation(),
                        image.getTimestamp());
            } catch (IllegalStateException ex) {
                Log.e(TAG, "acquireLatestImage():", ex);
            }
        }
    }
    ;

    // Inner class to extend a Photo Session state change listener.
    // Error paths must signal notifyTakePhotoError().
    private class CrPhotoSessionListener extends CameraCaptureSession.StateCallback {
        private final ImageReader mImageReader;
        private final CaptureRequest mPhotoRequest;
        private final long mCallbackId;

        CrPhotoSessionListener(
                ImageReader imageReader, CaptureRequest photoRequest, long callbackId) {
            mImageReader = imageReader;
            mPhotoRequest = photoRequest;
            mCallbackId = callbackId;
        }

        @Override
        public void onConfigured(CameraCaptureSession session) {
            TraceEvent.instant("VideoCaptureCamera2.java", "CrPhotoSessionListener.onConfigured");
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

            Log.d(TAG, "CrPhotoSessionListener.onConfigured");
            try {
                TraceEvent.instant(
                        "VideoCaptureCamera2.java", "Calling CameraCaptureSession.capture()");
                // This line triggers a single photo capture. No |listener| is registered, so we
                // will get notified via a CrPhotoSessionListener. Since |handler| is null, we'll
                // work on the current Thread Looper.
                session.capture(mPhotoRequest, null, null);
            } catch (CameraAccessException ex) {
                Log.e(TAG, "capture() CameraAccessException", ex);
                notifyTakePhotoError(mCallbackId);
                return;
            } catch (IllegalStateException ex) {
                Log.e(TAG, "capture() IllegalStateException", ex);
                notifyTakePhotoError(mCallbackId);
                return;
            }
        }

        @Override
        public void onConfigureFailed(CameraCaptureSession session) {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

            Log.e(TAG, "failed configuring capture session");
            notifyTakePhotoError(mCallbackId);
        }

        @Override
        public void onClosed(CameraCaptureSession session) {
            mImageReader.close();
        }
    }

    // Internal class implementing an ImageReader listener for encoded Photos.
    // Gets pinged when a new Image is been captured.
    private class CrPhotoReaderListener implements ImageReader.OnImageAvailableListener {
        private final long mCallbackId;

        CrPhotoReaderListener(long callbackId) {
            mCallbackId = callbackId;
        }

        private byte[] readCapturedData(Image image) {
            byte[] capturedData = null;
            try {
                capturedData = image.getPlanes()[0].getBuffer().array();
            } catch (UnsupportedOperationException ex) {
                // Try reading the pixels in a different way.
                final ByteBuffer buffer = image.getPlanes()[0].getBuffer();
                capturedData = new byte[buffer.remaining()];
                buffer.get(capturedData);
            } finally {
                return capturedData;
            }
        }

        @Override
        public void onImageAvailable(ImageReader reader) {
            TraceEvent.instant(
                    "VideoCaptureCamera2.java", "CrPhotoReaderListener.onImageAvailable");
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

            try (Image image = reader.acquireLatestImage()) {
                if (image == null) {
                    throw new IllegalStateException();
                }

                if (image.getFormat() != ImageFormat.JPEG) {
                    Log.e(TAG, "Unexpected image format: %d", image.getFormat());
                    throw new IllegalStateException();
                }

                final byte[] capturedData = readCapturedData(image);
                onPhotoTaken(VideoCaptureCamera2.this, mCallbackId, capturedData);

            } catch (IllegalStateException ex) {
                notifyTakePhotoError(mCallbackId);
                return;
            }

            createPreviewObjectsAndStartPreviewOrFailWith(
                    AndroidVideoCaptureError.ANDROID_API_2_ERROR_RESTARTING_PREVIEW);
        }
    }
    ;

    private class StopCaptureTask implements Runnable {
        @Override
        public void run() {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

            if (mCameraDevice == null) return;

            // As per Android API documentation, this will automatically abort captures
            // pending for mPreviewSession, but it will not lead to callbacks such as
            // onClosed() to the corresponding CrPreviewSessionListener.
            // Different from what the Android API documentation says, pending frames
            // may still get delivered after this call. Therefore, we have to wait for
            // CrStateListener.onClosed() in order to have a guarantee that no more
            // frames are delivered.
            mCameraDevice.close();

            changeCameraStateAndNotify(CameraState.STOPPED);
            mCropRect = new Rect();
        }
    }

    private class GetPhotoCapabilitiesTask implements Runnable {
        private final long mCallbackId;

        public GetPhotoCapabilitiesTask(long callbackId) {
            mCallbackId = callbackId;
        }

        @Override
        public void run() {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

            final CameraCharacteristics cameraCharacteristics = getCameraCharacteristics(mId);
            PhotoCapabilities.Builder builder = new PhotoCapabilities.Builder();
            if (cameraCharacteristics == null) {
                onGetPhotoCapabilitiesReply(VideoCaptureCamera2.this, mCallbackId, builder.build());
                return;
            }
            int minIso = 0;
            int maxIso = 0;
            final Range<Integer> iso_range =
                    cameraCharacteristics.get(CameraCharacteristics.SENSOR_INFO_SENSITIVITY_RANGE);
            if (iso_range != null) {
                minIso = iso_range.getLower();
                maxIso = iso_range.getUpper();
            }
            builder.setInt(PhotoCapabilityInt.MIN_ISO, minIso)
                    .setInt(PhotoCapabilityInt.MAX_ISO, maxIso)
                    .setInt(PhotoCapabilityInt.STEP_ISO, 1);
            if (mPreviewRequest.get(CaptureRequest.SENSOR_SENSITIVITY) != null) {
                builder.setInt(
                        PhotoCapabilityInt.CURRENT_ISO,
                        mPreviewRequest.get(CaptureRequest.SENSOR_SENSITIVITY));
            }

            final StreamConfigurationMap streamMap =
                    cameraCharacteristics.get(
                            CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            final Size[] supportedSizes = streamMap.getOutputSizes(ImageFormat.JPEG);
            int minWidth = Integer.MAX_VALUE;
            int minHeight = Integer.MAX_VALUE;
            int maxWidth = 0;
            int maxHeight = 0;
            for (Size size : supportedSizes) {
                if (size.getWidth() < minWidth) minWidth = size.getWidth();
                if (size.getHeight() < minHeight) minHeight = size.getHeight();
                if (size.getWidth() > maxWidth) maxWidth = size.getWidth();
                if (size.getHeight() > maxHeight) maxHeight = size.getHeight();
            }
            builder.setInt(PhotoCapabilityInt.MIN_HEIGHT, minHeight)
                    .setInt(PhotoCapabilityInt.MAX_HEIGHT, maxHeight)
                    .setInt(PhotoCapabilityInt.STEP_HEIGHT, 1)
                    .setInt(
                            PhotoCapabilityInt.CURRENT_HEIGHT,
                            (mPhotoHeight > 0) ? mPhotoHeight : mCaptureFormat.getHeight())
                    .setInt(PhotoCapabilityInt.MIN_WIDTH, minWidth)
                    .setInt(PhotoCapabilityInt.MAX_WIDTH, maxWidth)
                    .setInt(PhotoCapabilityInt.STEP_WIDTH, 1)
                    .setInt(
                            PhotoCapabilityInt.CURRENT_WIDTH,
                            (mPhotoWidth > 0) ? mPhotoWidth : mCaptureFormat.getWidth());

            float currentZoom = 1.0f;
            if (cameraCharacteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE)
                            != null
                    && mPreviewRequest.get(CaptureRequest.SCALER_CROP_REGION) != null) {
                currentZoom =
                        cameraCharacteristics
                                        .get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE)
                                        .width()
                                / (float)
                                        mPreviewRequest
                                                .get(CaptureRequest.SCALER_CROP_REGION)
                                                .width();
            }
            // There is no min-zoom per se, so clamp it to always 1.
            builder.setDouble(PhotoCapabilityDouble.MIN_ZOOM, 1.0)
                    .setDouble(PhotoCapabilityDouble.MAX_ZOOM, mMaxZoom)
                    .setDouble(PhotoCapabilityDouble.CURRENT_ZOOM, currentZoom)
                    .setDouble(PhotoCapabilityDouble.STEP_ZOOM, 0.1);

            // Classify the Focus capabilities. In CONTINUOUS and SINGLE_SHOT, we can call
            // autoFocus(AutoFocusCallback) to configure region(s) to focus onto.
            final int[] jniFocusModes =
                    cameraCharacteristics.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES);
            ArrayList<Integer> focusModes = new ArrayList<Integer>(3);

            // Android reports the focus metadata in units of diopters (1/meter), so
            // 0.0f represents focusing at infinity, and increasing positive numbers represent
            // focusing closer and closer to the camera device.
            float minFocusDistance = 0; // >= 0
            float maxFocusDistance = 0; // (0.0f, android.lens.info.minimumFocusDistance]
            if (cameraCharacteristics.get(CameraCharacteristics.LENS_INFO_MINIMUM_FOCUS_DISTANCE)
                    != null) {
                minFocusDistance =
                        cameraCharacteristics.get(
                                CameraCharacteristics.LENS_INFO_MINIMUM_FOCUS_DISTANCE);
                if (minFocusDistance == 0) {
                    Log.d(TAG, "lens is fixed-focus");
                } else if (minFocusDistance > 0) {
                    // Android provides focusDistance in diopters, but specs is in SI units
                    // (meters).
                    minFocusDistance = 1 / minFocusDistance;
                }
            } else { //  null value
                Log.d(TAG, "LENS_INFO_MINIMUM_FOCUS_DISTANCE is null");
            }
            if (cameraCharacteristics.get(CameraCharacteristics.LENS_INFO_HYPERFOCAL_DISTANCE)
                    != null) {
                maxFocusDistance =
                        cameraCharacteristics.get(
                                CameraCharacteristics.LENS_INFO_HYPERFOCAL_DISTANCE);
                if (maxFocusDistance == 0) {
                    maxFocusDistance = (long) Double.POSITIVE_INFINITY;
                } else if (maxFocusDistance > 0) {
                    // Android provides focusDistance in diopters, but specs is in SI units
                    // (meters).
                    maxFocusDistance = 1 / maxFocusDistance;
                }
            } else { //  null value
                Log.d(TAG, "LENS_INFO_HYPERFOCAL_DISTANCE is null");
            }
            if (mPreviewRequest.get(CaptureRequest.LENS_FOCUS_DISTANCE) != null) {
                mCurrentFocusDistance = mPreviewRequest.get(CaptureRequest.LENS_FOCUS_DISTANCE);

                // LENS_FOCUS_DISTANCE is in the range [0.0f,
                // android.lens.info.minimumFocusDistance] Android provides focusDistance in
                // diopters, but specs is in SI units (meters).
                if (mCurrentFocusDistance == 0) {
                    Log.d(TAG, "infinity focus.");
                    mCurrentFocusDistance = (long) Double.POSITIVE_INFINITY;
                } else if (mCurrentFocusDistance > 0) {
                    builder.setDouble(
                            PhotoCapabilityDouble.CURRENT_FOCUS_DISTANCE,
                            1 / mCurrentFocusDistance);
                }
            } else { //  null value
                Log.d(TAG, "LENS_FOCUS_DISTANCE is null");
            }

            for (int mode : jniFocusModes) {
                if (mode == CameraMetadata.CONTROL_AF_MODE_OFF) {
                    focusModes.add(Integer.valueOf(AndroidMeteringMode.FIXED));
                    // Smallest step by which focus distance can be changed. This value is not
                    // exposed by Android.
                    float mStepFocusDistance = 0.01f;
                    builder.setDouble(PhotoCapabilityDouble.MIN_FOCUS_DISTANCE, minFocusDistance)
                            .setDouble(PhotoCapabilityDouble.MAX_FOCUS_DISTANCE, maxFocusDistance)
                            .setDouble(
                                    PhotoCapabilityDouble.STEP_FOCUS_DISTANCE, mStepFocusDistance);
                } else if (mode == CameraMetadata.CONTROL_AF_MODE_AUTO
                        || mode == CameraMetadata.CONTROL_AF_MODE_MACRO) {
                    // CONTROL_AF_MODE_{AUTO,MACRO} do not imply continuously focusing.
                    if (!focusModes.contains(Integer.valueOf(AndroidMeteringMode.SINGLE_SHOT))) {
                        focusModes.add(Integer.valueOf(AndroidMeteringMode.SINGLE_SHOT));
                    }
                } else if (mode == CameraMetadata.CONTROL_AF_MODE_CONTINUOUS_VIDEO
                        || mode == CameraMetadata.CONTROL_AF_MODE_CONTINUOUS_PICTURE
                        || mode == CameraMetadata.CONTROL_AF_MODE_EDOF) {
                    if (!focusModes.contains(Integer.valueOf(AndroidMeteringMode.CONTINUOUS))) {
                        focusModes.add(Integer.valueOf(AndroidMeteringMode.CONTINUOUS));
                    }
                }
            }
            builder.setMeteringModeArray(
                    MeteringModeType.FOCUS, integerArrayListToArray(focusModes));

            int jniFocusMode = AndroidMeteringMode.NONE;
            if (mPreviewRequest.get(CaptureRequest.CONTROL_AF_MODE) != null) {
                final int focusMode = mPreviewRequest.get(CaptureRequest.CONTROL_AF_MODE);
                if (focusMode == CameraMetadata.CONTROL_AF_MODE_CONTINUOUS_VIDEO
                        || focusMode == CameraMetadata.CONTROL_AF_MODE_CONTINUOUS_PICTURE) {
                    jniFocusMode = AndroidMeteringMode.CONTINUOUS;
                } else if (focusMode == CameraMetadata.CONTROL_AF_MODE_AUTO
                        || focusMode == CameraMetadata.CONTROL_AF_MODE_MACRO) {
                    jniFocusMode = AndroidMeteringMode.SINGLE_SHOT;
                } else if (focusMode == CameraMetadata.CONTROL_AF_MODE_OFF) {
                    jniFocusMode = AndroidMeteringMode.FIXED;
                    // Set focus distance here.
                    if (mCurrentFocusDistance > 0) {
                        builder.setDouble(
                                PhotoCapabilityDouble.CURRENT_FOCUS_DISTANCE,
                                1 / mCurrentFocusDistance);
                    }
                } else {
                    assert jniFocusMode == CameraMetadata.CONTROL_AF_MODE_EDOF;
                }
            }
            builder.setMeteringMode(MeteringModeType.FOCUS, jniFocusMode);

            // Auto Exposure is the usual capability and state, unless AE is not available at all,
            // which is signalled by an empty CONTROL_AE_AVAILABLE_MODES list. Exposure Compensation
            // can also support or be locked, this is equivalent to AndroidMeteringMode.FIXED.
            final int[] jniExposureModes =
                    cameraCharacteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_MODES);
            ArrayList<Integer> exposureModes = new ArrayList<Integer>(1);
            for (int mode : jniExposureModes) {
                if (mode == CameraMetadata.CONTROL_AE_MODE_ON
                        || mode == CameraMetadata.CONTROL_AE_MODE_ON_AUTO_FLASH
                        || mode == CameraMetadata.CONTROL_AE_MODE_ON_ALWAYS_FLASH
                        || mode == CameraMetadata.CONTROL_AE_MODE_ON_AUTO_FLASH_REDEYE) {
                    exposureModes.add(Integer.valueOf(AndroidMeteringMode.CONTINUOUS));
                    break;
                } else {
                    // Exposure mode is Manual. Here we can set exposure time.
                    // All exposure time values from Android are in nano seconds.
                    // Spec (https://w3c.github.io/mediacapture-image/#exposure-time)
                    // expects exposureTime to be in 100 microsecond units.
                    // A value of 1.0 means an exposure time of 1/10000th of a second
                    // and a value of 10000.0 means an exposure time of 1 second.
                    if (cameraCharacteristics.get(
                                    CameraCharacteristics.SENSOR_INFO_EXPOSURE_TIME_RANGE)
                            != null) {
                        // The minimum exposure time will be less than 100 micro-seconds.
                        // For FULL capability devices (android.info.supportedHardwareLevel ==
                        // FULL), the maximum exposure time will be greater than 100 millisecond.
                        Range<Long> range =
                                cameraCharacteristics.get(
                                        CameraCharacteristics.SENSOR_INFO_EXPOSURE_TIME_RANGE);
                        long minExposureTime = range.getLower();
                        long maxExposureTime = range.getUpper();

                        if (minExposureTime != 0 && maxExposureTime != 0) {
                            builder.setDouble(
                                            PhotoCapabilityDouble.MAX_EXPOSURE_TIME,
                                            maxExposureTime / kNanosecondsPer100Microsecond)
                                    .setDouble(
                                            PhotoCapabilityDouble.MIN_EXPOSURE_TIME,
                                            minExposureTime / kNanosecondsPer100Microsecond);
                        }
                        // Smallest step by which exposure time can be changed. This value is not
                        // exposed by Android.
                        builder.setDouble(
                                        PhotoCapabilityDouble.STEP_EXPOSURE_TIME,
                                        10000.0 / kNanosecondsPer100Microsecond)
                                .setDouble(
                                        PhotoCapabilityDouble.CURRENT_EXPOSURE_TIME,
                                        mLastExposureTimeNs / kNanosecondsPer100Microsecond);
                    }
                }
            }
            try {
                Boolean ae_lock_available =
                        cameraCharacteristics.get(CameraCharacteristics.CONTROL_AE_LOCK_AVAILABLE);
                if (ae_lock_available != null && ae_lock_available.booleanValue()) {
                    exposureModes.add(Integer.valueOf(AndroidMeteringMode.FIXED));
                }
            } catch (NoSuchFieldError e) {
                // Ignore this exception, it means CONTROL_AE_LOCK_AVAILABLE is not known.
            }
            builder.setMeteringModeArray(
                    MeteringModeType.EXPOSURE, integerArrayListToArray(exposureModes));

            int jniExposureMode = AndroidMeteringMode.CONTINUOUS;
            if ((mPreviewRequest.get(CaptureRequest.CONTROL_AE_MODE) != null)
                    && mPreviewRequest.get(CaptureRequest.CONTROL_AE_MODE)
                            == CameraMetadata.CONTROL_AE_MODE_OFF) {
                jniExposureMode = AndroidMeteringMode.NONE;
            }
            if (mPreviewRequest.get(CaptureRequest.CONTROL_AE_LOCK) != null
                    && mPreviewRequest.get(CaptureRequest.CONTROL_AE_LOCK)) {
                jniExposureMode = AndroidMeteringMode.FIXED;
            }
            builder.setMeteringMode(MeteringModeType.EXPOSURE, jniExposureMode);

            final float step =
                    cameraCharacteristics
                            .get(CameraCharacteristics.CONTROL_AE_COMPENSATION_STEP)
                            .floatValue();
            builder.setDouble(PhotoCapabilityDouble.STEP_EXPOSURE_COMPENSATION, step);
            final Range<Integer> exposureCompensationRange =
                    cameraCharacteristics.get(CameraCharacteristics.CONTROL_AE_COMPENSATION_RANGE);
            builder.setDouble(
                            PhotoCapabilityDouble.MIN_EXPOSURE_COMPENSATION,
                            exposureCompensationRange.getLower() * step)
                    .setDouble(
                            PhotoCapabilityDouble.MAX_EXPOSURE_COMPENSATION,
                            exposureCompensationRange.getUpper() * step);
            if (mPreviewRequest.get(CaptureRequest.CONTROL_AE_EXPOSURE_COMPENSATION) != null) {
                builder.setDouble(
                        PhotoCapabilityDouble.CURRENT_EXPOSURE_COMPENSATION,
                        mPreviewRequest.get(CaptureRequest.CONTROL_AE_EXPOSURE_COMPENSATION)
                                * step);
            }

            final int[] jniWhiteBalanceMode =
                    cameraCharacteristics.get(CameraCharacteristics.CONTROL_AWB_AVAILABLE_MODES);
            ArrayList<Integer> whiteBalanceModes = new ArrayList<Integer>(1);
            for (int mode : jniWhiteBalanceMode) {
                if (mode == CameraMetadata.CONTROL_AWB_MODE_AUTO) {
                    whiteBalanceModes.add(Integer.valueOf(AndroidMeteringMode.CONTINUOUS));
                    break;
                }
            }
            try {
                Boolean awb_lock_available =
                        cameraCharacteristics.get(CameraCharacteristics.CONTROL_AWB_LOCK_AVAILABLE);
                if (awb_lock_available != null && awb_lock_available.booleanValue()) {
                    whiteBalanceModes.add(Integer.valueOf(AndroidMeteringMode.FIXED));
                }
            } catch (NoSuchFieldError e) {
                // Ignore this exception, it means CONTROL_AWB_LOCK_AVAILABLE is not known.
            }
            builder.setMeteringModeArray(
                    MeteringModeType.WHITE_BALANCE, integerArrayListToArray(whiteBalanceModes));

            int whiteBalanceMode = CameraMetadata.CONTROL_AWB_MODE_AUTO;
            if (mPreviewRequest.get(CaptureRequest.CONTROL_AWB_MODE) != null) {
                whiteBalanceMode = mPreviewRequest.get(CaptureRequest.CONTROL_AWB_MODE);
                if (whiteBalanceMode == CameraMetadata.CONTROL_AWB_MODE_OFF) {
                    builder.setMeteringMode(
                            MeteringModeType.WHITE_BALANCE, AndroidMeteringMode.NONE);
                } else {
                    builder.setMeteringMode(
                            MeteringModeType.WHITE_BALANCE,
                            whiteBalanceMode == CameraMetadata.CONTROL_AWB_MODE_AUTO
                                    ? AndroidMeteringMode.CONTINUOUS
                                    : AndroidMeteringMode.FIXED);
                }
            }
            builder.setInt(
                            PhotoCapabilityInt.MIN_COLOR_TEMPERATURE,
                            COLOR_TEMPERATURES_MAP.keyAt(0))
                    .setInt(
                            PhotoCapabilityInt.MAX_COLOR_TEMPERATURE,
                            COLOR_TEMPERATURES_MAP.keyAt(COLOR_TEMPERATURES_MAP.size() - 1))
                    .setInt(PhotoCapabilityInt.STEP_COLOR_TEMPERATURE, 50);
            final int index = COLOR_TEMPERATURES_MAP.indexOfValue(whiteBalanceMode);
            if (index >= 0) {
                builder.setInt(
                        PhotoCapabilityInt.CURRENT_COLOR_TEMPERATURE,
                        COLOR_TEMPERATURES_MAP.keyAt(index));
            }

            if (!cameraCharacteristics.get(CameraCharacteristics.FLASH_INFO_AVAILABLE)) {
                builder.setBool(PhotoCapabilityBool.SUPPORTS_TORCH, false)
                        .setBool(PhotoCapabilityBool.RED_EYE_REDUCTION, false);
            } else {
                // There's no way to query if torch and/or red eye reduction modes are available
                // using Camera2 API but since there's a Flash unit, we assume so.
                builder.setBool(PhotoCapabilityBool.SUPPORTS_TORCH, true)
                        .setBool(PhotoCapabilityBool.RED_EYE_REDUCTION, true);

                if (mPreviewRequest.get(CaptureRequest.FLASH_MODE) != null) {
                    builder.setBool(
                            PhotoCapabilityBool.TORCH,
                            mPreviewRequest.get(CaptureRequest.FLASH_MODE)
                                    == CameraMetadata.FLASH_MODE_TORCH);
                }

                final int[] flashModes =
                        cameraCharacteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_MODES);
                ArrayList<Integer> modes = new ArrayList<Integer>(0);
                for (int flashMode : flashModes) {
                    if (flashMode == CameraMetadata.FLASH_MODE_OFF) {
                        modes.add(Integer.valueOf(AndroidFillLightMode.OFF));
                    } else if (flashMode == CameraMetadata.CONTROL_AE_MODE_ON_AUTO_FLASH) {
                        modes.add(Integer.valueOf(AndroidFillLightMode.AUTO));
                    } else if (flashMode == CameraMetadata.CONTROL_AE_MODE_ON_ALWAYS_FLASH) {
                        modes.add(Integer.valueOf(AndroidFillLightMode.FLASH));
                    }
                }
                builder.setFillLightModeArray(integerArrayListToArray(modes));
            }

            onGetPhotoCapabilitiesReply(VideoCaptureCamera2.this, mCallbackId, builder.build());
        }
    }

    private static class PhotoOptions {
        public final double zoom;
        public final int focusMode;
        public final double currentFocusDistance;
        public final int exposureMode;
        public final double width;
        public final double height;
        public final double[] pointsOfInterest2D;
        public final boolean hasExposureCompensation;
        public final double exposureCompensation;
        public final double exposureTime;
        public final int whiteBalanceMode;
        public final double iso;
        public final boolean hasRedEyeReduction;
        public final boolean redEyeReduction;
        public final int fillLightMode;
        public final boolean hasTorch;
        public final boolean torch;
        public final double colorTemperature;

        public PhotoOptions(
                double zoom,
                int focusMode,
                double currentFocusDistance,
                int exposureMode,
                double width,
                double height,
                double[] pointsOfInterest2D,
                boolean hasExposureCompensation,
                double exposureCompensation,
                double exposureTime,
                int whiteBalanceMode,
                double iso,
                boolean hasRedEyeReduction,
                boolean redEyeReduction,
                int fillLightMode,
                boolean hasTorch,
                boolean torch,
                double colorTemperature) {
            this.zoom = zoom;
            this.focusMode = focusMode;
            this.currentFocusDistance = currentFocusDistance;
            this.exposureMode = exposureMode;
            this.width = width;
            this.height = height;
            this.pointsOfInterest2D = pointsOfInterest2D;
            this.hasExposureCompensation = hasExposureCompensation;
            this.exposureCompensation = exposureCompensation;
            this.exposureTime = exposureTime;
            this.whiteBalanceMode = whiteBalanceMode;
            this.iso = iso;
            this.hasRedEyeReduction = hasRedEyeReduction;
            this.redEyeReduction = redEyeReduction;
            this.fillLightMode = fillLightMode;
            this.hasTorch = hasTorch;
            this.torch = torch;
            this.colorTemperature = colorTemperature;
        }
    }

    private class SetPhotoOptionsTask implements Runnable {
        private final PhotoOptions mOptions;

        public SetPhotoOptionsTask(PhotoOptions options) {
            mOptions = options;
        }

        @Override
        public void run() {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

            final CameraCharacteristics cameraCharacteristics = getCameraCharacteristics(mId);
            if (cameraCharacteristics == null) return;
            final Rect canvas =
                    cameraCharacteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);

            if (mOptions.zoom != 0) {
                final float normalizedZoom =
                        Math.max(1.0f, Math.min((float) mOptions.zoom, mMaxZoom));
                final float cropFactor = (normalizedZoom - 1) / (2 * normalizedZoom);

                mCropRect =
                        new Rect(
                                Math.round(canvas.width() * cropFactor),
                                Math.round(canvas.height() * cropFactor),
                                Math.round(canvas.width() * (1 - cropFactor)),
                                Math.round(canvas.height() * (1 - cropFactor)));
                Log.d(TAG, "zoom level %f, rectangle: %s", normalizedZoom, mCropRect.toString());
            }

            if (mOptions.focusMode != AndroidMeteringMode.NOT_SET) mFocusMode = mOptions.focusMode;
            if (mOptions.currentFocusDistance != 0) {
                mCurrentFocusDistance = (float) mOptions.currentFocusDistance;
            }
            if (mOptions.exposureMode != AndroidMeteringMode.NOT_SET) {
                mExposureMode = mOptions.exposureMode;
            }
            if (mOptions.exposureTime != 0) {
                // The web API (https://w3c.github.io/mediacapture-image/#exposure-time) provides
                // exposureTime in 100 microsecond units.
                mLastExposureTimeNs =
                        (long) (mOptions.exposureTime * kNanosecondsPer100Microsecond);
            }
            if (mOptions.whiteBalanceMode != AndroidMeteringMode.NOT_SET) {
                mWhiteBalanceMode = mOptions.whiteBalanceMode;
            }
            if (mOptions.width > 0) mPhotoWidth = (int) Math.round(mOptions.width);
            if (mOptions.height > 0) mPhotoHeight = (int) Math.round(mOptions.height);

            // Upon new |zoom| configuration, clear up the previous |mAreaOfInterest| if any.
            if (mAreaOfInterest != null
                    && !mAreaOfInterest.getRect().isEmpty()
                    && mOptions.zoom > 0) {
                mAreaOfInterest = null;
            }
            // Also clear |mAreaOfInterest| if the user sets it as NONE.
            if (mFocusMode == AndroidMeteringMode.NONE
                    || mExposureMode == AndroidMeteringMode.NONE) {
                mAreaOfInterest = null;
            }
            // Update |mAreaOfInterest| if the camera supports and there are |pointsOfInterest2D|.
            final boolean pointsOfInterestSupported =
                    cameraCharacteristics.get(CameraCharacteristics.CONTROL_MAX_REGIONS_AF) > 0
                            || cameraCharacteristics.get(
                                            CameraCharacteristics.CONTROL_MAX_REGIONS_AE)
                                    > 0
                            || cameraCharacteristics.get(
                                            CameraCharacteristics.CONTROL_MAX_REGIONS_AWB)
                                    > 0;
            if (pointsOfInterestSupported && mOptions.pointsOfInterest2D.length > 0) {
                assert mOptions.pointsOfInterest2D.length == 2
                        : "Only 1 point of interest supported";
                assert mOptions.pointsOfInterest2D[0] <= 1.0
                        && mOptions.pointsOfInterest2D[0] >= 0.0;
                assert mOptions.pointsOfInterest2D[1] <= 1.0
                        && mOptions.pointsOfInterest2D[1] >= 0.0;
                // Calculate a Rect of 1/8 the |visibleRect| dimensions, and center it w.r.t.
                // |canvas|.
                final Rect visibleRect = mCropRect.isEmpty() ? canvas : mCropRect;
                int centerX =
                        (int) Math.round(mOptions.pointsOfInterest2D[0] * visibleRect.width());
                int centerY =
                        (int) Math.round(mOptions.pointsOfInterest2D[1] * visibleRect.height());
                if (visibleRect.equals(mCropRect)) {
                    centerX += (canvas.width() - visibleRect.width()) / 2;
                    centerY += (canvas.height() - visibleRect.height()) / 2;
                }
                final int regionWidth = visibleRect.width() / 8;
                final int regionHeight = visibleRect.height() / 8;

                mAreaOfInterest =
                        new MeteringRectangle(
                                Math.max(0, centerX - regionWidth / 2),
                                Math.max(0, centerY - regionHeight / 2),
                                regionWidth,
                                regionHeight,
                                MeteringRectangle.METERING_WEIGHT_MAX);

                Log.d(
                        TAG,
                        "Calculating (%.2fx%.2f) wrt to %s (canvas being %s)",
                        mOptions.pointsOfInterest2D[0],
                        mOptions.pointsOfInterest2D[1],
                        visibleRect.toString(),
                        canvas.toString());
                Log.d(TAG, "Area of interest %s", mAreaOfInterest.toString());
            }

            if (mOptions.hasExposureCompensation) {
                float aeCompensationStep =
                        cameraCharacteristics
                                .get(CameraCharacteristics.CONTROL_AE_COMPENSATION_STEP)
                                .floatValue();
                mExposureCompensation =
                        (int) Math.round(mOptions.exposureCompensation / aeCompensationStep);
            }
            if (mOptions.iso > 0) mIso = (int) Math.round(mOptions.iso);
            if (mOptions.colorTemperature > 0) {
                mColorTemperature = (int) Math.round(mOptions.colorTemperature);
            }
            if (mOptions.hasRedEyeReduction) mRedEyeReduction = mOptions.redEyeReduction;
            if (mOptions.fillLightMode != AndroidFillLightMode.NOT_SET) {
                mFillLightMode = mOptions.fillLightMode;
            }
            if (mOptions.hasTorch) mTorch = mOptions.torch;

            if (mPreviewSession != null) {
                assert mPreviewRequestBuilder != null : "preview request builder";

                // Reuse most of |mPreviewRequestBuilder| since it has expensive items inside that
                // have to do with preview, e.g. the ImageReader and its associated Surface.
                configureCommonCaptureSettings(mPreviewRequestBuilder);

                if (mOptions.fillLightMode != AndroidFillLightMode.NOT_SET) {
                    // Run the precapture sequence for capturing a still image.
                    mPreviewRequestBuilder.set(
                            CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                            CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START);
                }

                mPreviewRequest = mPreviewRequestBuilder.build();

                try {
                    mPreviewSession.setRepeatingRequest(mPreviewRequest, null, null);
                } catch (CameraAccessException
                        | SecurityException
                        | IllegalStateException
                        | IllegalArgumentException ex) {
                    Log.e(TAG, "setRepeatingRequest: ", ex);
                }
            }
        }
    }

    private class TakePhotoTask implements Runnable {
        private final long mCallbackId;

        public TakePhotoTask(long callbackId) {
            mCallbackId = callbackId;
        }

        @Override
        public void run() {
            assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";
            TraceEvent.instant("VideoCaptureCamera2.java", "TakePhotoTask.run");

            if (mCameraDevice == null || mCameraState != CameraState.STARTED) {
                Log.e(
                        TAG,
                        "TakePhoto failed because mCameraDevice == null || "
                                + "mCameraState != CameraState.STARTED");
                notifyTakePhotoError(mCallbackId);
                return;
            }

            final CameraCharacteristics cameraCharacteristics = getCameraCharacteristics(mId);
            if (cameraCharacteristics == null) {
                Log.e(TAG, "cameraCharacteristics error");
                notifyTakePhotoError(mCallbackId);
                return;
            }
            final StreamConfigurationMap streamMap =
                    cameraCharacteristics.get(
                            CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            final Size[] supportedSizes = streamMap.getOutputSizes(ImageFormat.JPEG);
            final Size closestSize =
                    findClosestSizeInArray(supportedSizes, mPhotoWidth, mPhotoHeight);

            Log.d(TAG, "requested resolution: (%dx%d)", mPhotoWidth, mPhotoHeight);
            if (closestSize != null) {
                Log.d(TAG, " matched (%dx%d)", closestSize.getWidth(), closestSize.getHeight());
            }
            TraceEvent.instant(
                    "VideoCaptureCamera2.java", "TakePhotoTask.run creating ImageReader");
            final ImageReader imageReader =
                    ImageReader.newInstance(
                            (closestSize != null)
                                    ? closestSize.getWidth()
                                    : mCaptureFormat.getWidth(),
                            (closestSize != null)
                                    ? closestSize.getHeight()
                                    : mCaptureFormat.getHeight(),
                            ImageFormat.JPEG,
                            /* maxImages= */ 1);

            final CrPhotoReaderListener photoReaderListener =
                    new CrPhotoReaderListener(mCallbackId);
            imageReader.setOnImageAvailableListener(photoReaderListener, mCameraThreadHandler);

            final List<Surface> surfaceList = new ArrayList<Surface>(1);
            // TODO(mcasas): release this Surface when not needed, https://crbug.com/643884.
            surfaceList.add(imageReader.getSurface());

            CaptureRequest.Builder photoRequestBuilder = null;
            try {
                photoRequestBuilder =
                        mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
            } catch (CameraAccessException ex) {
                Log.e(TAG, "createCaptureRequest() error ", ex);
                notifyTakePhotoError(mCallbackId);
                return;
            }
            if (photoRequestBuilder == null) {
                Log.e(TAG, "photoRequestBuilder error");
                notifyTakePhotoError(mCallbackId);
                return;
            }
            photoRequestBuilder.addTarget(imageReader.getSurface());
            photoRequestBuilder.set(CaptureRequest.JPEG_ORIENTATION, getCameraRotation());

            TraceEvent.instant(
                    "VideoCaptureCamera2.java",
                    "TakePhotoTask.run calling configureCommonCaptureSettings");
            configureCommonCaptureSettings(photoRequestBuilder);

            TraceEvent.instant(
                    "VideoCaptureCamera2.java",
                    "TakePhotoTask.run calling photoRequestBuilder.build()");
            final CaptureRequest photoRequest = photoRequestBuilder.build();
            final CrPhotoSessionListener sessionListener =
                    new CrPhotoSessionListener(imageReader, photoRequest, mCallbackId);
            try {
                TraceEvent.instant(
                        "VideoCaptureCamera2.java",
                        "TakePhotoTask.run calling mCameraDevice.createCaptureSession()");
                mCameraDevice.createCaptureSession(
                        surfaceList, sessionListener, mCameraThreadHandler);
            } catch (CameraAccessException | IllegalArgumentException | SecurityException ex) {
                Log.e(TAG, "createCaptureSession: " + ex);
                notifyTakePhotoError(mCallbackId);
            }
        }
    }

    private static final double kNanosecondsPerSecond = 1000000000;
    private static final double kNanosecondsPer100Microsecond = 100000;
    private static final String TAG = "VideoCapture";

    private static final String[] AE_TARGET_FPS_RANGE_BUGGY_DEVICE_LIST = {
        // See https://crbug.com/913203 for more info.
        "Pixel 3", "Pixel 3 XL",
    };

    // Map of the equivalent color temperature in Kelvin for the White Balance setting. The
    // values are a mixture of educated guesses and data from Android's Camera2 API. The
    // temperatures must be ordered increasingly.
    private static final SparseIntArray COLOR_TEMPERATURES_MAP;

    static {
        COLOR_TEMPERATURES_MAP = new SparseIntArray();
        COLOR_TEMPERATURES_MAP.append(2850, CameraMetadata.CONTROL_AWB_MODE_INCANDESCENT);
        COLOR_TEMPERATURES_MAP.append(2950, CameraMetadata.CONTROL_AWB_MODE_WARM_FLUORESCENT);
        COLOR_TEMPERATURES_MAP.append(4250, CameraMetadata.CONTROL_AWB_MODE_FLUORESCENT);
        COLOR_TEMPERATURES_MAP.append(4600, CameraMetadata.CONTROL_AWB_MODE_TWILIGHT);
        COLOR_TEMPERATURES_MAP.append(5000, CameraMetadata.CONTROL_AWB_MODE_DAYLIGHT);
        COLOR_TEMPERATURES_MAP.append(6000, CameraMetadata.CONTROL_AWB_MODE_CLOUDY_DAYLIGHT);
        COLOR_TEMPERATURES_MAP.append(7000, CameraMetadata.CONTROL_AWB_MODE_SHADE);
    }

    @IntDef({
        CameraState.OPENING,
        CameraState.CONFIGURING,
        CameraState.STARTED,
        CameraState.STOPPED
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface CameraState {
        int OPENING = 0;
        int CONFIGURING = 1;
        int STARTED = 2;
        int STOPPED = 3;
    }

    private final Object mCameraStateLock = new Object();

    private CameraDevice mCameraDevice;
    private CameraCaptureSession mPreviewSession;
    private CaptureRequest mPreviewRequest;
    private CaptureRequest.Builder mPreviewRequestBuilder;
    private ImageReader mImageReader;
    // We create a dedicated HandlerThread for operating the camera on. This
    // is needed, because the camera APIs requires a Looper for posting
    // asynchronous callbacks to. The native thread that calls the constructor
    // and public API cannot be used for this, because it does not have a
    // Looper.
    private Handler mCameraThreadHandler;
    private ConditionVariable mWaitForDeviceClosedConditionVariable = new ConditionVariable();

    private Range<Integer> mAeFpsRange;
    private @CameraState int mCameraState = CameraState.STOPPED;
    private float mMaxZoom = 1.0f;
    private Rect mCropRect = new Rect();
    private int mPhotoWidth;
    private int mPhotoHeight;
    private int mFocusMode = AndroidMeteringMode.CONTINUOUS;
    private float mCurrentFocusDistance = 1.0f;
    private int mExposureMode = AndroidMeteringMode.CONTINUOUS;
    private long mLastExposureTimeNs;
    private MeteringRectangle mAreaOfInterest;
    private int mExposureCompensation;
    private int mWhiteBalanceMode = AndroidMeteringMode.CONTINUOUS;
    private int mColorTemperature = -1;
    private int mIso;
    private boolean mRedEyeReduction;
    private int mFillLightMode = AndroidFillLightMode.OFF;
    private boolean mTorch;
    private boolean mEnableFaceDetection;

    // Service function to grab CameraCharacteristics and handle exceptions.
    private static CameraCharacteristics getCameraCharacteristics(int id) {
        final CameraManager manager =
                (CameraManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CAMERA_SERVICE);
        try {
            final String str_id = String.valueOf(id);
            return manager.getCameraCharacteristics(str_id);
        } catch (CameraAccessException
                | IllegalArgumentException
                | AssertionError
                | NullPointerException
                | SecurityException ex) {
            Log.e(TAG, "getCameraCharacteristics: ", ex);
        }
        return null;
    }

    private void createPreviewObjectsAndStartPreviewOrFailWith(int androidVideoCaptureError) {
        assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";

        if (createPreviewObjectsAndStartPreview()) return;

        changeCameraStateAndNotify(CameraState.STOPPED);
        onError(
                VideoCaptureCamera2.this,
                androidVideoCaptureError,
                "Error starting or restarting preview");
    }

    private boolean createPreviewObjectsAndStartPreview() {
        assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";
        if (mCameraDevice == null) return false;

        try (TraceEvent trace_event =
                TraceEvent.scoped("VideoCaptureCamera2.createPreviewObjectsAndStartPreview")) {
            // Create an ImageReader and plug a thread looper into it to have
            // readback take place on its own thread.
            mImageReader =
                    ImageReader.newInstance(
                            mCaptureFormat.getWidth(),
                            mCaptureFormat.getHeight(),
                            mCaptureFormat.getPixelFormat(),
                            /* maxImages= */ 2);
            final CrPreviewReaderListener imageReaderListener = new CrPreviewReaderListener();
            mImageReader.setOnImageAvailableListener(imageReaderListener, mCameraThreadHandler);

            try {
                // TEMPLATE_PREVIEW specifically means "high frame rate is given
                // priority over the highest-quality post-processing".
                mPreviewRequestBuilder =
                        mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            } catch (CameraAccessException | IllegalArgumentException | SecurityException ex) {
                Log.e(TAG, "createCaptureRequest: ", ex);
                return false;
            }

            if (mPreviewRequestBuilder == null) {
                Log.e(TAG, "mPreviewRequestBuilder error");
                return false;
            }

            // Construct an ImageReader Surface and plug it into our CaptureRequest.Builder.
            mPreviewRequestBuilder.addTarget(mImageReader.getSurface());

            // A series of configuration options in the PreviewBuilder
            mPreviewRequestBuilder.set(
                    CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_AUTO);
            mPreviewRequestBuilder.set(
                    CaptureRequest.NOISE_REDUCTION_MODE, CameraMetadata.NOISE_REDUCTION_MODE_FAST);
            mPreviewRequestBuilder.set(CaptureRequest.EDGE_MODE, CameraMetadata.EDGE_MODE_FAST);

            // Depending on the resolution and other parameters, stabilization might not be
            // available, see https://crbug.com/718387.
            // https://developer.android.com/reference/android/hardware/camera2/CaptureRequest.html#CONTROL_VIDEO_STABILIZATION_MODE
            final CameraCharacteristics cameraCharacteristics = getCameraCharacteristics(mId);
            if (cameraCharacteristics == null) return false;
            final int[] stabilizationModes =
                    cameraCharacteristics.get(
                            CameraCharacteristics.CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES);
            for (int mode : stabilizationModes) {
                if (mode == CameraMetadata.CONTROL_VIDEO_STABILIZATION_MODE_ON) {
                    mPreviewRequestBuilder.set(
                            CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE,
                            CameraMetadata.CONTROL_VIDEO_STABILIZATION_MODE_ON);
                    break;
                }
            }

            configureCommonCaptureSettings(mPreviewRequestBuilder);

            // Overwrite settings to enable face detection.
            if (mEnableFaceDetection) {
                mPreviewRequestBuilder.set(
                        CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_USE_SCENE_MODE);
                mPreviewRequestBuilder.set(
                        CaptureRequest.CONTROL_SCENE_MODE,
                        CameraMetadata.CONTROL_SCENE_MODE_FACE_PRIORITY);
            }

            List<Surface> surfaceList = new ArrayList<Surface>(1);
            // TODO(mcasas): release this Surface when not needed, https://crbug.com/643884.
            surfaceList.add(mImageReader.getSurface());

            mPreviewRequest = mPreviewRequestBuilder.build();

            try {
                mCameraDevice.createCaptureSession(
                        surfaceList, new CrPreviewSessionListener(mPreviewRequest), null);
            } catch (CameraAccessException | IllegalArgumentException | SecurityException ex) {
                Log.e(TAG, "createCaptureSession: ", ex);
                return false;
            }
            // Wait for trigger on CrPreviewSessionListener.onConfigured();
            return true;
        }
    }

    private void configureCommonCaptureSettings(CaptureRequest.Builder requestBuilder) {
        assert mCameraThreadHandler.getLooper() == Looper.myLooper() : "called on wrong thread";
        try (TraceEvent trace_event =
                TraceEvent.scoped("VideoCaptureCamera2.configureCommonCaptureSettings")) {
            final CameraCharacteristics cameraCharacteristics = getCameraCharacteristics(mId);

            // |mFocusMode| indicates if we're in auto/continuous, single-shot or manual mode.
            // AndroidMeteringMode.SINGLE_SHOT is dealt with independently since it needs to be
            // triggered by a capture.
            if (mFocusMode == AndroidMeteringMode.CONTINUOUS) {
                requestBuilder.set(
                        CaptureRequest.CONTROL_AF_MODE,
                        CameraMetadata.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
                requestBuilder.set(
                        CaptureRequest.CONTROL_AF_TRIGGER, CameraMetadata.CONTROL_AF_TRIGGER_IDLE);
            } else if (mFocusMode == AndroidMeteringMode.FIXED) {
                requestBuilder.set(
                        CaptureRequest.CONTROL_AF_MODE, CameraMetadata.CONTROL_AF_MODE_OFF);
                requestBuilder.set(
                        CaptureRequest.CONTROL_AF_TRIGGER, CameraMetadata.CONTROL_AF_TRIGGER_IDLE);
                requestBuilder.set(CaptureRequest.LENS_FOCUS_DISTANCE, 1 / mCurrentFocusDistance);
            }

            // |mExposureMode|, |mFillLightMode| and |mTorch| interact to configure the AE and Flash
            // modes. In a nutshell, FLASH_MODE is only effective if the auto-exposure is ON/OFF,
            // otherwise the auto-exposure related flash control (ON_{AUTO,ALWAYS}_FLASH{_REDEYE)
            // takes priority.  |mTorch| mode overrides any previous |mFillLightMode| flash control.
            if (mExposureMode == AndroidMeteringMode.NONE
                    || mExposureMode == AndroidMeteringMode.FIXED) {
                requestBuilder.set(
                        CaptureRequest.CONTROL_AE_MODE, CameraMetadata.CONTROL_AE_MODE_OFF);

                // We need to configure by hand the exposure time when AE mode is off.  Set it to
                // the last known exposure interval if known, otherwise set it to the middle of the
                // allowed range. Further tuning will be done via |mIso| and
                // |mExposureCompensation|.
                if (mLastExposureTimeNs != 0) {
                    requestBuilder.set(CaptureRequest.SENSOR_EXPOSURE_TIME, mLastExposureTimeNs);
                } else if (cameraCharacteristics != null) {
                    Range<Long> range =
                            cameraCharacteristics.get(
                                    CameraCharacteristics.SENSOR_INFO_EXPOSURE_TIME_RANGE);
                    requestBuilder.set(
                            CaptureRequest.SENSOR_EXPOSURE_TIME,
                            range.getLower() + (range.getUpper() + range.getLower()) / 2);
                }

            } else {
                requestBuilder.set(CaptureRequest.CONTROL_MODE, CaptureRequest.CONTROL_MODE_AUTO);
                requestBuilder.set(
                        CaptureRequest.CONTROL_AE_MODE, CameraMetadata.CONTROL_AE_MODE_ON);
                if (!shouldSkipSettingAeTargetFpsRange()) {
                    requestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, mAeFpsRange);
                }
            }

            if (mTorch) {
                requestBuilder.set(
                        CaptureRequest.CONTROL_AE_MODE,
                        mExposureMode == AndroidMeteringMode.CONTINUOUS
                                ? CameraMetadata.CONTROL_AE_MODE_ON
                                : CameraMetadata.CONTROL_AE_MODE_OFF);
                requestBuilder.set(CaptureRequest.FLASH_MODE, CameraMetadata.FLASH_MODE_TORCH);
            } else {
                switch (mFillLightMode) {
                    case AndroidFillLightMode.OFF:
                        requestBuilder.set(
                                CaptureRequest.FLASH_MODE, CameraMetadata.FLASH_MODE_OFF);
                        break;
                    case AndroidFillLightMode.AUTO:
                        // Setting the AE to CONTROL_AE_MODE_ON_AUTO_FLASH[_REDEYE] overrides
                        // FLASH_MODE.
                        requestBuilder.set(
                                CaptureRequest.CONTROL_AE_MODE,
                                mRedEyeReduction
                                        ? CameraMetadata.CONTROL_AE_MODE_ON_AUTO_FLASH_REDEYE
                                        : CameraMetadata.CONTROL_AE_MODE_ON_AUTO_FLASH);
                        break;
                    case AndroidFillLightMode.FLASH:
                        // Setting the AE to CONTROL_AE_MODE_ON_ALWAYS_FLASH overrides FLASH_MODE.
                        requestBuilder.set(
                                CaptureRequest.CONTROL_AE_MODE,
                                CameraMetadata.CONTROL_AE_MODE_ON_ALWAYS_FLASH);
                        requestBuilder.set(
                                CaptureRequest.FLASH_MODE, CameraMetadata.FLASH_MODE_SINGLE);
                        break;
                    default:
                }
                requestBuilder.set(
                        CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                        CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_IDLE);
            }

            requestBuilder.set(
                    CaptureRequest.CONTROL_AE_EXPOSURE_COMPENSATION, mExposureCompensation);

            // White Balance mode AndroidMeteringMode.SINGLE_SHOT is not supported.
            if (mWhiteBalanceMode == AndroidMeteringMode.CONTINUOUS) {
                requestBuilder.set(CaptureRequest.CONTROL_AWB_LOCK, false);
                requestBuilder.set(
                        CaptureRequest.CONTROL_AWB_MODE, CameraMetadata.CONTROL_AWB_MODE_AUTO);
                // TODO(mcasas): support different luminant color temperatures, e.g. DAYLIGHT,
                // SHADE. https://crbug.com/518807
            } else if (mWhiteBalanceMode == AndroidMeteringMode.NONE) {
                requestBuilder.set(CaptureRequest.CONTROL_AWB_LOCK, false);
                requestBuilder.set(
                        CaptureRequest.CONTROL_AWB_MODE, CameraMetadata.CONTROL_AWB_MODE_OFF);
            } else if (mWhiteBalanceMode == AndroidMeteringMode.FIXED) {
                requestBuilder.set(CaptureRequest.CONTROL_AWB_LOCK, true);
            }
            if (mColorTemperature > 0) {
                int colorSetting = -1;
                if (cameraCharacteristics != null) {
                    colorSetting =
                            getClosestWhiteBalance(
                                    mColorTemperature,
                                    cameraCharacteristics.get(
                                            CameraCharacteristics.CONTROL_AWB_AVAILABLE_MODES));
                }
                Log.d(TAG, " Color temperature (%d ==> %d)", mColorTemperature, colorSetting);
                if (colorSetting != -1) {
                    requestBuilder.set(CaptureRequest.CONTROL_AWB_MODE, colorSetting);
                }
            }

            if (mAreaOfInterest != null) {
                MeteringRectangle[] array = {mAreaOfInterest};
                Log.d(TAG, "Area of interest %s", mAreaOfInterest.toString());
                requestBuilder.set(CaptureRequest.CONTROL_AF_REGIONS, array);
                requestBuilder.set(CaptureRequest.CONTROL_AE_REGIONS, array);
                requestBuilder.set(CaptureRequest.CONTROL_AWB_REGIONS, array);
            }

            if (!mCropRect.isEmpty()) {
                requestBuilder.set(CaptureRequest.SCALER_CROP_REGION, mCropRect);
            }

            if (mIso > 0) requestBuilder.set(CaptureRequest.SENSOR_SENSITIVITY, mIso);
        }
    }

    private void changeCameraStateAndNotify(@CameraState int state) {
        synchronized (mCameraStateLock) {
            mCameraState = state;
            mCameraStateLock.notifyAll();
        }
    }

    private static boolean shouldSkipSettingAeTargetFpsRange() {
        for (String buggyDevice : AE_TARGET_FPS_RANGE_BUGGY_DEVICE_LIST) {
            if (buggyDevice.contentEquals(android.os.Build.MODEL)) {
                return true;
            }
        }
        return false;
    }

    // Finds the closest Size to (|width|x|height|) in |sizes|, and returns it or null.
    // Ignores |width| or |height| if either is zero (== don't care).
    private static Size findClosestSizeInArray(Size[] sizes, int width, int height) {
        if (sizes == null) return null;
        Size closestSize = null;
        int minDiff = Integer.MAX_VALUE;
        for (Size size : sizes) {
            final int diff =
                    ((width > 0) ? Math.abs(size.getWidth() - width) : 0)
                            + ((height > 0) ? Math.abs(size.getHeight() - height) : 0);
            if (diff < minDiff) {
                minDiff = diff;
                closestSize = size;
            }
        }
        if (minDiff == Integer.MAX_VALUE) {
            Log.e(TAG, "Couldn't find resolution close to (%dx%d)", width, height);
            return null;
        }
        return closestSize;
    }

    private static int findInIntArray(int[] hayStack, int needle) {
        for (int i = 0; i < hayStack.length; ++i) {
            if (needle == hayStack[i]) return i;
        }
        return -1;
    }

    private static int getClosestWhiteBalance(int colorTemperature, int[] supportedTemperatures) {
        int minDiff = Integer.MAX_VALUE;
        int matchedTemperature = -1;

        for (int i = 0; i < COLOR_TEMPERATURES_MAP.size(); ++i) {
            if (findInIntArray(supportedTemperatures, COLOR_TEMPERATURES_MAP.valueAt(i)) == -1) {
                continue;
            }
            final int diff = Math.abs(colorTemperature - COLOR_TEMPERATURES_MAP.keyAt(i));
            if (diff >= minDiff) continue;
            minDiff = diff;
            matchedTemperature = COLOR_TEMPERATURES_MAP.valueAt(i);
        }
        return matchedTemperature;
    }

    public static boolean isLegacyDevice(int id) {
        final CameraCharacteristics cameraCharacteristics = getCameraCharacteristics(id);
        return cameraCharacteristics != null
                && cameraCharacteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL)
                        == CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY;
    }

    public static int getNumberOfCameras() {
        CameraManager manager = null;
        try {
            manager =
                    (CameraManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.CAMERA_SERVICE);
        } catch (IllegalArgumentException ex) {
            Log.e(TAG, "getSystemService(Context.CAMERA_SERVICE): ", ex);
            return 0;
        }
        if (manager == null) return 0;
        try {
            return manager.getCameraIdList().length;
        } catch (CameraAccessException | SecurityException | AssertionError ex) {
            // SecurityException is undocumented but seen in the wild: https://crbug/605424.
            Log.e(TAG, "getNumberOfCameras: getCameraIdList(): ", ex);
            return 0;
        }
    }

    public static int getCaptureApiType(int index) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(getDeviceIdInt(index));
        if (cameraCharacteristics == null) {
            return VideoCaptureApi.UNKNOWN;
        }

        final int supportedHWLevel =
                cameraCharacteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);

        // https://crbug.com/1155568: We must explicitly check for
        // BACKWARD_COMPATIBLE, except for LEGACY, where it's implied. See also
        // https://developer.android.com/reference/android/hardware/camera2/CameraMetadata#INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY
        if (supportedHWLevel == CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY) {
            return VideoCaptureApi.ANDROID_API2_LEGACY;
        }
        final int[] capabilities =
                cameraCharacteristics.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
        boolean backwardCompatible = false;
        for (int cap : capabilities) {
            if (cap == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE) {
                backwardCompatible = true;
                break;
            }
        }
        if (!backwardCompatible) {
            return VideoCaptureApi.UNKNOWN;
        }

        switch (supportedHWLevel) {
            case CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_FULL:
                return VideoCaptureApi.ANDROID_API2_FULL;
            case CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED:
                return VideoCaptureApi.ANDROID_API2_LIMITED;
            default:
                return VideoCaptureApi.ANDROID_API2_LEGACY;
        }
    }

    public static boolean isZoomSupported(int index) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(getDeviceIdInt(index));
        if (cameraCharacteristics == null) {
            return false;
        }

        final float maxZoom =
                cameraCharacteristics.get(CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
        final boolean isZoomSupported = maxZoom > 1.0f;
        return isZoomSupported;
    }

    public static int getFacingMode(int index) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(getDeviceIdInt(index));
        if (cameraCharacteristics == null) {
            return VideoFacingMode.MEDIA_VIDEO_FACING_NONE;
        }

        final int facing = cameraCharacteristics.get(CameraCharacteristics.LENS_FACING);
        switch (facing) {
            case CameraCharacteristics.LENS_FACING_FRONT:
                return VideoFacingMode.MEDIA_VIDEO_FACING_USER;
            case CameraCharacteristics.LENS_FACING_BACK:
                return VideoFacingMode.MEDIA_VIDEO_FACING_ENVIRONMENT;
            default:
                return VideoFacingMode.MEDIA_VIDEO_FACING_NONE;
        }
    }

    public static String getName(int index) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(getDeviceIdInt(index));
        if (cameraCharacteristics == null) return null;
        final int facing = cameraCharacteristics.get(CameraCharacteristics.LENS_FACING);
        String displayFacing = "unknown";
        switch (facing) {
            case CameraCharacteristics.LENS_FACING_FRONT:
                displayFacing = "front";
                break;
            case CameraCharacteristics.LENS_FACING_BACK:
                displayFacing = "back";
                break;
            case CameraCharacteristics.LENS_FACING_EXTERNAL:
                displayFacing = "external";
                break;
            default:
                assert false : "Unexpected facing";
        }

        boolean isInfrared = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            final Integer infoColor =
                    cameraCharacteristics.get(
                            CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT);
            isInfrared =
                    infoColor != null
                            && infoColor.equals(
                                    CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_NIR);
        }
        return "camera2 " + index + ", facing " + displayFacing + (isInfrared ? " infrared" : "");
    }

    // Retrieves the index within the camera ID list for the specified camera ID; returns
    // -1 if the specified camera ID is not found
    public static int getDeviceIndex(int id) {
        final CameraManager manager =
                (CameraManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CAMERA_SERVICE);
        try {
            final String[] cameraIdList = manager.getCameraIdList();
            for (int index = 0; index < cameraIdList.length; ++index) {
                try {
                    if (Integer.parseInt(cameraIdList[index]) == id) {
                        return index;
                    }
                } catch (NumberFormatException e) {
                    continue;
                }
            }
        } catch (CameraAccessException ex) {
            Log.e(TAG, "manager.getCameraIdList: ", ex);
        }
        return -1;
    }

    // Helper to retrieve the camera device ID, as an integer, at the specified
    // index within the camera ID list; returns -1 if camera does not exist at the
    // specified index
    private static int getDeviceIdInt(int index) {
        try {
            return Integer.parseInt(getDeviceId(index));
        } catch (NumberFormatException ex) {
            Log.e(TAG, "Invalid camera index: ", index);
            return -1;
        }
    }

    static String getDeviceId(int index) {
        final CameraManager manager =
                (CameraManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CAMERA_SERVICE);
        try {
            final String[] cameraIdList = manager.getCameraIdList();
            if (index >= cameraIdList.length) {
                Log.e(TAG, "Invalid camera index: ", index);
                return null;
            }
            return cameraIdList[index];
        } catch (CameraAccessException ex) {
            Log.e(TAG, "manager.getCameraIdList: ", ex);
            return null;
        }
    }

    public static VideoCaptureFormat[] getDeviceSupportedFormats(int index) {
        final CameraCharacteristics cameraCharacteristics =
                getCameraCharacteristics(getDeviceIdInt(index));
        if (cameraCharacteristics == null) return null;

        try {
            final int[] capabilities =
                    cameraCharacteristics.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            // Per-format frame rate via getOutputMinFrameDuration() is only available if the
            // property REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR is set.
            boolean minFrameDurationAvailable = false;
            for (int cap : capabilities) {
                if (cap == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR) {
                    minFrameDurationAvailable = true;
                    break;
                }
            }

            ArrayList<VideoCaptureFormat> formatList = new ArrayList<VideoCaptureFormat>();
            final StreamConfigurationMap streamMap =
                    cameraCharacteristics.get(
                            CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            final int[] formats = streamMap.getOutputFormats();
            for (int format : formats) {
                final Size[] sizes = streamMap.getOutputSizes(format);
                if (sizes == null) continue;
                for (Size size : sizes) {
                    double minFrameRate = 0.0f;
                    if (minFrameDurationAvailable) {
                        final long minFrameDurationInNanoseconds =
                                streamMap.getOutputMinFrameDuration(format, size);
                        minFrameRate =
                                (minFrameDurationInNanoseconds == 0)
                                        ? 0.0f
                                        : (kNanosecondsPerSecond / minFrameDurationInNanoseconds);
                    } else {
                        // TODO(mcasas): find out where to get the info from in this case.
                        // Hint: perhaps using SCALER_AVAILABLE_PROCESSED_MIN_DURATIONS.
                        minFrameRate = 0.0;
                    }
                    formatList.add(
                            new VideoCaptureFormat(
                                    size.getWidth(), size.getHeight(), (int) minFrameRate, format));
                }
            }
            return formatList.toArray(new VideoCaptureFormat[formatList.size()]);
        } catch (Exception e) {
            Log.e(TAG, "Unable to catch device supported video formats: ", e);
            return null;
        }
    }

    VideoCaptureCamera2(int id, long nativeVideoCaptureDeviceAndroid) {
        super(id, nativeVideoCaptureDeviceAndroid);

        dCheckCurrentlyOnIncomingTaskRunner(VideoCaptureCamera2.this);

        HandlerThread thread = new HandlerThread("VideoCaptureCamera2_CameraThread");
        thread.start();
        mCameraThreadHandler = new Handler(thread.getLooper());

        final CameraCharacteristics cameraCharacteristics = getCameraCharacteristics(id);
        if (cameraCharacteristics != null) {
            mMaxZoom =
                    cameraCharacteristics.get(
                            CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
        }
    }

    @Override
    @SuppressWarnings("Finalize")
    public void finalize() {
        // TODO(crbug.com/40286193): Use an explicit close (or timer-based timeout?) rather than
        // finalize, which
        // discouraged and difficult to ensure actually runs.
        mCameraThreadHandler.getLooper().quit();
    }

    @Override
    public boolean allocate(int width, int height, int frameRate, boolean enableFaceDetection) {
        Log.d(TAG, "allocate: requested (%d x %d) @%dfps", width, height, frameRate);
        dCheckCurrentlyOnIncomingTaskRunner(VideoCaptureCamera2.this);
        synchronized (mCameraStateLock) {
            if (mCameraState == CameraState.OPENING || mCameraState == CameraState.CONFIGURING) {
                Log.e(TAG, "allocate() invoked while Camera is busy opening/configuring.");
                return false;
            }
        }
        final CameraCharacteristics cameraCharacteristics = getCameraCharacteristics(mId);
        if (cameraCharacteristics == null) return false;
        final StreamConfigurationMap streamMap =
                cameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);

        mCameraNativeOrientation =
                cameraCharacteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);

        // Update the capture width and height based on the camera orientation.
        // With device's native orientation being Portrait for Android devices,
        // for cameras that are mounted 0 or 180 degrees in respect to device's
        // native orientation, we will need to swap the width and height in
        // order to capture upright frames in respect to device's current
        // orientation.
        int capture_width = width;
        int capture_height = height;
        if (mCameraNativeOrientation == 0 || mCameraNativeOrientation == 180) {
            Log.d(
                    TAG,
                    "Flipping capture width and height to match device's " + "natural orientation");
            capture_width = height;
            capture_height = width;
        }

        // Find closest supported size.
        final Size[] supportedSizes = streamMap.getOutputSizes(ImageFormat.YUV_420_888);
        final Size closestSupportedSize =
                findClosestSizeInArray(supportedSizes, capture_width, capture_height);
        if (closestSupportedSize == null) {
            Log.e(TAG, "No supported resolutions.");
            return false;
        }
        final List<Range<Integer>> fpsRanges =
                Arrays.asList(
                        cameraCharacteristics.get(
                                CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES));
        if (fpsRanges.isEmpty()) {
            Log.e(TAG, "No supported framerate ranges.");
            return false;
        }
        final List<FramerateRange> framerateRanges =
                new ArrayList<FramerateRange>(fpsRanges.size());
        // On some legacy implementations FPS values are multiplied by 1000. Multiply by 1000
        // everywhere for consistency. Set fpsUnitFactor to 1 if fps ranges are already multiplied
        // by 1000.
        final int fpsUnitFactor = fpsRanges.get(0).getUpper() > 1000 ? 1 : 1000;
        for (Range<Integer> range : fpsRanges) {
            framerateRanges.add(
                    new FramerateRange(
                            range.getLower() * fpsUnitFactor, range.getUpper() * fpsUnitFactor));
        }
        final FramerateRange aeFramerateRange =
                getClosestFramerateRange(framerateRanges, frameRate * 1000);
        mAeFpsRange =
                new Range<Integer>(
                        aeFramerateRange.min / fpsUnitFactor, aeFramerateRange.max / fpsUnitFactor);
        Log.d(
                TAG,
                "allocate: matched (%d x %d) @[%d - %d]",
                closestSupportedSize.getWidth(),
                closestSupportedSize.getHeight(),
                mAeFpsRange.getLower(),
                mAeFpsRange.getUpper());

        // |mCaptureFormat| is also used to configure the ImageReader.
        mCaptureFormat =
                new VideoCaptureFormat(
                        closestSupportedSize.getWidth(),
                        closestSupportedSize.getHeight(),
                        frameRate,
                        ImageFormat.YUV_420_888);

        // TODO(mcasas): The following line is correct for N5 with prerelease Build,
        // but NOT for N7 with a dev Build. Figure out which one to support.
        mInvertDeviceOrientationReadings =
                cameraCharacteristics.get(CameraCharacteristics.LENS_FACING)
                        == CameraCharacteristics.LENS_FACING_BACK;

        mEnableFaceDetection = enableFaceDetection;
        return true;
    }

    @Override
    public boolean startCaptureMaybeAsync() {
        dCheckCurrentlyOnIncomingTaskRunner(VideoCaptureCamera2.this);

        changeCameraStateAndNotify(CameraState.OPENING);
        final CameraManager manager =
                (CameraManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CAMERA_SERVICE);

        final CrStateListener stateListener = new CrStateListener();
        try {
            final String[] cameraIdList = manager.getCameraIdList();
            final int cameraIndex = getDeviceIndex(mId);
            if (cameraIndex < 0) {
                Log.e(TAG, "Invalid camera Id: ", mId);
                return false;
            }
            TraceEvent.instant(
                    "VideoCaptureCamera2.java",
                    "VideoCaptureCamera2.startCaptureMaybeAsync calling manager.openCamera");
            manager.openCamera(cameraIdList[cameraIndex], stateListener, mCameraThreadHandler);
        } catch (CameraAccessException | IllegalArgumentException | SecurityException ex) {
            Log.e(TAG, "allocate: manager.openCamera: ", ex);
            return false;
        }

        return true;
    }

    @Override
    public boolean stopCaptureAndBlockUntilStopped() {
        dCheckCurrentlyOnIncomingTaskRunner(VideoCaptureCamera2.this);
        try (TraceEvent trace_event =
                TraceEvent.scoped("VideoCaptureCamera2.stopCaptureAndBlockUntilStopped")) {
            // With Camera2 API, the capture is started asynchronously, which will cause problem if
            // stopCapture comes too quickly. Without stopping the previous capture properly, the
            // next startCapture will fail and make Chrome no-responding. So wait camera to be
            // STARTED.
            synchronized (mCameraStateLock) {
                while (mCameraState != CameraState.STARTED && mCameraState != CameraState.STOPPED) {
                    try {
                        mCameraStateLock.wait();
                    } catch (InterruptedException ex) {
                        Log.e(TAG, "CaptureStartedEvent: ", ex);
                    }
                }
                if (mCameraState == CameraState.STOPPED) return true;
            }

            mCameraThreadHandler.post(new StopCaptureTask());
            mWaitForDeviceClosedConditionVariable.block();

            return true;
        }
    }

    @Override
    public void getPhotoCapabilitiesAsync(long callbackId) {
        dCheckCurrentlyOnIncomingTaskRunner(VideoCaptureCamera2.this);
        mCameraThreadHandler.post(new GetPhotoCapabilitiesTask(callbackId));
    }

    @Override
    public void setPhotoOptions(
            double zoom,
            int focusMode,
            double currentFocusDistance,
            int exposureMode,
            double width,
            double height,
            double[] pointsOfInterest2D,
            boolean hasExposureCompensation,
            double exposureCompensation,
            double exposureTime,
            int whiteBalanceMode,
            double iso,
            boolean hasRedEyeReduction,
            boolean redEyeReduction,
            int fillLightMode,
            boolean hasTorch,
            boolean torch,
            double colorTemperature) {
        dCheckCurrentlyOnIncomingTaskRunner(VideoCaptureCamera2.this);
        mCameraThreadHandler.post(
                new SetPhotoOptionsTask(
                        new PhotoOptions(
                                zoom,
                                focusMode,
                                currentFocusDistance,
                                exposureMode,
                                width,
                                height,
                                pointsOfInterest2D,
                                hasExposureCompensation,
                                exposureCompensation,
                                exposureTime,
                                whiteBalanceMode,
                                iso,
                                hasRedEyeReduction,
                                redEyeReduction,
                                fillLightMode,
                                hasTorch,
                                torch,
                                colorTemperature)));
    }

    @Override
    public void takePhotoAsync(long callbackId) {
        dCheckCurrentlyOnIncomingTaskRunner(VideoCaptureCamera2.this);
        TraceEvent.instant("VideoCaptureCamera2.java", "takePhotoAsync");

        mCameraThreadHandler.post(new TakePhotoTask(callbackId));
    }

    @Override
    public void deallocateInternal() {
        Log.d(TAG, "deallocate");
    }
}
