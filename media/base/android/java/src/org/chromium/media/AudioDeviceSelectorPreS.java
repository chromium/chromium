// Copyright 2022 The Chromium Authors. All rights reserved.
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
import android.media.AudioManager;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.compat.ApiHelperForS;

import java.util.Map;

class AudioDeviceSelectorPreS extends AudioDeviceSelector {
    private static final String TAG = "media";

    // Bluetooth audio SCO states. Example of valid state sequence:
    // SCO_INVALID -> SCO_TURNING_ON -> SCO_ON -> SCO_TURNING_OFF -> SCO_OFF.
    private static final int STATE_BLUETOOTH_SCO_INVALID = -1;
    private static final int STATE_BLUETOOTH_SCO_OFF = 0;
    private static final int STATE_BLUETOOTH_SCO_ON = 1;
    private static final int STATE_BLUETOOTH_SCO_TURNING_ON = 2;
    private static final int STATE_BLUETOOTH_SCO_TURNING_OFF = 3;

    // Enabled during initialization if BLUETOOTH permission is granted.
    private boolean mHasBluetoothPermission;

    // Stores the audio states related to Bluetooth SCO audio, where some
    // states are needed to keep track of intermediate states while the SCO
    // channel is enabled or disabled (switching state can take a few seconds).
    private int mBluetoothScoState = STATE_BLUETOOTH_SCO_INVALID;

    // Broadcast receiver for wired headset intent broadcasts.
    private BroadcastReceiver mWiredHeadsetReceiver;

    // Broadcast receiver for Bluetooth headset intent broadcasts.
    // Utilized to detect changes in Bluetooth headset availability.
    private BroadcastReceiver mBluetoothHeadsetReceiver;

    // Broadcast receiver for Bluetooth SCO broadcasts.
    // Utilized to detect if BT SCO streaming is on or off.
    private BroadcastReceiver mBluetoothScoReceiver;

    // The UsbManager of this system.
    private final UsbManager mUsbManager;
    // Broadcast receiver for USB audio devices intent broadcasts.
    // Utilized to detect if a USB device is attached or detached.
    private BroadcastReceiver mUsbAudioReceiver;

    public AudioDeviceSelectorPreS(AudioManager audioManager) {
        super(audioManager);
        mUsbManager = (UsbManager) ContextUtils.getApplicationContext().getSystemService(
                Context.USB_SERVICE);
    }

    @Override
    public void init() {
        // Initialize audio device list with things we know is always available.
        mAudioDevices[DEVICE_EARPIECE] = hasEarpiece();
        mAudioDevices[DEVICE_WIRED_HEADSET] = hasWiredHeadset();
        mAudioDevices[DEVICE_USB_AUDIO] = hasUsbAudio();
        mAudioDevices[DEVICE_SPEAKERPHONE] = true;

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

    @Override
    public void close() {
        unregisterForWiredHeadsetIntentBroadcast();
        unregisterBluetoothIntentsIfNeeded();
        unregisterForUsbAudioIntentBroadcast();
    }

    @Override
    public void setCommunicationAudioModeOn(boolean on) {
        if (!on) {
            stopBluetoothSco();
            synchronized (mLock) {
                mRequestedAudioDevice = DEVICE_INVALID;
            }
        }
    }

    @Override
    public boolean isSpeakerphoneOn() {
        return mAudioManager.isSpeakerphoneOn();
    }

    @Override
    public void setSpeakerphoneOn(boolean on) {
        boolean wasOn = mAudioManager.isSpeakerphoneOn();
        if (wasOn == on) {
            return;
        }
        mAudioManager.setSpeakerphoneOn(on);
    }

    /** Checks if the process has as specified permission or not. */
    private boolean hasPermission(String permission) {
        return ContextUtils.getApplicationContext().checkSelfPermission(permission)
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Register for BT intents if we have the BLUETOOTH permission.
     * Also extends the list of available devices with a BT device if one exists.
     */
    private void registerBluetoothIntentsIfNeeded() {
        // Check if this process has the BLUETOOTH permission or not.
        mHasBluetoothPermission = hasPermission(android.Manifest.permission.BLUETOOTH);

        // TODO(crbug.com/1317548): Remove this check once there is an AudioDeviceSelector
        // for S and above.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            mHasBluetoothPermission &= ApiHelperForS.hasBluetoothConnectPermission();
        }

        // Add a Bluetooth headset to the list of available devices if a BT
        // headset is detected and if we have the BLUETOOTH permission.
        // We must do this initial check using a dedicated method since the
        // broadcasted intent BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED
        // is not sticky and will only be received if a BT headset is connected
        // after this method has been called.
        if (!mHasBluetoothPermission) {
            Log.w(TAG, "Requires BLUETOOTH permission");
            return;
        }
        mAudioDevices[DEVICE_BLUETOOTH_HEADSET] = hasBluetoothHeadset();

        // Register receivers for broadcast intents related to changes in
        // Bluetooth headset availability and usage of the SCO channel.
        registerForBluetoothHeadsetIntentBroadcast();
        registerForBluetoothScoIntentBroadcast();
    }

    /** Unregister for BT intents if a registration has been made. */
    private void unregisterBluetoothIntentsIfNeeded() {
        if (mHasBluetoothPermission) {
            mAudioManager.stopBluetoothSco();
            unregisterForBluetoothHeadsetIntentBroadcast();
            unregisterForBluetoothScoIntentBroadcast();
        }
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

        int profileConnectionState;
        profileConnectionState =
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
        // Android 5.0 (API level 21) and above supports USB audio class 1 (UAC1) features for
        // audio functions, capture and playback, in host mode.

        boolean hasUsbAudio = false;
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
                hasUsbAudio = true;
                break;
            }
        }

        return hasUsbAudio;
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
                        synchronized (mLock) {
                            // Wired headset and earpiece and USB audio are mutually exclusive.
                            mAudioDevices[DEVICE_WIRED_HEADSET] = false;
                            if (hasUsbAudio()) {
                                mAudioDevices[DEVICE_USB_AUDIO] = true;
                                mAudioDevices[DEVICE_EARPIECE] = false;
                            } else if (hasEarpiece()) {
                                mAudioDevices[DEVICE_EARPIECE] = true;
                                mAudioDevices[DEVICE_USB_AUDIO] = false;
                            }
                        }
                        break;
                    case STATE_PLUGGED:
                        synchronized (mLock) {
                            // Wired headset and earpiece and USB audio are mutually exclusive.
                            mAudioDevices[DEVICE_WIRED_HEADSET] = true;
                            mAudioDevices[DEVICE_EARPIECE] = false;
                            mAudioDevices[DEVICE_USB_AUDIO] = false;
                        }
                        break;
                    default:
                        loge("Invalid state");
                        break;
                }

                // Update the existing device selection, but only if a specific
                // device has already been selected explicitly.
                if (deviceHasBeenRequested()) {
                    updateDeviceActivation();
                } else if (DEBUG) {
                    reportUpdate();
                }
            }
        };

        // Note: the intent we register for here is sticky, so it'll tell us
        // immediately what the last action was (plugged or unplugged).
        // It will enable us to set the speakerphone correctly.
        ContextUtils.getApplicationContext().registerReceiver(mWiredHeadsetReceiver, filter);
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
                        synchronized (mLock) {
                            // Remove the BT device from the list of devices.
                            mAudioDevices[DEVICE_BLUETOOTH_HEADSET] = false;
                        }
                        break;
                    case android.bluetooth.BluetoothProfile.STATE_CONNECTED:
                        synchronized (mLock) {
                            // Add the BT device to the list of devices.
                            mAudioDevices[DEVICE_BLUETOOTH_HEADSET] = true;
                        }
                        break;
                    case android.bluetooth.BluetoothProfile.STATE_CONNECTING:
                        // Bluetooth service is switching from off to on.
                        break;
                    case android.bluetooth.BluetoothProfile.STATE_DISCONNECTING:
                        // Bluetooth service is switching from on to off.
                        break;
                    default:
                        loge("Invalid state");
                        break;
                }

                if (DEBUG) {
                    reportUpdate();
                }
            }
        };

        ContextUtils.getApplicationContext().registerReceiver(mBluetoothHeadsetReceiver, filter);
    }

    private void unregisterForBluetoothHeadsetIntentBroadcast() {
        ContextUtils.getApplicationContext().unregisterReceiver(mBluetoothHeadsetReceiver);
        mBluetoothHeadsetReceiver = null;
    }

    /**
     * Registers receiver for the broadcasted intent related the existence
     * of a BT SCO channel. Indicates if BT SCO streaming is on or off.
     */
    private void registerForBluetoothScoIntentBroadcast() {
        IntentFilter filter = new IntentFilter(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED);

        /** BroadcastReceiver implementation which handles changes in BT SCO. */
        mBluetoothScoReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                int state = intent.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_STATE,
                        AudioManager.SCO_AUDIO_STATE_DISCONNECTED);
                if (DEBUG) {
                    logd("BroadcastReceiver.onReceive: a=" + intent.getAction() + ", s=" + state
                            + ", sb=" + isInitialStickyBroadcast());
                }

                switch (state) {
                    case AudioManager.SCO_AUDIO_STATE_CONNECTED:
                        mBluetoothScoState = STATE_BLUETOOTH_SCO_ON;
                        break;
                    case AudioManager.SCO_AUDIO_STATE_DISCONNECTED:
                        if (mBluetoothScoState != STATE_BLUETOOTH_SCO_TURNING_OFF) {
                            // Bluetooth is probably powered off during the call.
                            // Update the existing device selection, but only if a specific
                            // device has already been selected explicitly.
                            if (deviceHasBeenRequested()) {
                                updateDeviceActivation();
                            }
                        }
                        mBluetoothScoState = STATE_BLUETOOTH_SCO_OFF;
                        break;
                    case AudioManager.SCO_AUDIO_STATE_CONNECTING:
                        // do nothing
                        break;
                    default:
                        loge("Invalid state");
                }
                if (DEBUG) {
                    reportUpdate();
                }
            }
        };

        ContextUtils.getApplicationContext().registerReceiver(mBluetoothScoReceiver, filter);
    }

    private void unregisterForBluetoothScoIntentBroadcast() {
        ContextUtils.getApplicationContext().unregisterReceiver(mBluetoothScoReceiver);
        mBluetoothScoReceiver = null;
    }

    /** Enables BT audio using the SCO audio channel. */
    private void startBluetoothSco() {
        if (!mHasBluetoothPermission) {
            return;
        }
        if (mBluetoothScoState == STATE_BLUETOOTH_SCO_ON
                || mBluetoothScoState == STATE_BLUETOOTH_SCO_TURNING_ON) {
            // Unable to turn on BT in this state.
            return;
        }

        // Check if audio is already routed to BT SCO; if so, just update
        // states but don't try to enable it again.
        if (mAudioManager.isBluetoothScoOn()) {
            mBluetoothScoState = STATE_BLUETOOTH_SCO_ON;
            return;
        }

        if (DEBUG) logd("startBluetoothSco: turning BT SCO on...");
        mBluetoothScoState = STATE_BLUETOOTH_SCO_TURNING_ON;
        mAudioManager.startBluetoothSco();
    }

    /** Disables BT audio using the SCO audio channel. */
    private void stopBluetoothSco() {
        if (!mHasBluetoothPermission) {
            return;
        }

        if (mBluetoothScoState != STATE_BLUETOOTH_SCO_ON
                && mBluetoothScoState != STATE_BLUETOOTH_SCO_TURNING_ON) {
            // No need to turn off BT in this state.
            return;
        }
        if (!mAudioManager.isBluetoothScoOn()) {
            // TODO(henrika): can we do anything else than logging here?
            loge("Unable to stop BT SCO since it is already disabled");
            mBluetoothScoState = STATE_BLUETOOTH_SCO_OFF;
            return;
        }

        if (DEBUG) logd("stopBluetoothSco: turning BT SCO off...");
        mBluetoothScoState = STATE_BLUETOOTH_SCO_TURNING_OFF;
        mAudioManager.stopBluetoothSco();
    }

    @Override
    protected void setAudioDevice(int device) {
        if (DEBUG) logd("setAudioDevice(device=" + device + ")");

        // Ensure that the Bluetooth SCO audio channel is always disabled
        // unless the BT headset device is selected.
        if (device == DEVICE_BLUETOOTH_HEADSET) {
            startBluetoothSco();
        } else {
            stopBluetoothSco();
        }

        switch (device) {
            case DEVICE_BLUETOOTH_HEADSET:
                break;
            case DEVICE_SPEAKERPHONE:
                setSpeakerphoneOn(true);
                break;
            case DEVICE_WIRED_HEADSET:
                setSpeakerphoneOn(false);
                break;
            case DEVICE_EARPIECE:
                setSpeakerphoneOn(false);
                break;
            case DEVICE_USB_AUDIO:
                setSpeakerphoneOn(false);
                break;
            default:
                loge("Invalid audio device selection");
                break;
        }
        reportUpdate();
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
        boolean hasUsbAudioCommInterface = false;
        for (int i = 0; i < device.getInterfaceCount(); ++i) {
            UsbInterface iface = device.getInterface(i);
            if (iface.getInterfaceClass() == UsbConstants.USB_CLASS_AUDIO
                    && iface.getInterfaceSubclass() == UsbConstants.USB_CLASS_COMM) {
                // There is at least one interface supporting audio communication.
                hasUsbAudioCommInterface = true;
                break;
            }
        }

        return hasUsbAudioCommInterface;
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
                    synchronized (mLock) {
                        // Wired headset and earpiece and USB audio are mutually exclusive.
                        if (!hasWiredHeadset()) {
                            mAudioDevices[DEVICE_USB_AUDIO] = true;
                            mAudioDevices[DEVICE_EARPIECE] = false;
                        }
                    }
                } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(intent.getAction())
                        && !hasUsbAudio()) {
                    // When a USB audio device is detached, we need to check if there is any other
                    // USB audio device still connected, e.g. through a USB hub.
                    // Only update the device list when there is no more USB audio device attached.
                    synchronized (mLock) {
                        if (!hasWiredHeadset()) {
                            mAudioDevices[DEVICE_USB_AUDIO] = false;
                            // Wired headset and earpiece and USB audio are mutually exclusive.
                            if (hasEarpiece()) {
                                mAudioDevices[DEVICE_EARPIECE] = true;
                            }
                        }
                    }
                }

                // Update the existing device selection, but only if a specific
                // device has already been selected explicitly.
                if (deviceHasBeenRequested()) {
                    updateDeviceActivation();
                } else if (DEBUG) {
                    reportUpdate();
                }
            }
        };

        IntentFilter filter = new IntentFilter();
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);

        ContextUtils.getApplicationContext().registerReceiver(mUsbAudioReceiver, filter);
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

    /**
     * Checks whether a wired headset is connected or not.
     * This is not a valid indication that audio playback is actually over
     * the wired headset as audio routing depends on other conditions. We
     * only use it as an early indicator (during initialization) of an attached
     * wired headset.
     */
    @Deprecated
    private boolean hasWiredHeadset() {
        return mAudioManager.isWiredHeadsetOn();
    }
}
