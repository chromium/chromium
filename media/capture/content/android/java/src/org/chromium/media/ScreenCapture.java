// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.content.Context;
import android.content.Intent;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Surface;
import android.view.WindowManager;

import androidx.annotation.IntDef;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;

/**
 * This class implements Screen Capture using projection API, introduced in Android
 * API 21 (L Release). Capture takes place in the current Looper, while pixel
 * download takes place in another thread used by ImageReader.
 **/
@JNINamespace("media")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class ScreenCapture extends Fragment {
    private static final String TAG = "ScreenCapture";

    private static final int REQUEST_MEDIA_PROJECTION = 1;

    @IntDef({CaptureState.ATTACHED, CaptureState.ALLOWED, CaptureState.STARTED,
            CaptureState.STOPPING, CaptureState.STOPPED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface CaptureState {
        int ATTACHED = 0;
        int ALLOWED = 1;
        int STARTED = 2;
        int STOPPING = 3;
        int STOPPED = 4;
    }

    @IntDef({DeviceOrientation.PORTRAIT, DeviceOrientation.LANDSCAPE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface DeviceOrientation {
        int PORTRAIT = 0;
        int LANDSCAPE = 1;
    }

    // Native callback context variable.
    private final long mNativeScreenCaptureMachineAndroid;

    private final Object mCaptureStateLock = new Object();
    private @CaptureState int mCaptureState = CaptureState.STOPPED;

    private MediaProjection mMediaProjection;
    private MediaProjectionManager mMediaProjectionManager;
    private VirtualDisplay mVirtualDisplay;
    private Surface mSurface;
    private ImageReader mImageReader;
    private HandlerThread mThread;
    private Handler mBackgroundHandler;
    private Display mDisplay;
    private @DeviceOrientation int mCurrentOrientation;
    private Intent mResultData;

    private int mScreenDensity;
    private int mWidth;
    private int mHeight;
    private int mFormat;
    private int mResultCode;

    ScreenCapture(long nativeScreenCaptureMachineAndroid) {
        mNativeScreenCaptureMachineAndroid = nativeScreenCaptureMachineAndroid;
    }

    // Factory method.
    @CalledByNative
    static ScreenCapture createScreenCaptureMachine(long nativeScreenCaptureMachineAndroid) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return new ScreenCapture(nativeScreenCaptureMachineAndroid);
        }
        return null;
    }

    // Internal class implementing the ImageReader listener. Gets pinged when a
    // new frame is been captured and downloaded to memory-backed buffers.
    private class CrImageReaderListener implements ImageReader.OnImageAvailableListener {
        @Override
        public void onImageAvailable(ImageReader reader) {
            synchronized (mCaptureStateLock) {
                if (mCaptureState != CaptureState.STARTED) {
                    Log.e(TAG, "Get captured frame in unexpected state.");
                    return;
                }
            }

            // If device is rotated, inform native, then re-create ImageReader and VirtualDisplay
            // with the new orientation, and drop the current frame.
            if (maybeDoRotation()) {
                createImageReaderWithFormat();
                createVirtualDisplay();
                return;
            }

            try (Image image = reader.acquireLatestImage()) {
                if (image == null) return;
                if (reader.getWidth() != image.getWidth()
                        || reader.getHeight() != image.getHeight()) {
                    Log.e(TAG, "ImageReader size (" + reader.getWidth() + "x" + reader.getHeight()
                                    + ") did not match Image size (" + image.getWidth() + "x"
                                    + image.getHeight() + ")");
                    throw new IllegalStateException();
                }

                switch (image.getFormat()) {
                    case PixelFormat.RGBA_8888:
                        if (image.getPlanes().length != 1) {
                            Log.e(TAG, "Unexpected image planes for RGBA_8888 format: "
                                            + image.getPlanes().length);
                            throw new IllegalStateException();
                        }

                        ScreenCaptureJni.get().onRGBAFrameAvailable(
                                mNativeScreenCaptureMachineAndroid, ScreenCapture.this,
                                image.getPlanes()[0].getBuffer(),
                                image.getPlanes()[0].getRowStride(), image.getCropRect().left,
                                image.getCropRect().top, image.getCropRect().width(),
                                image.getCropRect().height(), image.getTimestamp());
                        break;
                    case ImageFormat.YUV_420_888:
                        if (image.getPlanes().length != 3) {
                            Log.e(TAG, "Unexpected image planes for YUV_420_888 format: "
                                            + image.getPlanes().length);
                            throw new IllegalStateException();
                        }

                        // The pixel stride of Y plane is always 1. The U/V planes are guaranteed
                        // to have the same row stride and pixel stride.
                        ScreenCaptureJni.get().onI420FrameAvailable(
                                mNativeScreenCaptureMachineAndroid, ScreenCapture.this,
                                image.getPlanes()[0].getBuffer(),
                                image.getPlanes()[0].getRowStride(),
                                image.getPlanes()[1].getBuffer(), image.getPlanes()[2].getBuffer(),
                                image.getPlanes()[1].getRowStride(),
                                image.getPlanes()[1].getPixelStride(), image.getCropRect().left,
                                image.getCropRect().top, image.getCropRect().width(),
                                image.getCropRect().height(), image.getTimestamp());
                        break;
                    default:
                        Log.e(TAG, "Unexpected image format: " + image.getFormat());
                        throw new IllegalStateException();
                }
            } catch (IllegalStateException ex) {
                Log.e(TAG, "acquireLatestImage():" + ex);
            } catch (UnsupportedOperationException ex) {
                Log.i(TAG, "acquireLatestImage():" + ex);
                if (mFormat == ImageFormat.YUV_420_888) {
                    // YUV_420_888 is the preference, but not all devices support it,
                    // fall-back to RGBA_8888 then.
                    mFormat = PixelFormat.RGBA_8888;
                    createImageReaderWithFormat();
                    createVirtualDisplay();
                }
            }
        }
    }

    private class MediaProjectionCallback extends MediaProjection.Callback {
        @Override
        public void onStop() {
            changeCaptureStateAndNotify(CaptureState.STOPPED);
            mMediaProjection = null;
            if (mVirtualDisplay == null) return;
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        Log.d(TAG, "onAttach");
        changeCaptureStateAndNotify(CaptureState.ATTACHED);
    }

    // This method was deprecated in API level 23 by onAttach(Context).
    // TODO(braveyao): remove this method after the minSdkVersion of chrome is 23,
    // https://crbug.com/614172.
    @SuppressWarnings("deprecation")
    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        Log.d(TAG, "onAttach");
        changeCaptureStateAndNotify(CaptureState.ATTACHED);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        Log.d(TAG, "onDetach");
        stopCapture();
    }

    @CalledByNative
    public boolean allocate(int width, int height) {
        mWidth = width;
        mHeight = height;

        mMediaProjectionManager =
                (MediaProjectionManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.MEDIA_PROJECTION_SERVICE);
        if (mMediaProjectionManager == null) {
            Log.e(TAG, "mMediaProjectionManager is null");
            return false;
        }

        WindowManager windowManager =
                (WindowManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.WINDOW_SERVICE);
        mDisplay = windowManager.getDefaultDisplay();

        DisplayMetrics metrics = new DisplayMetrics();
        mDisplay.getMetrics(metrics);
        mScreenDensity = metrics.densityDpi;

        return true;
    }

    @CalledByNative
    public boolean startPrompt() {
        Log.d(TAG, "startPrompt");
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            Log.e(TAG, "activity is null");
            return false;
        }
        FragmentManager fragmentManager = activity.getFragmentManager();
        FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
        fragmentTransaction.add(this, "screencapture");
        try {
            fragmentTransaction.commit();
        } catch (RuntimeException e) {
            Log.e(TAG, "ScreenCaptureExcaption " + e);
            return false;
        }

        synchronized (mCaptureStateLock) {
            while (mCaptureState != CaptureState.ATTACHED) {
                try {
                    mCaptureStateLock.wait();
                } catch (InterruptedException ex) {
                    Log.e(TAG, "ScreenCaptureException: " + ex);
                }
            }
        }

        try {
            startActivityForResult(
                    mMediaProjectionManager.createScreenCaptureIntent(), REQUEST_MEDIA_PROJECTION);
        } catch (android.content.ActivityNotFoundException e) {
            Log.e(TAG, "ScreenCaptureException " + e);
            return false;
        }
        return true;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != REQUEST_MEDIA_PROJECTION) return;

        if (resultCode == Activity.RESULT_OK) {
            mResultCode = resultCode;
            mResultData = data;
            changeCaptureStateAndNotify(CaptureState.ALLOWED);
        }
        ScreenCaptureJni.get().onActivityResult(mNativeScreenCaptureMachineAndroid,
                ScreenCapture.this, resultCode == Activity.RESULT_OK);
    }

    @CalledByNative
    public boolean startCapture() {
        Log.d(TAG, "startCapture");
        synchronized (mCaptureStateLock) {
            if (mCaptureState != CaptureState.ALLOWED) {
                Log.e(TAG, "startCapture() invoked without user permission.");
                return false;
            }
        }
        mMediaProjection = mMediaProjectionManager.getMediaProjection(mResultCode, mResultData);
        if (mMediaProjection == null) {
            Log.e(TAG, "mMediaProjection is null");
            return false;
        }
        mMediaProjection.registerCallback(new MediaProjectionCallback(), null);

        mThread = new HandlerThread("ScreenCapture");
        mThread.start();
        mBackgroundHandler = new Handler(mThread.getLooper());

        // YUV420 is preferred. But not all devices supports it and it even will
        // crash some devices. See https://crbug.com/674989 . A feature request
        // was already filed to support YUV420 in VirturalDisplay. Before YUV420
        // is available, stay with RGBA_8888 at present.
        mFormat = PixelFormat.RGBA_8888;

        maybeDoRotation();
        createImageReaderWithFormat();
        createVirtualDisplay();

        changeCaptureStateAndNotify(CaptureState.STARTED);
        return true;
    }

    @CalledByNative
    public void stopCapture() {
        Log.d(TAG, "stopCapture");
        synchronized (mCaptureStateLock) {
            if (mMediaProjection != null && mCaptureState == CaptureState.STARTED) {
                mMediaProjection.stop();
                changeCaptureStateAndNotify(CaptureState.STOPPING);

                while (mCaptureState != CaptureState.STOPPED) {
                    try {
                        mCaptureStateLock.wait();
                    } catch (InterruptedException ex) {
                        Log.e(TAG, "ScreenCaptureEvent: " + ex);
                    }
                }
            } else {
                changeCaptureStateAndNotify(CaptureState.STOPPED);
            }
        }
    }

    private void createImageReaderWithFormat() {
        if (mImageReader != null) {
            mImageReader.close();
        }

        final int maxImages = 2;
        mImageReader = ImageReader.newInstance(mWidth, mHeight, mFormat, maxImages);
        mSurface = mImageReader.getSurface();
        final CrImageReaderListener imageReaderListener = new CrImageReaderListener();
        mImageReader.setOnImageAvailableListener(imageReaderListener, mBackgroundHandler);
    }

    private void createVirtualDisplay() {
        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
        }

        mVirtualDisplay = mMediaProjection.createVirtualDisplay("ScreenCapture", mWidth, mHeight,
                mScreenDensity, DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR, mSurface, null,
                null);
    }

    private void changeCaptureStateAndNotify(@CaptureState int state) {
        synchronized (mCaptureStateLock) {
            mCaptureState = state;
            mCaptureStateLock.notifyAll();
        }
    }

    private int getDeviceRotation() {
        switch (mDisplay.getRotation()) {
            case Surface.ROTATION_0:
                return 0;
            case Surface.ROTATION_90:
                return 90;
            case Surface.ROTATION_180:
                return 180;
            case Surface.ROTATION_270:
                return 270;
            default:
                // This should not happen.
                assert false;
                return 0;
        }
    }

    private @DeviceOrientation int getDeviceOrientation(int rotation) {
        switch (rotation) {
            case 0:
            case 180:
                return DeviceOrientation.PORTRAIT;
            case 90:
            case 270:
                return DeviceOrientation.LANDSCAPE;
            default:
                // This should not happen;
                assert false;
                return DeviceOrientation.LANDSCAPE;
        }
    }

    private boolean maybeDoRotation() {
        final int rotation = getDeviceRotation();
        final @DeviceOrientation int orientation = getDeviceOrientation(rotation);
        if (orientation == mCurrentOrientation) {
            return false;
        }

        mCurrentOrientation = orientation;
        rotateCaptureOrientation(orientation);
        ScreenCaptureJni.get().onOrientationChange(
                mNativeScreenCaptureMachineAndroid, ScreenCapture.this, rotation);
        return true;
    }

    private void rotateCaptureOrientation(@DeviceOrientation int orientation) {
        if ((orientation == DeviceOrientation.LANDSCAPE && mWidth < mHeight)
                || (orientation == DeviceOrientation.PORTRAIT && mHeight < mWidth)) {
            mWidth += mHeight - (mHeight = mWidth);
        }
    }

    @NativeMethods
    interface Natives {
        // Method for ScreenCapture implementations to call back native code.
        void onRGBAFrameAvailable(long nativeScreenCaptureMachineAndroid, ScreenCapture caller,
                ByteBuffer buf, int rowStride, int left, int top, int width, int height,
                long timestamp);

        void onI420FrameAvailable(long nativeScreenCaptureMachineAndroid, ScreenCapture caller,
                ByteBuffer yBuffer, int yStride, ByteBuffer uBuffer, ByteBuffer vBuffer,
                int uvRowStride, int uvPixelStride, int left, int top, int width, int height,
                long timestamp);
        // Method for ScreenCapture implementations to notify activity result.
        void onActivityResult(
                long nativeScreenCaptureMachineAndroid, ScreenCapture caller, boolean result);

        // Method for ScreenCapture implementations to notify orientation change.
        void onOrientationChange(
                long nativeScreenCaptureMachineAndroid, ScreenCapture caller, int rotation);
    }
}
