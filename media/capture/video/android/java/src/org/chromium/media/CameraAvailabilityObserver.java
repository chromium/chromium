// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.hardware.camera2.CameraManager;
import android.os.Handler;
import android.os.HandlerThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;

/**
 * This class listens to camera availability changes and notifies the native
 * CameraAvailabilityObserver of such changes. Only one observer is expected to be created at a
 * time.
 */
@JNINamespace("media")
class CameraAvailabilityObserver extends CameraManager.AvailabilityCallback {
    /**
     * Creates a new CameraAvailabilityObserver object.
     *
     * @param nativeCameraAvailabilityObserver The native observer to be notified of camera
     *     availability changes.
     * @return A new CameraAvailabilityObserver instance.
     */
    @CalledByNative
    static CameraAvailabilityObserver createCameraAvailabilityObserver(
            long nativeCameraAvailabilityObserver) {
        return new CameraAvailabilityObserver(nativeCameraAvailabilityObserver);
    }

    public CameraAvailabilityObserver(long nativeCameraAvailabilityObserver) {
        synchronized (mNativeCameraAvailabilityObserverLock) {
            mNativeCameraAvailabilityObserver = nativeCameraAvailabilityObserver;
        }
        mCameraManager =
                (CameraManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CAMERA_SERVICE);
        HandlerThread thread = new HandlerThread("CameraAvailabilityObserver_ObservationThread");
        thread.start();
        mObservationThreadHandler = new Handler(thread.getLooper());
    }

    /** Starts observing camera availability changes. */
    @CalledByNative
    public void startObservation() {
        mCameraManager.registerAvailabilityCallback(this, mObservationThreadHandler);
    }

    /** Stops observing camera availability changes. */
    @CalledByNative
    public void stopObservation() {
        synchronized (mNativeCameraAvailabilityObserverLock) {
            mNativeCameraAvailabilityObserver = 0;
        }
        mCameraManager.unregisterAvailabilityCallback(this);
    }

    @Override
    public void onCameraAvailable(String cameraId) {
        synchronized (mNativeCameraAvailabilityObserverLock) {
            if (mNativeCameraAvailabilityObserver == 0) {
                return;
            }
            CameraAvailabilityObserverJni.get()
                    .onCameraAvailabilityChanged(mNativeCameraAvailabilityObserver, this);
        }
    }

    @Override
    public void onCameraUnavailable(String cameraId) {
        synchronized (mNativeCameraAvailabilityObserverLock) {
            if (mNativeCameraAvailabilityObserver == 0) {
                return;
            }
            CameraAvailabilityObserverJni.get()
                    .onCameraAvailabilityChanged(mNativeCameraAvailabilityObserver, this);
        }
    }

    // Lock for guarding |mNativeCameraAvailabilityObserver|.
    private final Object mNativeCameraAvailabilityObserverLock = new Object();
    private long mNativeCameraAvailabilityObserver;
    private CameraManager mCameraManager;
    private Handler mObservationThreadHandler;

    @NativeMethods
    interface Natives {
        void onCameraAvailabilityChanged(
                long nativeCameraAvailabilityObserver, CameraAvailabilityObserver caller);
    }
}
