// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.util.Map;

class AudioDeviceListener {
    private static final boolean DEBUG = false;

    private static final String TAG = "media";

    // Enabled during initialization if BLUETOOTH permission is granted.
    private boolean mHasBluetoothPermission;

    // Broadcast receiver for wired headset intent broadcasts.
    private BroadcastReceiver mWiredHeadsetReceiver;

    // Broadcast receiver for Bluetooth headset intent broadcasts.
    // Utilized to detect changes in Bluetooth headset availability.
    private BroadcastReceiver mBluetoothHeadsetReceiver;

    // The UsbManager of this system.
    private final UsbManager mUsbManager;

    // Broadcast receiver for USB audio devices intent broadcasts.
    // Utilized to detect if a USB device is attached or detached.
    private BroadcastReceiver mUsbAudioReceiver;

    private final AudioDeviceSelector.Devices mDeviceStates;

    public AudioDeviceListener(AudioDeviceSelector.Devices devices) {
        mUsbManager = (UsbManager) ContextUtils.getApplicationContext().getSystemService(
                Context.USB_SERVICE);
        mDeviceStates = devices;
    }

    public void init(boolean hasBluetoothPermission) {
        // Initialize audio device list with things we know is always available.
        mDeviceStates.setDeviceExistence(AudioDeviceSelector.Devices.ID_EARPIECE, hasEarpiece());
        mDeviceStates.setDeviceExistence(AudioDeviceSelector.Devices.ID_USB_AUDIO, hasUsbAudio());
        mDeviceStates.setDeviceExistence(AudioDeviceSelector.Devices.ID_SPEAKERPHONE, true);

        mHasBluetoothPermission = hasBluetoothPermission;

        // Register receivers for broadcasting intents related to Bluetooth device
        // and Bluetooth SCO notifications. Requires BLUETOOTH permission.
        registerBluetoothIntentsIfNeeded();

        // Register receiver for broadcasting intents related to adding/
        // removing a wired headset (Intent.ACTION_HEADSET_PLUG).
        registerForWiredHeadsetIntentBroadcast();

        // Register receiver for broadcasting intents related to adding/removing a
        // USB audio device (ACTION_USB_DEVICE_ATTACHED/DETACHED);
        registerForUsbAudioIntentBroadcast();
    }

    public void close() {
        unregisterForWiredHeadsetIntentBroadcast();
        unregisterBluetoothIntentsIfNeeded();
        unregisterForUsbAudioIntentBroadcast();
    }

    /**
     * Register for BT intents if we have the BLUETOOTH permission.
     * Also extends the list of available devices with a BT device if one exists.
     */
    private void registerBluetoothIntentsIfNeeded() {
        // Add a Bluetooth headset to the list of available devices if a BT
        // headset is detected and if we have the BLUETOOTH permission.
        // We must do this initial check using a dedicated method since the
        // broadcasted intent BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED
        // is not sticky and will only be received if a BT headset is connected
        // after this method has been called.
        if (!mHasBluetoothPermission) {
            Log.w(TAG, "registerBluetoothIntentsIfNeeded: Requires BLUETOOTH permission");
            return;
        }
        mDeviceStates.setDeviceExistence(
                AudioDeviceSelector.Devices.ID_BLUETOOTH_HEADSET, hasBluetoothHeadset());

        // Register receivers for broadcast intents related to changes in
        // Bluetooth headset availability.
        registerForBluetoothHeadsetIntentBroadcast();
    }

    /** Unregister for BT intents if a registration has been made. */
    private void unregisterBluetoothIntentsIfNeeded() {
        // No need to unregister if we don't have BT permissions.
        if (!mHasBluetoothPermission) return;

        ContextUtils.getApplicationContext().unregisterReceiver(mBluetoothHeadsetReceiver);
        mBluetoothHeadsetReceiver = null;
    }

    /**
     * Gets the current Bluetooth headset state.
     * android.bluetooth.BluetoothAdapter.getProfileConnectionState() requires
     * the BLUETOOTH permission.
     */
    private boolean hasBluetoothHeadset() {
        if (!mHasBluetoothPermission) {
            Log.w(TAG, "hasBluetoothHeadset() requires BLUETOOTH permission");
            return false;
        }

        BluetoothManager btManager =
                (BluetoothManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.BLUETOOTH_SERVICE);
        BluetoothAdapter btAdapter = btManager.getAdapter();

        if (btAdapter == null) {
            // Bluetooth not supported on this platform.
            return false;
        }

        int profileConnectionState =
                btAdapter.getProfileConnectionState(android.bluetooth.BluetoothProfile.HEADSET);

        // Ensure that Bluetooth is enabled and that a device which supports the
        // headset and handsfree profile is connected.
        // TODO(henrika): it is possible that btAdapter.isEnabled() is
        // redundant. It might be sufficient to only check the profile state.
        return btAdapter.isEnabled()
                && profileConnectionState == android.bluetooth.BluetoothProfile.STATE_CONNECTED;
    }

    /**
     * Get the current USB audio device state. Android detects a compatible USB digital audio
     * peripheral and automatically routes audio playback and capture appropriately on Android5.0
     * and higher in the order of wired headset first, then USB audio device and earpiece at last.
     */
    private boolean hasUsbAudio() {
        // UsbManager fails internally with NullPointerException on the emulator created without
        // Google APIs.
        Map<String, UsbDevice> devices;
        try {
            devices = mUsbManager.getDeviceList();
        } catch (NullPointerException e) {
            return false;
        }

        for (UsbDevice device : devices.values()) {
            // A USB device with USB_CLASS_AUDIO and USB_CLASS_COMM interface is
            // considerred as a USB audio device here.
            if (hasUsbAudioCommInterface(device)) {
                if (DEBUG) {
                    logd("USB audio device: " + device.getProductName());
                }
                return true;
            }
        }

        return false;
    }

    /**
     * Registers receiver for the broadcasted intent when a wired headset is
     * plugged in or unplugged. The received intent will have an extra
     * 'state' value where 0 means unplugged, and 1 means plugged.
     */
    private void registerForWiredHeadsetIntentBroadcast() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);

        /** Receiver which handles changes in wired headset availability. */
        mWiredHeadsetReceiver = new BroadcastReceiver() {
            private static final int STATE_UNPLUGGED = 0;
            private static final int STATE_PLUGGED = 1;
            private static final int HAS_NO_MIC = 0;
            private static final int HAS_MIC = 1;

            @Override
            public void onReceive(Context context, Intent intent) {
                int state = intent.getIntExtra("state", STATE_UNPLUGGED);
                if (DEBUG) {
                    int microphone = intent.getIntExtra("microphone", HAS_NO_MIC);
                    String name = intent.getStringExtra("name");
                    logd("BroadcastReceiver.onReceive: a=" + intent.getAction() + ", s=" + state
                            + ", m=" + microphone + ", n=" + name
                            + ", sb=" + isInitialStickyBroadcast());
                }
                switch (state) {
                    case STATE_UNPLUGGED:
                        mDeviceStates.setDeviceExistence(
                                AudioDeviceSelector.Devices.ID_WIRED_HEADSET, false);
                        break;
                    case STATE_PLUGGED:
                        mDeviceStates.setDeviceExistence(
                                AudioDeviceSelector.Devices.ID_WIRED_HEADSET, true);
                        break;
                    default:
                        break;
                }

                mDeviceStates.onPotentialDeviceStatusChange();
            }
        };

        // Note: the intent we register for here is sticky, so it'll tell us
        // immediately what the last action was (plugged or unplugged).
        // It will enable us to set the speakerphone correctly.
        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(), mWiredHeadsetReceiver, filter);
    }

    /** Unregister receiver for broadcasted ACTION_HEADSET_PLUG intent. */
    private void unregisterForWiredHeadsetIntentBroadcast() {
        ContextUtils.getApplicationContext().unregisterReceiver(mWiredHeadsetReceiver);
        mWiredHeadsetReceiver = null;
    }

    /**
     * Registers receiver for the broadcasted intent related to BT headset
     * availability or a change in connection state of the local Bluetooth
     * adapter. Example: triggers when the BT device is turned on or off.
     * BLUETOOTH permission is required to receive this one.
     */
    private void registerForBluetoothHeadsetIntentBroadcast() {
        IntentFilter filter = new IntentFilter(
                android.bluetooth.BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);

        /** Receiver which handles changes in BT headset availability. */
        mBluetoothHeadsetReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                // A change in connection state of the Headset profile has
                // been detected, e.g. BT headset has been connected or
                // disconnected. This broadcast is *not* sticky.
                int profileState =
                        intent.getIntExtra(android.bluetooth.BluetoothHeadset.EXTRA_STATE,
                                android.bluetooth.BluetoothHeadset.STATE_DISCONNECTED);
                if (DEBUG) {
                    logd("BroadcastReceiver.onReceive: a=" + intent.getAction()
                            + ", s=" + profileState + ", sb=" + isInitialStickyBroadcast());
                }

                switch (profileState) {
                    case android.bluetooth.BluetoothProfile.STATE_DISCONNECTED:
                        // We do not have to explicitly call stopBluetoothSco()
                        // since BT SCO will be disconnected automatically when
                        // the BT headset is disabled.
                        mDeviceStates.setDeviceExistence(
                                AudioDeviceSelector.Devices.ID_BLUETOOTH_HEADSET, false);
                        mDeviceStates.onPotentialDeviceStatusChange();
                        break;
                    case android.bluetooth.BluetoothProfile.STATE_CONNECTED:
                        mDeviceStates.setDeviceExistence(
                                AudioDeviceSelector.Devices.ID_BLUETOOTH_HEADSET, true);
                        mDeviceStates.onPotentialDeviceStatusChange();
                        break;
                    case android.bluetooth.BluetoothProfile.STATE_CONNECTING:
                        // Bluetooth service is switching from off to on.
                        break;
                    case android.bluetooth.BluetoothProfile.STATE_DISCONNECTING:
                        // Bluetooth service is switching from on to off.
                        break;
                    default:
                        break;
                }
            }
        };

        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(), mBluetoothHeadsetReceiver, filter);
    }

    /**
     * Enumerates the USB interfaces of the given USB device for interface with USB_CLASS_AUDIO
     * class (USB class for audio devices) and USB_CLASS_COMM subclass (USB class for communication
     * devices). Any device that supports these conditions will be considered a USB audio device.
     *
     * @param device USB device to be checked.
     * @return Whether the USB device has such an interface.
     */
    private boolean hasUsbAudioCommInterface(UsbDevice device) {
        for (int i = 0; i < device.getInterfaceCount(); ++i) {
            UsbInterface iface = device.getInterface(i);
            if (iface.getInterfaceClass() == UsbConstants.USB_CLASS_AUDIO
                    && iface.getInterfaceSubclass() == UsbConstants.USB_CLASS_COMM) {
                // There is at least one interface supporting audio communication.
                return true;
            }
        }

        return false;
    }

    /**
     * Registers receiver for the broadcasted intent when a USB device is plugged in or unplugged.
     * Notice: Android supports multiple USB audio devices connected through a USB hub and OS will
     * select the capture device and playback device from them. But plugging them in/out during a
     * call may cause some unexpected result, i.e capturing error or zero capture length.
     */
    private void registerForUsbAudioIntentBroadcast() {
        mUsbAudioReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (DEBUG) {
                    logd("UsbDeviceBroadcastReceiver.onReceive: a= " + intent.getAction()
                            + ", Device: " + device.toString());
                }

                // Not a USB audio device.
                if (!hasUsbAudioCommInterface(device)) return;

                if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(intent.getAction())) {
                    mDeviceStates.setDeviceExistence(
                            AudioDeviceSelector.Devices.ID_USB_AUDIO, true);
                } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(intent.getAction())
                        && !hasUsbAudio()) {
                    mDeviceStates.setDeviceExistence(
                            AudioDeviceSelector.Devices.ID_USB_AUDIO, false);
                }

                mDeviceStates.onPotentialDeviceStatusChange();
            }
        };

        IntentFilter filter = new IntentFilter();
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);

        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(), mUsbAudioReceiver, filter);
    }

    /** Unregister receiver for broadcasted ACTION_USB_DEVICE_ATTACHED/DETACHED intent. */
    private void unregisterForUsbAudioIntentBroadcast() {
        ContextUtils.getApplicationContext().unregisterReceiver(mUsbAudioReceiver);
        mUsbAudioReceiver = null;
    }

    /** Gets the current earpiece state. */
    private boolean hasEarpiece() {
        return ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_TELEPHONY);
    }

    /** Trivial helper method for debug logging */
    private static void logd(String msg) {
        Log.d(TAG, msg);
    }

    /** Trivial helper method for error logging */
    private static void loge(String msg) {
        Log.e(TAG, msg);
    }
}
