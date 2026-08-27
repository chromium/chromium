// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.hid;

import android.os.Build;
import android.os.OutcomeReceiver;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.hid.HidDevice;
import org.chromium.base.hid.HidDevice.HidEventListener;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Java wrapper for org.chromium.base.hid.HidDevice. Handled entirely on the Android UI (main)
 * thread to ensure thread safety with C++ HidConnectionAndroid.
 */
@RequiresApi(Build.VERSION_CODES.CINNAMON_BUN) // API 37+ (Cinnamon Bun)
@JNINamespace("device")
@NullMarked
public class ChromeHidConnection {
    private static final String TAG = "HidConn";
    private final HidDevice mDevice;
    private final SequencedTaskRunner mUiTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.UI_DEFAULT);
    private long mNativeConnectionPointer;
    private @Nullable HidEventListener mEventListener;

    public ChromeHidConnection(HidDevice device) {
        mDevice = device;
    }

    @CalledByNative
    static ChromeHidConnection create(HidDevice device) {
        return new ChromeHidConnection(device);
    }

    @CalledByNative
    private void setNativeConnection(long nativeConnectionPointer) {
        mNativeConnectionPointer = nativeConnectionPointer;
        if (nativeConnectionPointer == 0 && mEventListener != null) {
            mDevice.unregisterEventListener(mEventListener);
            mEventListener = null;
        } else if (nativeConnectionPointer != 0 && mEventListener == null) {
            mEventListener =
                    new HidEventListener() {
                        @Override
                        public void onInputReport(int reportId, byte[] data) {
                            if (mNativeConnectionPointer != 0) {
                                ChromeHidConnectionJni.get()
                                        .onInputReport(mNativeConnectionPointer, reportId, data);
                            }
                        }
                    };
            try {
                mDevice.registerEventListener(mUiTaskRunner, mEventListener);
            } catch (IllegalStateException e) {
                Log.w(TAG, "Failed to register HID event listener", e);
            }
        }
    }

    @CalledByNative
    private void close() {
        mNativeConnectionPointer = 0;
        mEventListener = null;
        if (mDevice.isOpen()) {
            // Note: We catch SecurityException here because on Android 17+, if the user revokes
            // HID Special App Access in Android Settings while a connection is open, subsequent
            // calls on HidDevice throw SecurityException.
            try {
                mDevice.close();
            } catch (SecurityException | IllegalStateException e) {
                Log.w(TAG, "Error closing connection", e);
            }
        }
    }

    @CalledByNative
    private void sendOutputReport(int reportId, byte[] data, int callbackId) {
        // Note: We catch SecurityException here because on Android 17+, if the user revokes
        // HID Special App Access in Android Settings while a connection is open, subsequent
        // calls on HidDevice throw SecurityException.
        try {
            mDevice.sendOutputReport(
                    reportId,
                    data,
                    mUiTaskRunner,
                    new OutcomeReceiver<Void, Exception>() {
                        @Override
                        public void onResult(@Nullable Void result) {
                            if (mNativeConnectionPointer != 0) {
                                ChromeHidConnectionJni.get()
                                        .onWriteComplete(
                                                mNativeConnectionPointer, callbackId, true);
                            }
                        }

                        @Override
                        public void onError(Exception e) {
                            if (mNativeConnectionPointer != 0) {
                                ChromeHidConnectionJni.get()
                                        .onWriteComplete(
                                                mNativeConnectionPointer, callbackId, false);
                            }
                        }
                    });
        } catch (SecurityException | IllegalStateException e) {
            Log.e(TAG, "Error sending output report", e);
            if (mNativeConnectionPointer != 0) {
                ChromeHidConnectionJni.get()
                        .onWriteComplete(mNativeConnectionPointer, callbackId, false);
            }
        }
    }

    @CalledByNative
    private void sendFeatureReport(int reportId, byte[] data, int callbackId) {
        // Note: We catch SecurityException here because on Android 17+, if the user revokes
        // HID Special App Access in Android Settings while a connection is open, subsequent
        // calls on HidDevice throw SecurityException.
        try {
            mDevice.sendFeatureReport(
                    reportId,
                    data,
                    mUiTaskRunner,
                    new OutcomeReceiver<Void, Exception>() {
                        @Override
                        public void onResult(@Nullable Void result) {
                            if (mNativeConnectionPointer != 0) {
                                ChromeHidConnectionJni.get()
                                        .onWriteComplete(
                                                mNativeConnectionPointer, callbackId, true);
                            }
                        }

                        @Override
                        public void onError(Exception e) {
                            if (mNativeConnectionPointer != 0) {
                                ChromeHidConnectionJni.get()
                                        .onWriteComplete(
                                                mNativeConnectionPointer, callbackId, false);
                            }
                        }
                    });
        } catch (SecurityException | IllegalStateException e) {
            Log.e(TAG, "Error sending feature report", e);
            if (mNativeConnectionPointer != 0) {
                ChromeHidConnectionJni.get()
                        .onWriteComplete(mNativeConnectionPointer, callbackId, false);
            }
        }
    }

    @CalledByNative
    private void getFeatureReport(int reportId, int callbackId) {
        // Note: We catch SecurityException here because on Android 17+, if the user revokes
        // HID Special App Access in Android Settings while a connection is open, subsequent
        // calls on HidDevice throw SecurityException.
        try {
            mDevice.getFeatureReport(
                    reportId,
                    mUiTaskRunner,
                    new OutcomeReceiver<byte[], Exception>() {
                        @Override
                        public void onResult(byte[] data) {
                            if (mNativeConnectionPointer != 0) {
                                ChromeHidConnectionJni.get()
                                        .onReadFeatureComplete(
                                                mNativeConnectionPointer,
                                                callbackId,
                                                true,
                                                reportId,
                                                data);
                            }
                        }

                        @Override
                        public void onError(Exception e) {
                            if (mNativeConnectionPointer != 0) {
                                ChromeHidConnectionJni.get()
                                        .onReadFeatureComplete(
                                                mNativeConnectionPointer,
                                                callbackId,
                                                false,
                                                0,
                                                null);
                            }
                        }
                    });
        } catch (SecurityException | IllegalStateException e) {
            Log.e(TAG, "Error getting feature report", e);
            if (mNativeConnectionPointer != 0) {
                ChromeHidConnectionJni.get()
                        .onReadFeatureComplete(
                                mNativeConnectionPointer, callbackId, false, 0, null);
            }
        }
    }

    @NativeMethods
    interface Interface {
        void onWriteComplete(long nativeHidConnectionAndroid, int callbackId, boolean success);

        void onReadFeatureComplete(
                long nativeHidConnectionAndroid,
                int callbackId,
                boolean success,
                int reportId,
                @JniType("std::vector<uint8_t>") byte @Nullable [] data);

        void onInputReport(long nativeHidConnectionAndroid, int reportId, byte[] data);
    }
}
