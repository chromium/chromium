// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.gamepad;

import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputDevice.MotionRange;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.annotation.VisibleForTesting;

import java.util.Arrays;
import java.util.List;

/**
 * Manages information related to each connected gamepad device.
 */
class GamepadDevice {
    // Axis ids are used as indices which are empirically always smaller than 256 so this allows
    // us to create cheap associative arrays.
    @VisibleForTesting
    static final int MAX_RAW_AXIS_VALUES = 256;

    // Keycodes are used as indices which are empirically always smaller than 256 so this allows
    // us to create cheap associative arrays.
    @VisibleForTesting
    static final int MAX_RAW_BUTTON_VALUES = 256;

    // An id for the gamepad.
    private int mDeviceId;
    // The index of the gamepad in the Navigator.
    private int mDeviceIndex;
    // Last time the data for this gamepad was updated.
    private long mTimestamp;

    // Array of values for all axes of the gamepad.
    // All axis values must be linearly normalized to the range [-1.0 .. 1.0].
    // As appropriate, -1.0 should correspond to "up" or "left", and 1.0
    // should correspond to "down" or "right".
    private final float[] mAxisValues = new float[CanonicalAxisIndex.COUNT];

    private final float[] mButtonsValues = new float[CanonicalButtonIndex.COUNT];

    // When the user agent recognizes the attached inputDevice, it is recommended
    // that it be remapped to a canonical ordering when possible. Devices that are
    // not recognized should still be exposed in their raw form. Therefore we must
    // pass the raw Button and raw Axis values.
    private final float[] mRawButtons = new float[MAX_RAW_BUTTON_VALUES];
    private final float[] mRawAxes = new float[MAX_RAW_AXIS_VALUES];

    // An identification string for the gamepad.
    private String mDeviceName;

    // Array of axes ids.
    private int[] mAxes;

    // Mappings to canonical gamepad
    private GamepadMappings mMappings;

    GamepadDevice(int index, InputDevice inputDevice) {
        mDeviceIndex = index;
        mDeviceId = inputDevice.getId();
        mDeviceName = inputDevice.getName();
        mTimestamp = SystemClock.uptimeMillis();
        // Get axis ids and initialize axes values.
        final List<MotionRange> ranges = inputDevice.getMotionRanges();
        mAxes = new int[ranges.size()];
        int i = 0;
        for (MotionRange range : ranges) {
            if ((range.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
                int axis = range.getAxis();
                assert axis < MAX_RAW_AXIS_VALUES;
                mAxes[i++] = axis;
            }
        }
        mMappings = GamepadMappings.getMappings(inputDevice, mAxes);
    }

    /**
     * Updates the axes and buttons maping of a gamepad device to a standard gamepad format.
     */
    public void updateButtonsAndAxesMapping() {
        mMappings.mapToStandardGamepad(mAxisValues, mButtonsValues, mRawAxes, mRawButtons);
    }

    /**
     * @return Device Id of the gamepad device.
     */
    public int getId() {
        return mDeviceId;
    }

    /**
     * @return Mapping status of the gamepad device.
     */
    public boolean isStandardGamepad() {
        return mMappings.isStandard();
    }

    /**
     * @return Device name of the gamepad device.
     */
    public String getName() {
        return mDeviceName;
    }

    /**
     * @return Device index of the gamepad device.
     */
    public int getIndex() {
        return mDeviceIndex;
    }

    /**
     * @return The timestamp when the gamepad device was last interacted.
     */
    public long getTimestamp() {
        return mTimestamp;
    }

    /**
     * @return The axes state of the gamepad device.
     */
    public float[] getAxes() {
        return mAxisValues;
    }

    /**
     * @return The buttons state of the gamepad device.
     */
    public float[] getButtons() {
        return mButtonsValues;
    }

    /**
     * Reset the axes and buttons data of the gamepad device everytime gamepad data access is
     * paused.
     */
    public void clearData() {
        Arrays.fill(mAxisValues, 0);
        Arrays.fill(mRawAxes, 0);
        Arrays.fill(mButtonsValues, 0);
        Arrays.fill(mRawButtons, 0);
    }

    /**
     * Handles key event from the gamepad device.
     * @return True if the key event from the gamepad device has been consumed.
     */
    public boolean handleKeyEvent(KeyEvent event) {
        // Ignore event if it is not for standard gamepad key.
        if (!GamepadList.isGamepadEvent(event)) return false;
        int keyCode = event.getKeyCode();
        assert keyCode < MAX_RAW_BUTTON_VALUES;
        // Button value 0.0 must mean fully unpressed, and 1.0 must mean fully pressed.
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            mRawButtons[keyCode] = 1.0f;
        } else if (event.getAction() == KeyEvent.ACTION_UP) {
            mRawButtons[keyCode] = 0.0f;
        }
        mTimestamp = event.getEventTime();

        return true;
    }

    /**
     * Handles motion event from the gamepad device.
     * @return True if the motion event from the gamepad device has been consumed.
     */
    public boolean handleMotionEvent(MotionEvent event) {
        // Ignore event if it is not a standard gamepad motion event.
        if (!GamepadList.isGamepadEvent(event)) return false;
        // Update axes values.
        for (int i = 0; i < mAxes.length; i++) {
            int axis = mAxes[i];
            mRawAxes[axis] = event.getAxisValue(axis);
        }
        mTimestamp = event.getEventTime();
        return true;
    }
}
