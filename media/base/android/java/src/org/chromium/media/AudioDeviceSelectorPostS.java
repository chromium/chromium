// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.Manifest;
import android.content.pm.PackageManager;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Process;

import androidx.annotation.RequiresApi;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

@RequiresApi(Build.VERSION_CODES.S)
class AudioDeviceSelectorPostS extends AudioDeviceSelector {
    private static final String TAG = "media";

    private boolean mHasBluetoothConnectPermission;

    public AudioDeviceSelectorPostS(AudioManager audioManager) {
        super(audioManager);
    }

    private static List<Integer> getTargetTypesFromId(int deviceId) {
        List<Integer> types = new ArrayList<Integer>();

        switch (deviceId) {
            case Devices.ID_SPEAKERPHONE:
                types.add(AudioDeviceInfo.TYPE_BUILTIN_SPEAKER);
                break;
            case Devices.ID_WIRED_HEADSET:
                types.add(AudioDeviceInfo.TYPE_WIRED_HEADSET);
                types.add(AudioDeviceInfo.TYPE_WIRED_HEADPHONES);
                break;
            case Devices.ID_EARPIECE:
                types.add(AudioDeviceInfo.TYPE_BUILTIN_EARPIECE);
                break;
            case Devices.ID_BLUETOOTH_HEADSET:
                // Add Bluteooth LE Audio devices.
                types.add(AudioDeviceInfo.TYPE_BLE_HEADSET);
                types.add(AudioDeviceInfo.TYPE_BLE_SPEAKER);

                // Add "classic" bluetooth devices.
                types.add(AudioDeviceInfo.TYPE_BLUETOOTH_SCO);
                types.add(AudioDeviceInfo.TYPE_BLUETOOTH_A2DP);
                break;
            case Devices.ID_USB_AUDIO:
                types.add(AudioDeviceInfo.TYPE_USB_HEADSET);
                types.add(AudioDeviceInfo.TYPE_USB_DEVICE);
                break;
        }

        return types;
    }

    @Override
    public void init() {
        mHasBluetoothConnectPermission =
                ApiCompatibilityUtils.checkPermission(
                                ContextUtils.getApplicationContext(),
                                Manifest.permission.BLUETOOTH_CONNECT,
                                Process.myPid(),
                                Process.myUid())
                        == PackageManager.PERMISSION_GRANTED;

        if (!mHasBluetoothConnectPermission) {
            Log.w(TAG, "BLUETOOTH_CONNECT permission is missing.");
        }

        mDeviceListener.init(mHasBluetoothConnectPermission);
    }

    @Override
    public void close() {
        mDeviceListener.close();
    }

    @Override
    public void setCommunicationAudioModeOn(boolean on) {
        if (on) {
            // TODO(crbug.com/40222537): Prompt for BLUETOOTH_CONNECT permission at this point if we
            // don't have it.
        } else {
            mDeviceStates.clearRequestedDevice();
            mAudioManager.clearCommunicationDevice();
        }
    }

    @Override
    public boolean isSpeakerphoneOn() {
        AudioDeviceInfo currentDevice = mAudioManager.getCommunicationDevice();
        return currentDevice != null
                && currentDevice.getType() == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;
    }

    @Override
    public void setSpeakerphoneOn(boolean on) {
        boolean isCurrentlyOn = isSpeakerphoneOn();

        if (isCurrentlyOn == on) return;

        if (on) {
            setAudioDevice(Devices.ID_SPEAKERPHONE);
        } else {
            // Turn speakerphone OFF.
            mAudioManager.clearCommunicationDevice();
            maybeUpdateSelectedDevice();
        }
    }

    @Override
    public boolean[] getAvailableDevices_Locked() {
        List<AudioDeviceInfo> communicationDevices =
                mAudioManager.getAvailableCommunicationDevices();

        boolean[] availableDevices = new boolean[Devices.DEVICE_COUNT];

        for (AudioDeviceInfo device : communicationDevices) {
            switch (device.getType()) {
                case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
                    availableDevices[Devices.ID_SPEAKERPHONE] = true;
                    break;

                case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                    availableDevices[Devices.ID_WIRED_HEADSET] = true;
                    break;

                case AudioDeviceInfo.TYPE_USB_DEVICE:
                case AudioDeviceInfo.TYPE_USB_HEADSET:
                    availableDevices[Devices.ID_USB_AUDIO] = true;
                    break;

                case AudioDeviceInfo.TYPE_BLE_HEADSET:
                case AudioDeviceInfo.TYPE_BLE_SPEAKER:
                case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
                case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
                    availableDevices[Devices.ID_BLUETOOTH_HEADSET] = true;
                    break;

                case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE:
                    availableDevices[Devices.ID_EARPIECE] = true;
                    break;
            }
        }

        return availableDevices;
    }

    public AudioDeviceInfo getMatchingCommunicationDevice(List<Integer> targetTypes) {
        // Despite supporting 2 BT devices being connected at once,
        // `getAvailableCommunicationDevices()` only seems to return the last connected BT device.
        // There should therefore never be a conflict between choosing between BT headsets.
        List<AudioDeviceInfo> availableDevices = mAudioManager.getAvailableCommunicationDevices();

        for (AudioDeviceInfo device : availableDevices) {
            if (targetTypes.contains(device.getType())) return device;
        }

        return null;
    }

    @Override
    protected void setAudioDevice(int deviceId) {
        if (!DeviceHelpers.isDeviceValid(deviceId)) return;

        AudioDeviceInfo targetDevice =
                getMatchingCommunicationDevice(getTargetTypesFromId(deviceId));

        if (targetDevice != null) {
            boolean result = mAudioManager.setCommunicationDevice(targetDevice);
            if (!result) {
                loge("Error setting communication device");
            }
        } else {
            loge("Couldn't find available device for: " + DeviceHelpers.getDeviceName(deviceId));
        }
    }
}
