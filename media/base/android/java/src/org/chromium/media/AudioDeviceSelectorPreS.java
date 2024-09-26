// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.media.AudioManager;

import org.chromium.base.ContextUtils;

class AudioDeviceSelectorPreS extends AudioDeviceSelector {
    private static final String TAG = "media";

    // Bluetooth audio SCO states. Example of valid state sequence:
    // SCO_INVALID -> SCO_TURNING_ON -> SCO_ON -> SCO_TURNING_OFF -> SCO_OFF.
    private static final int STATE_BLUETOOTH_SCO_INVALID = -1;
    private static final int STATE_BLUETOOTH_SCO_OFF = 0;
    private static final int STATE_BLUETOOTH_SCO_ON = 1;
    private static final int STATE_BLUETOOTH_SCO_TURNING_ON = 2;
    private static final int STATE_BLUETOOTH_SCO_TURNING_OFF = 3;

    // Stores the audio states related to Bluetooth SCO audio, where some
    // states are needed to keep track of intermediate states while the SCO
    // channel is enabled or disabled (switching state can take a few seconds).
    private int mBluetoothScoState = STATE_BLUETOOTH_SCO_INVALID;

    private boolean mHasBluetoothPermission;

    private boolean[] mDeviceExistence = new boolean[Devices.DEVICE_COUNT];

    public AudioDeviceSelectorPreS(AudioManager audioManager) {
        super(audioManager);
    }

    // Broadcast receiver for Bluetooth SCO broadcasts.
    // Utilized to detect if BT SCO streaming is on or off.
    private BroadcastReceiver mBluetoothScoReceiver;

    @Override
    public void init() {
        mHasBluetoothPermission = hasPermission(android.Manifest.permission.BLUETOOTH);

        mDeviceListener.init(mHasBluetoothPermission);

        if (mHasBluetoothPermission) registerForBluetoothScoIntentBroadcast();
    }

    @Override
    public void close() {
        mDeviceListener.close();
        if (mHasBluetoothPermission) unregisterForBluetoothScoIntentBroadcast();
    }

    @Override
    public void setCommunicationAudioModeOn(boolean on) {
        if (!on) {
            stopBluetoothSco();
            mDeviceStates.clearRequestedDevice();
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

    @Override
    public boolean[] getAvailableDevices_Locked() {
        boolean[] availableDevices = mDeviceExistence.clone();

        // Wired headset, USB audio and earpiece are mutually exclusive, and
        // prioritized in that order.
        if (availableDevices[Devices.ID_WIRED_HEADSET]) {
            availableDevices[Devices.ID_USB_AUDIO] = false;
            availableDevices[Devices.ID_EARPIECE] = false;
        } else if (availableDevices[Devices.ID_USB_AUDIO]) {
            availableDevices[Devices.ID_EARPIECE] = false;
        }

        return availableDevices;
    }

    @Override
    public void setDeviceExistence_Locked(int deviceId, boolean exists) {
        mDeviceExistence[deviceId] = exists;
    }

    /** Checks if the process has as specified permission or not. */
    private boolean hasPermission(String permission) {
        return ContextUtils.getApplicationContext().checkSelfPermission(permission)
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Registers receiver for the broadcasted intent related the existence
     * of a BT SCO channel. Indicates if BT SCO streaming is on or off.
     */
    private void registerForBluetoothScoIntentBroadcast() {
        IntentFilter filter = new IntentFilter(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED);

        /* BroadcastReceiver implementation which handles changes in BT SCO. */
        mBluetoothScoReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        int state =
                                intent.getIntExtra(
                                        AudioManager.EXTRA_SCO_AUDIO_STATE,
                                        AudioManager.SCO_AUDIO_STATE_DISCONNECTED);
                        if (DEBUG) {
                            logd(
                                    "BroadcastReceiver.onReceive: a="
                                            + intent.getAction()
                                            + ", s="
                                            + state
                                            + ", sb="
                                            + isInitialStickyBroadcast());
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
                                    maybeUpdateSelectedDevice();
                                }
                                mBluetoothScoState = STATE_BLUETOOTH_SCO_OFF;
                                break;
                            case AudioManager.SCO_AUDIO_STATE_CONNECTING:
                                // do nothing
                                break;
                            default:
                                break;
                        }
                    }
                };

        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(), mBluetoothScoReceiver, filter);
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
        if (device == Devices.ID_BLUETOOTH_HEADSET) {
            startBluetoothSco();
        } else {
            stopBluetoothSco();
        }

        switch (device) {
            case Devices.ID_BLUETOOTH_HEADSET:
                break;
            case Devices.ID_SPEAKERPHONE:
                setSpeakerphoneOn(true);
                break;
            case Devices.ID_WIRED_HEADSET:
                setSpeakerphoneOn(false);
                break;
            case Devices.ID_EARPIECE:
                setSpeakerphoneOn(false);
                break;
            case Devices.ID_USB_AUDIO:
                setSpeakerphoneOn(false);
                break;
            default:
                loge("Invalid audio device selection");
                break;
        }
    }
}
