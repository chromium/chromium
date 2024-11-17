// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.midi;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Owned by its native counterpart declared in
 * usb_midi_device_factory_android.h. Refer to that class for general comments.
 */
@JNINamespace("midi")
class UsbMidiDeviceFactoryAndroid {
    /** The UsbManager of this system. */
    private UsbManager mUsbManager;

    /** BroadcastReceiver for USB device permission granted/denied responses from UsbManager. */
    private BroadcastReceiver mPermissionReceiver;

    /** BroadcastReceiver for USB device attached/detached events. */
    private BroadcastReceiver mDeviceChangeReceiver;

    /** Accessible USB-MIDI devices got so far. */
    private final List<UsbMidiDeviceAndroid> mDevices = new ArrayList<UsbMidiDeviceAndroid>();

    /** Devices whose access permission requested but not resolved so far. */
    private Set<UsbDevice> mRequestedDevices;

    /** True when the enumeration is in progress. */
    private boolean mIsEnumeratingDevices;

    /** The identifier of this factory. */
    private long mNativePointer;

    private static final String ACTION_USB_PERMISSION = "org.chromium.midi.USB_PERMISSION";

    /**
     * Constructs a UsbMidiDeviceAndroid.
     * @param nativePointer The native pointer to which the created factory is associated.
     */
    UsbMidiDeviceFactoryAndroid(long nativePointer) {
        mUsbManager =
                (UsbManager)
                        ContextUtils.getApplicationContext().getSystemService(Context.USB_SERVICE);
        mNativePointer = nativePointer;
        mPermissionReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        if (!IntentUtils.isTrustedIntentFromSelf(intent)) return;
                        assert ACTION_USB_PERMISSION.equals(intent.getAction());
                        onUsbDevicePermissionRequestDone(intent);
                    }
                };
        mDeviceChangeReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                        if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(intent.getAction())) {
                            requestDevicePermissionIfNecessary(device);
                        }
                        if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(intent.getAction())) {
                            onUsbDeviceDetached(device);
                        }
                    }
                };
        mRequestedDevices = new HashSet<UsbDevice>();

        Context context = ContextUtils.getApplicationContext();
        IntentFilter permissionFilter = new IntentFilter();
        permissionFilter.addAction(ACTION_USB_PERMISSION);
        ContextUtils.registerNonExportedBroadcastReceiver(
                context, mPermissionReceiver, permissionFilter);
        IntentFilter deviceChangeFilter = new IntentFilter();
        deviceChangeFilter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        deviceChangeFilter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        ContextUtils.registerProtectedBroadcastReceiver(
                context, mDeviceChangeReceiver, deviceChangeFilter);
    }

    /**
     * Constructs a UsbMidiDeviceAndroid.
     * @param nativePointer The native pointer to which the created factory is associated.
     */
    @CalledByNative
    static UsbMidiDeviceFactoryAndroid create(long nativePointer) {
        return new UsbMidiDeviceFactoryAndroid(nativePointer);
    }

    /**
     * Enumerates USB-MIDI devices.
     * If there are devices having USB-MIDI interfaces, this function requests permission for
     * accessing the device to the user.
     * When the permission request is accepted or rejected,
     * UsbMidiDeviceFactoryAndroidJni.get().onUsbMidiDeviceRequestDone will be called.
     *
     * If there are no USB-MIDI interfaces, this function returns false.
     * @return true if some permission requests are in progress.
     */
    @CalledByNative
    boolean enumerateDevices() {
        assert !mIsEnumeratingDevices;
        mIsEnumeratingDevices = true;
        Map<String, UsbDevice> devices = mUsbManager.getDeviceList();
        if (devices.isEmpty()) {
            // No USB-MIDI devices are found.
            mIsEnumeratingDevices = false;
            return false;
        }
        for (UsbDevice device : devices.values()) {
            requestDevicePermissionIfNecessary(device);
        }
        return !mRequestedDevices.isEmpty();
    }

    /**
     * Request a device access permission if there is a MIDI interface in the device.
     *
     * @param device a USB device
     */
    private void requestDevicePermissionIfNecessary(UsbDevice device) {
        for (UsbDevice d : mRequestedDevices) {
            if (d.getDeviceId() == device.getDeviceId()) {
                // It is already requested.
                return;
            }
        }

        for (int i = 0; i < device.getInterfaceCount(); ++i) {
            UsbInterface iface = device.getInterface(i);
            if (iface.getInterfaceClass() == UsbConstants.USB_CLASS_AUDIO
                    && iface.getInterfaceSubclass() == UsbMidiDeviceAndroid.MIDI_SUBCLASS) {
                Context context = ContextUtils.getApplicationContext();
                Intent intent = new Intent(ACTION_USB_PERMISSION);
                intent.setPackage(context.getPackageName());
                IntentUtils.addTrustedIntentExtras(intent);
                // There is at least one interface supporting MIDI.
                mUsbManager.requestPermission(
                        device,
                        PendingIntent.getBroadcast(
                                context,
                                0,
                                intent,
                                IntentUtils.getPendingIntentMutabilityFlag(true)));
                mRequestedDevices.add(device);
                break;
            }
        }
    }

    /**
     * Called when a USB device is detached.
     *
     * @param device a USB device
     */
    private void onUsbDeviceDetached(UsbDevice device) {
        for (UsbDevice usbDevice : mRequestedDevices) {
            if (usbDevice.getDeviceId() == device.getDeviceId()) {
                mRequestedDevices.remove(usbDevice);
                break;
            }
        }
        for (int i = 0; i < mDevices.size(); ++i) {
            UsbMidiDeviceAndroid midiDevice = mDevices.get(i);
            if (midiDevice.isClosed()) {
                // Once a device is disconnected, the system may reassign its device ID to
                // another device. So we should ignore disconnected ones.
                continue;
            }
            if (midiDevice.getUsbDevice().getDeviceId() == device.getDeviceId()) {
                midiDevice.close();
                if (mIsEnumeratingDevices) {
                    // In this case, we don't have to keep mDevices sync with the devices list
                    // in MidiManagerUsb.
                    mDevices.remove(i);
                    return;
                }
                if (mNativePointer != 0) {
                    UsbMidiDeviceFactoryAndroidJni.get().onUsbMidiDeviceDetached(mNativePointer, i);
                }
                return;
            }
        }
    }

    /**
     * Called when the user accepts or rejects the permission request requested by EnumerateDevices.
     */
    private void onUsbDevicePermissionRequestDone(Intent intent) {
        UsbDevice device = (UsbDevice) intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
        UsbMidiDeviceAndroid midiDevice = null;
        if (mRequestedDevices.contains(device)) {
            mRequestedDevices.remove(device);
            if (!intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                // The request was rejected.
                device = null;
            }
        } else {
            device = null;
        }

        if (device != null) {
            for (UsbMidiDeviceAndroid registered : mDevices) {
                if (!registered.isClosed()
                        && registered.getUsbDevice().getDeviceId() == device.getDeviceId()) {
                    // The device is already registered.
                    device = null;
                    break;
                }
            }
        }

        if (device != null) {
            // Now we can add the device.
            midiDevice = new UsbMidiDeviceAndroid(mUsbManager, device);
            mDevices.add(midiDevice);
        }

        if (!mRequestedDevices.isEmpty()) {
            return;
        }
        if (mNativePointer == 0) {
            return;
        }

        if (mIsEnumeratingDevices) {
            UsbMidiDeviceFactoryAndroidJni.get()
                    .onUsbMidiDeviceRequestDone(mNativePointer, mDevices.toArray());
            mIsEnumeratingDevices = false;
        } else if (midiDevice != null) {
            UsbMidiDeviceFactoryAndroidJni.get()
                    .onUsbMidiDeviceAttached(mNativePointer, midiDevice);
        }
    }

    /** Disconnects the native object. */
    @CalledByNative
    void close() {
        mNativePointer = 0;
        ContextUtils.getApplicationContext().unregisterReceiver(mDeviceChangeReceiver);
        ContextUtils.getApplicationContext().unregisterReceiver(mPermissionReceiver);
    }

    @NativeMethods
    interface Natives {
        void onUsbMidiDeviceRequestDone(long nativeUsbMidiDeviceFactoryAndroid, Object[] devices);

        void onUsbMidiDeviceAttached(long nativeUsbMidiDeviceFactoryAndroid, Object device);

        void onUsbMidiDeviceDetached(long nativeUsbMidiDeviceFactoryAndroid, int index);
    }
}
