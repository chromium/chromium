// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.midi;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.os.Build;
import android.os.Handler;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A Java class implementing midi::MidiManagerAndroid functionality.
 */
@JNINamespace("midi")
@TargetApi(Build.VERSION_CODES.M)
class MidiManagerAndroid {
    /**
     * Set true when this instance is successfully initialized.
     */
    private boolean mIsInitialized;
    /**
     * The devices held by this manager.
     */
    private final List<MidiDeviceAndroid> mDevices = new ArrayList<>();
    /**
     * The device information instances which are being initialized.
     */
    private final Set<MidiDeviceInfo> mPendingDevices = new HashSet<>();
    /**
     * The underlying MidiManager.
     */
    private final MidiManager mManager;
    /**
     * Callbacks will run on the message queue associated with this handler.
     */
    private final Handler mHandler;
    /**
     * The associated midi::MidiDeviceAndroid instance.
     */
    private final long mNativeManagerPointer;

    /**
     * True is this object is stopped.
     * This is needed because MidiManagerAndroid functions are called from the IO thread but
     * callbacks are called on the UI thread (because the IO thread doesn't have a Looper). We need
     * to protect each native function call with a synchronized block that also checks this flag.
     */
    private boolean mStopped;

    /**
     * Checks if Android MIDI is supported on the device.
     */
    @CalledByNative
    static boolean hasSystemFeatureMidi() {
        return ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_MIDI);
    }

    /**
     * A creation function called by C++.
     * @param nativeManagerPointer The native pointer to a midi::MidiManagerAndroid object.
     */
    @CalledByNative
    static MidiManagerAndroid create(long nativeManagerPointer) {
        return new MidiManagerAndroid(nativeManagerPointer);
    }

    /**
     * @param nativeManagerPointer The native pointer to a midi::MidiManagerAndroid object.
     */
    private MidiManagerAndroid(long nativeManagerPointer) {
        assert !ThreadUtils.runningOnUiThread();

        mManager = (MidiManager) ContextUtils.getApplicationContext().getSystemService(
                Context.MIDI_SERVICE);
        mHandler = new Handler(ThreadUtils.getUiThreadLooper());
        mNativeManagerPointer = nativeManagerPointer;
    }

    /**
     * Initializes this object.
     * This function must be called right after creation.
     */
    @CalledByNative
    void initialize() {
        if (mManager == null) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    synchronized (MidiManagerAndroid.this) {
                        if (mStopped) {
                            return;
                        }
                        MidiManagerAndroidJni.get().onInitializationFailed(mNativeManagerPointer);
                    }
                }
            });
            return;
        }
        mManager.registerDeviceCallback(new MidiManager.DeviceCallback() {
            @Override
            public void onDeviceAdded(MidiDeviceInfo device) {
                MidiManagerAndroid.this.onDeviceAdded(device);
            }

            @Override
            public void onDeviceRemoved(MidiDeviceInfo device) {
                MidiManagerAndroid.this.onDeviceRemoved(device);
            }
        }, mHandler);
        MidiDeviceInfo[] infos = mManager.getDevices();

        for (final MidiDeviceInfo info : infos) {
            mPendingDevices.add(info);
            openDevice(info);
        }
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                synchronized (MidiManagerAndroid.this) {
                    if (mStopped) {
                        return;
                    }
                    if (mPendingDevices.isEmpty() && !mIsInitialized) {
                        MidiManagerAndroidJni.get().onInitialized(
                                mNativeManagerPointer, mDevices.toArray(new MidiDeviceAndroid[0]));
                        mIsInitialized = true;
                    }
                }
            }
        });
    }

    /**
     * Marks this object as stopped.
     */
    @CalledByNative
    synchronized void stop() {
        mStopped = true;
    }

    private void openDevice(final MidiDeviceInfo info) {
        mManager.openDevice(info, new MidiManager.OnDeviceOpenedListener() {
            @Override
            public void onDeviceOpened(MidiDevice device) {
                MidiManagerAndroid.this.onDeviceOpened(device, info);
            }
        }, mHandler);
    }

    /**
     * Called when a midi device is attached.
     * @param info the attached device information.
     */
    private void onDeviceAdded(final MidiDeviceInfo info) {
        if (!mIsInitialized) {
            mPendingDevices.add(info);
        }
        openDevice(info);
    }

    /**
     * Called when a midi device is detached.
     * @param info the detached device information.
     */
    private synchronized void onDeviceRemoved(MidiDeviceInfo info) {
        if (mStopped) {
            return;
        }
        for (MidiDeviceAndroid device : mDevices) {
            if (device.isOpen() && device.getInfo().getId() == info.getId()) {
                device.close();
                MidiManagerAndroidJni.get().onDetached(mNativeManagerPointer, device);
            }
        }
    }

    private synchronized void onDeviceOpened(MidiDevice device, MidiDeviceInfo info) {
        if (mStopped) {
            return;
        }
        mPendingDevices.remove(info);
        if (device != null) {
            MidiDeviceAndroid xdevice = new MidiDeviceAndroid(device);
            mDevices.add(xdevice);
            if (mIsInitialized) {
                MidiManagerAndroidJni.get().onAttached(mNativeManagerPointer, xdevice);
            }
        }
        if (!mIsInitialized && mPendingDevices.isEmpty()) {
            MidiManagerAndroidJni.get().onInitialized(
                    mNativeManagerPointer, mDevices.toArray(new MidiDeviceAndroid[0]));
            mIsInitialized = true;
        }
    }

    @NativeMethods
    interface Natives {
        void onInitialized(long nativeMidiManagerAndroid, MidiDeviceAndroid[] devices);
        void onInitializationFailed(long nativeMidiManagerAndroid);
        void onAttached(long nativeMidiManagerAndroid, MidiDeviceAndroid device);
        void onDetached(long nativeMidiManagerAndroid, MidiDeviceAndroid device);
    }
}
