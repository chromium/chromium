// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.AudioManager;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

abstract class AudioDeviceSelector {
    private static final String TAG = "media";

    protected static final boolean DEBUG = false;

    protected Devices mDeviceStates = new Devices();

    protected final AudioDeviceListener mDeviceListener;

    protected final AudioManager mAudioManager;

    protected AudioDeviceSelector(AudioManager audioManager) {
        mAudioManager = audioManager;

        mDeviceListener = new AudioDeviceListener(mDeviceStates);
    }

    /** Initialized the AudioDeviceSelector. */
    public abstract void init();

    /** Closes the AudioDeviceSelector. Must be called before destruction if init() was called. */
    public abstract void close();

    /**
     * Called when the AudioManager changes between MODE_IN_COMMUNICATION and MODE_NORMAL.
     *
     * @param on Whether we are entering MODE_IN_COMMUNICATION.
     */
    public abstract void setCommunicationAudioModeOn(boolean on);

    /** Gets whether the speakerphone is currently active. */
    public abstract boolean isSpeakerphoneOn();

    /**
     * Sets speakerphone on or off.
     *
     * @param on The desired speakerphone state.
     */
    public abstract void setSpeakerphoneOn(boolean on);

    /**
     * Changes selection of the currently active audio device.
     *
     * @param device Specifies the selected audio device.
     */
    protected abstract void setAudioDevice(int device);

    public abstract boolean[] getAvailableDevices_Locked();

    public void setDeviceExistence_Locked(int deviceId, boolean exists) {
        // Overridden by AudioDeviceSelectorPreS.
    }

    public AudioManagerAndroid.AudioDeviceName[] getAudioInputDeviceNames() {
        return mDeviceStates.getAudioInputDeviceNames();
    }

    /**
     * Sets the passed ID as the active device if it is available. Also sets the given
     * ID as the requested device ID, which will be prioritized when a device change
     * occurs and maybeUpdateSelectedDevice() is called.
     */
    public boolean selectDevice(String stringDeviceId) {
        int deviceId = DeviceHelpers.parseStringId(stringDeviceId);

        int nextDevice = mDeviceStates.setRequestedDeviceIdAndGetNextId(deviceId);

        if (DEBUG) logd("selectDevice: id=" + DeviceHelpers.getDeviceName(nextDevice));

        // `deviceId` is invalid, or its corresponding device is not available.
        if (nextDevice == Devices.ID_INVALID) return false;

        setAudioDevice(nextDevice);
        return true;
    }

    /**
     * Updates the active device given the current list of devices and
     * information about if a specific device has been selected or if
     * the default device is selected.
     */
    protected void maybeUpdateSelectedDevice() {
        int nextDevice = mDeviceStates.getNextDeviceIfRequested();

        if (DEBUG) logd("maybeUpdateSelectedDevice: id=" + DeviceHelpers.getDeviceName(nextDevice));

        // No device was explicitly requested.
        if (nextDevice == Devices.ID_INVALID) return;

        setAudioDevice(nextDevice);
    }

    // Collection of static helpers.
    protected static class DeviceHelpers {
        // Maps audio device types to string values. This map must be in sync
        // with the Devices.ID_* below.
        // TODO(henrika): add support for proper detection of device names and
        // localize the name strings by using resource strings.
        // See http://crbug.com/333208 for details.
        public static final String[] DEVICE_NAMES =
                new String[] {
                    "Speakerphone",
                    "Wired headset", // With or without microphone.
                    "Headset earpiece", // Only available on mobile phones.
                    "Bluetooth headset", // Requires BLUETOOTH permission.
                    "USB audio",
                };

        private static final int ID_VALID_LOWER_BOUND = Devices.ID_SPEAKERPHONE;
        private static final int ID_VALID_UPPER_BOUND = Devices.ID_USB_AUDIO;

        public static String getDeviceName(int deviceId) {
            if (deviceId == Devices.ID_INVALID) return "invalid-ID";

            if (deviceId == Devices.ID_DEFAULT) return "default-device";

            return DEVICE_NAMES[deviceId];
        }

        /**
         * Use a special selection scheme if the default device is selected.
         * The "most unique" device will be selected; Wired headset first, then USB
         * audio device, then Bluetooth and last the speaker phone.
         */
        public static int selectDefaultDevice(boolean[] devices) {
            if (devices[Devices.ID_WIRED_HEADSET]) {
                return Devices.ID_WIRED_HEADSET;
            }

            if (devices[Devices.ID_USB_AUDIO]) {
                return Devices.ID_USB_AUDIO;
            }

            if (devices[Devices.ID_BLUETOOTH_HEADSET]) {
                return Devices.ID_BLUETOOTH_HEADSET;
            }

            return Devices.ID_SPEAKERPHONE;
        }

        public static boolean isDeviceValid(int deviceId) {
            return deviceId >= ID_VALID_LOWER_BOUND && deviceId <= ID_VALID_UPPER_BOUND;
        }

        public static boolean isDeviceValidOrDefault(int deviceId) {
            return deviceId == Devices.ID_DEFAULT || isDeviceValid(deviceId);
        }

        public static int getActiveDeviceCount(boolean[] devices) {
            int count = 0;

            for (boolean device : devices) {
                if (device) ++count;
            }

            return count;
        }

        public static int parseStringId(String stringDeviceId) {
            return stringDeviceId.isEmpty() ? Devices.ID_DEFAULT : Integer.parseInt(stringDeviceId);
        }
    }

    public class Devices {
        // Supported audio device types.
        public static final int ID_DEFAULT = -2;
        public static final int ID_INVALID = -1;
        public static final int ID_SPEAKERPHONE = 0;
        public static final int ID_WIRED_HEADSET = 1;
        public static final int ID_EARPIECE = 2;
        public static final int ID_BLUETOOTH_HEADSET = 3;
        public static final int ID_USB_AUDIO = 4;
        public static final int DEVICE_COUNT = 5;

        private Object mLock = new Object();

        private int mRequestedAudioDevice = ID_INVALID;

        /**
         * Sets the whether a device exists.
         *
         * @param deviceId The ID of the device.
         * @param exists Whether or not the device exists.
         */
        public void setDeviceExistence(int deviceId, boolean exists) {
            if (!DeviceHelpers.isDeviceValid(deviceId)) return;

            if (DEBUG) logd("Setting [" + DeviceHelpers.getDeviceName(deviceId) + "]=" + exists);

            synchronized (mLock) {
                setDeviceExistence_Locked(deviceId, exists);
            }
        }

        /** Called when an available device maybe became invalid or vice versa. */
        public void onPotentialDeviceStatusChange() {
            maybeUpdateSelectedDevice();
        }

        /**
         * Sets the requested device, and gets the device ID that should be currently selected.
         *
         * @param deviceId The requested device ID (including the DEVICE_DEFAULT ID).
         * @return The ID of the audio device which should be selected, or DEVICE_INVALID if the
         *         requested ID is unavailable.
         */
        public int setRequestedDeviceIdAndGetNextId(int deviceId) {
            if (!DeviceHelpers.isDeviceValidOrDefault(deviceId)) return Devices.ID_INVALID;

            synchronized (mLock) {
                mRequestedAudioDevice = deviceId;
                boolean[] availableDevices = getAvailableDevices_Locked();

                // Handle the default device request.
                if (deviceId == Devices.ID_DEFAULT) {
                    return DeviceHelpers.selectDefaultDevice(availableDevices);
                }

                // A non-default device is specified. Verify that it is available before using it.
                return availableDevices[deviceId] ? mRequestedAudioDevice : ID_INVALID;
            }
        }

        /**
         * Gets the ID of the device which should be currently selected, or ID_INVALID if no
         * device was ever requested.
         */
        public int getNextDeviceIfRequested() {
            synchronized (mLock) {
                if (mRequestedAudioDevice == ID_INVALID) return ID_INVALID;

                boolean[] availableDevices = getAvailableDevices_Locked();

                if (mRequestedAudioDevice == ID_DEFAULT
                        || !availableDevices[mRequestedAudioDevice]) {
                    return DeviceHelpers.selectDefaultDevice(availableDevices);
                }

                return mRequestedAudioDevice;
            }
        }

        /** Returns the list of currently available devices, to be used by the native side. */
        public AudioManagerAndroid.AudioDeviceName[] getAudioInputDeviceNames() {
            boolean devices[] = null;
            synchronized (mLock) {
                devices = getAvailableDevices_Locked();
            }
            List<String> list = new ArrayList<String>();

            int activeDeviceCount = DeviceHelpers.getActiveDeviceCount(devices);

            AudioManagerAndroid.AudioDeviceName[] array =
                    new AudioManagerAndroid.AudioDeviceName[activeDeviceCount];

            int i = 0;
            for (int id = 0; id < devices.length; ++id) {
                if (devices[id]) {
                    array[i] =
                            new AudioManagerAndroid.AudioDeviceName(
                                    id, DeviceHelpers.DEVICE_NAMES[id]);
                    list.add(DeviceHelpers.DEVICE_NAMES[id]);
                    i++;
                }
            }
            if (DEBUG) logd("getAudioInputDeviceNames: " + list);
            return array;
        }

        public void clearRequestedDevice() {
            if (DEBUG) logd("Clearing requested device");
            synchronized (mLock) {
                mRequestedAudioDevice = ID_INVALID;
            }
        }
    }

    /** Trivial helper method for debug logging */
    protected static void logd(String msg) {
        Log.d(TAG, msg);
    }

    /** Trivial helper method for error logging */
    protected static void loge(String msg) {
        Log.e(TAG, msg);
    }
}
