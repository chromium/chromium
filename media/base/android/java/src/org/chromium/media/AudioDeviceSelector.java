// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.AudioManager;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

abstract class AudioDeviceSelector {
    private static final String TAG = "media";

    protected static final boolean DEBUG = false;

    // Supported audio device types.
    protected static final int DEVICE_DEFAULT = -2;
    protected static final int DEVICE_INVALID = -1;
    protected static final int DEVICE_SPEAKERPHONE = 0;
    protected static final int DEVICE_WIRED_HEADSET = 1;
    protected static final int DEVICE_EARPIECE = 2;
    protected static final int DEVICE_BLUETOOTH_HEADSET = 3;
    protected static final int DEVICE_USB_AUDIO = 4;
    protected static final int DEVICE_COUNT = 5;

    // Maps audio device types to string values. This map must be in sync
    // with the device types above.
    // TODO(henrika): add support for proper detection of device names and
    // localize the name strings by using resource strings.
    // See http://crbug.com/333208 for details.
    protected static final String[] DEVICE_NAMES = new String[] {
            "Speakerphone",
            "Wired headset", // With or without microphone.
            "Headset earpiece", // Only available on mobile phones.
            "Bluetooth headset", // Requires BLUETOOTH permission.
            "USB audio", // Requires Android API level 21 (5.0).
    };

    // List of valid device types.
    protected static final Integer[] VALID_DEVICES = new Integer[] {
            DEVICE_SPEAKERPHONE,
            DEVICE_WIRED_HEADSET,
            DEVICE_EARPIECE,
            DEVICE_BLUETOOTH_HEADSET,
            DEVICE_USB_AUDIO,
    };

    // Lock to protect |mAudioDevices| and |mRequestedAudioDevice| which can
    // be accessed from the main thread and the audio manager thread.
    protected final Object mLock = new Object();

    // Id of the requested audio device. Can only be modified by
    // call to setDevice().
    protected int mRequestedAudioDevice = DEVICE_INVALID;

    // Contains a list of currently available audio devices.
    protected boolean[] mAudioDevices = new boolean[DEVICE_COUNT];

    protected final AudioManager mAudioManager;

    protected AudioDeviceSelector(AudioManager audioManager) {
        mAudioManager = audioManager;
    }

    /**
     * Initialized the AudioDeviceSelector.
     */
    public abstract void init();

    /**
     * Closes the AudioDeviceSelector. Must be called before destruction if init() was called.
     */
    public abstract void close();

    /**
     * Called when the AudioManager changes between MODE_IN_COMMUNICATION and MODE_NORMAL.
     *
     * @param on Whether we are entering MODE_IN_COMMUNICATION.
     */
    public abstract void setCommunicationAudioModeOn(boolean on);

    /**
     * Gets whether the speakerphone is currently active.
     */
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

    public AudioManagerAndroid.AudioDeviceName[] getAudioInputDeviceNames() {
        boolean devices[] = null;
        synchronized (mLock) {
            devices = mAudioDevices.clone();
        }
        List<String> list = new ArrayList<String>();
        AudioManagerAndroid.AudioDeviceName[] array =
                new AudioManagerAndroid.AudioDeviceName[getNumOfAudioDevices(devices)];
        int i = 0;
        for (int id = 0; id < DEVICE_COUNT; ++id) {
            if (devices[id]) {
                array[i] = new AudioManagerAndroid.AudioDeviceName(id, DEVICE_NAMES[id]);
                list.add(DEVICE_NAMES[id]);
                i++;
            }
        }
        if (DEBUG) logd("getAudioInputDeviceNames: " + list);
        return array;
    }

    public boolean setDevice(String deviceId) {
        int intDeviceId = deviceId.isEmpty() ? DEVICE_DEFAULT : Integer.parseInt(deviceId);

        if (intDeviceId == DEVICE_DEFAULT) {
            boolean devices[] = null;
            synchronized (mLock) {
                devices = mAudioDevices.clone();
                mRequestedAudioDevice = DEVICE_DEFAULT;
            }
            int defaultDevice = selectDefaultDevice(devices);
            setAudioDevice(defaultDevice);
            return true;
        }

        // A non-default device is specified. Verify that it is valid
        // device, and if so, start using it.
        List<Integer> validIds = Arrays.asList(VALID_DEVICES);
        if (!validIds.contains(intDeviceId) || !mAudioDevices[intDeviceId]) {
            return false;
        }
        synchronized (mLock) {
            mRequestedAudioDevice = intDeviceId;
        }
        setAudioDevice(intDeviceId);
        return true;
    }

    /**
     * Updates the active device given the current list of devices and
     * information about if a specific device has been selected or if
     * the default device is selected.
     */
    protected void updateDeviceActivation() {
        boolean devices[] = null;
        int requested = DEVICE_INVALID;
        synchronized (mLock) {
            requested = mRequestedAudioDevice;
            devices = mAudioDevices.clone();
        }
        if (requested == DEVICE_INVALID) {
            loge("Unable to activate device since no device is selected");
            return;
        }

        // Update default device if it has been selected explicitly, or
        // the selected device has been removed from the list.
        if (requested == DEVICE_DEFAULT || !devices[requested]) {
            // Get default device given current list and activate the device.
            int defaultDevice = selectDefaultDevice(devices);
            setAudioDevice(defaultDevice);
        } else {
            // Activate the selected device since we know that it exists in
            // the list.
            setAudioDevice(requested);
        }
    }

    /** Returns true if setDevice() has been called with a valid device id. */
    protected boolean deviceHasBeenRequested() {
        synchronized (mLock) {
            return (mRequestedAudioDevice != DEVICE_INVALID);
        }
    }

    /**
     * Use a special selection scheme if the default device is selected.
     * The "most unique" device will be selected; Wired headset first, then USB
     * audio device, then Bluetooth and last the speaker phone.
     */
    private static int selectDefaultDevice(boolean[] devices) {
        if (devices[DEVICE_WIRED_HEADSET]) {
            return DEVICE_WIRED_HEADSET;
        } else if (devices[DEVICE_USB_AUDIO]) {
            return DEVICE_USB_AUDIO;
        } else if (devices[DEVICE_BLUETOOTH_HEADSET]) {
            // TODO(henrika): possibly need improvements here if we are
            // in a state where Bluetooth is turning off.
            return DEVICE_BLUETOOTH_HEADSET;
        }
        return DEVICE_SPEAKERPHONE;
    }

    /** Returns number of available devices */
    private static int getNumOfAudioDevices(boolean[] devices) {
        int count = 0;
        for (int i = 0; i < DEVICE_COUNT; ++i) {
            if (devices[i]) ++count;
        }
        return count;
    }

    /**
     * For now, just log the state change but the idea is that we should
     * notify a registered state change listener (if any) that there has
     * been a change in the state.
     * TODO(henrika): add support for state change listener.
     */
    protected void reportUpdate() {
        if (!DEBUG) return;

        synchronized (mLock) {
            List<String> devices = new ArrayList<String>();
            for (int i = 0; i < DEVICE_COUNT; ++i) {
                if (mAudioDevices[i]) devices.add(DEVICE_NAMES[i]);
            }
            logd("reportUpdate: requested=" + mRequestedAudioDevice + ", devices=" + devices);
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
