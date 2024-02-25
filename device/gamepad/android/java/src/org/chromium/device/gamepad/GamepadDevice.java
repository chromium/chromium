// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.gamepad;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.CombinedVibration;
import android.os.SystemClock;
import android.os.VibrationEffect;
import android.os.VibratorManager;
import android.view.InputDevice;
import android.view.InputDevice.MotionRange;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.annotation.VisibleForTesting;

import java.util.Arrays;
import java.util.BitSet;
import java.util.List;

/** Manages information related to each connected gamepad device. */
@SuppressLint("NewApi") // VibratorManager requires API level 31.
class GamepadDevice {
    // Axis ids are used as indices which are empirically always smaller than 256 so this allows
    // us to create cheap associative arrays.
    @VisibleForTesting static final int MAX_RAW_AXIS_VALUES = 256;

    // Keycodes are used as indices which are empirically always smaller than 256 so this allows
    // us to create cheap associative arrays.
    @VisibleForTesting static final int MAX_RAW_BUTTON_VALUES = 256;

    // Allow for devices that have more buttons than the Standard Gamepad.
    static final int MAX_BUTTON_INDEX = CanonicalButtonIndex.COUNT;

    // Minimum and maximum scancodes for extra gamepad buttons. Android does not assign KeyEvent
    // keycodes for these buttons.
    static final int MIN_BTN_TRIGGER_HAPPY = 0x2c0;
    static final int MAX_BTN_TRIGGER_HAPPY = 0x2cf;

    /** Keycodes which might be mapped by {@link GamepadMappings}. Keep sorted by keycode. */
    @VisibleForTesting
    static final int RELEVANT_KEYCODES[] = {
        KeyEvent.KEYCODE_DPAD_UP, // 0x13
        KeyEvent.KEYCODE_DPAD_DOWN, // 0x14
        KeyEvent.KEYCODE_DPAD_LEFT, // 0x15
        KeyEvent.KEYCODE_DPAD_RIGHT, // 0x16
        KeyEvent.KEYCODE_BUTTON_A, // 0x60
        KeyEvent.KEYCODE_BUTTON_B, // 0x61
        KeyEvent.KEYCODE_BUTTON_C, // 0x62
        KeyEvent.KEYCODE_BUTTON_X, // 0x63
        KeyEvent.KEYCODE_BUTTON_Y, // 0x64
        KeyEvent.KEYCODE_BUTTON_Z, // 0x65
        KeyEvent.KEYCODE_BUTTON_L1, // 0x66
        KeyEvent.KEYCODE_BUTTON_R1, // 0x67
        KeyEvent.KEYCODE_BUTTON_L2, // 0x68
        KeyEvent.KEYCODE_BUTTON_R2, // 0x69
        KeyEvent.KEYCODE_BUTTON_THUMBL, // 0x6a
        KeyEvent.KEYCODE_BUTTON_THUMBR, // 0x6b
        KeyEvent.KEYCODE_BUTTON_START, // 0x6c
        KeyEvent.KEYCODE_BUTTON_SELECT, // 0x6d
        KeyEvent.KEYCODE_BUTTON_MODE, // 0x6e
        KeyEvent.KEYCODE_MEDIA_RECORD // 0x82
    };

    // The ID for the  Vibrator  that controls the strong rumble effect.
    static final int FF_STRONG_MAGNITUDE_CHANNEL_IDX = 0;

    // The ID for the  Vibrator  that controls the weak rumble effect.
    static final int FF_WEAK_MAGNITUDE_CHANNEL_IDX = 1;

    // The maximum effect length, in milliseconds.
    // See  device::AbstractHapticGamepad::GetMaxEffectDurationMillis .
    static final long VIBRATION_DEFAULT_DURATION_MILLIS = 5000;

    // @see VibrationEffect#MAX_AMPLITUDE
    static final int VIBRATION_MAX_AMPLITUDE = 255;

    // An id for the gamepad.
    private int mDeviceId;
    // The index of the gamepad in the Navigator.
    private int mDeviceIndex;
    // The vendor ID of the gamepad, or zero if the gamepad does not have a vendor ID.
    private int mDeviceVendorId;
    // The product ID of the gamepad, or zero if the gamepad does not have a product ID.
    private int mDeviceProductId;

    // Last time the data for this gamepad was updated.
    private long mTimestamp;

    // Array of values for all axes of the gamepad.
    // All axis values must be linearly normalized to the range [-1.0 .. 1.0].
    // As appropriate, -1.0 should correspond to "up" or "left", and 1.0
    // should correspond to "down" or "right".
    private final float[] mAxisValues = new float[CanonicalAxisIndex.COUNT];

    // Array of values for all buttons of the gamepad. All button values must be
    // linearly normalized to the range [0.0 .. 1.0]. 0.0 should correspond to
    // a neutral, unpressed state and 1.0 should correspond to a pressed state.
    // Allocate enough room for all Standard Gamepad buttons plus two extra
    // buttons.
    private final float[] mButtonsValues = new float[MAX_BUTTON_INDEX + 2];

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

    // True if the gamepad supports "dual-rumble" vibration effects.
    private boolean mSupportsDualRumble;
    private VibratorManager mVibratorManager;

    GamepadDevice(int index, InputDevice inputDevice) {
        mDeviceIndex = index;
        mDeviceId = inputDevice.getId();
        mDeviceName = inputDevice.getName();
        mDeviceVendorId = inputDevice.getVendorId();
        mDeviceProductId = inputDevice.getProductId();
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

        // Get the set of relevant buttons which exist on the gamepad.
        final int maxKeycode = RELEVANT_KEYCODES[RELEVANT_KEYCODES.length - 1];
        BitSet buttons = new BitSet(maxKeycode);
        boolean[] presentKeys = inputDevice.hasKeys(RELEVANT_KEYCODES);
        for (int j = 0; j < RELEVANT_KEYCODES.length; ++j) {
            if (presentKeys[j]) {
                buttons.set(RELEVANT_KEYCODES[j]);
            }
        }

        mMappings = GamepadMappings.getMappings(inputDevice, mAxes, buttons);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibratorManager vibratorManager = inputDevice.getVibratorManager();
            int[] vibratorIds = vibratorManager.getVibratorIds();
            if (vibratorIds.length >= 2) {
                mSupportsDualRumble =
                        vibratorManager
                                        .getVibrator(vibratorIds[FF_STRONG_MAGNITUDE_CHANNEL_IDX])
                                        .hasAmplitudeControl()
                                && vibratorManager
                                        .getVibrator(vibratorIds[FF_WEAK_MAGNITUDE_CHANNEL_IDX])
                                        .hasAmplitudeControl();
                if (mSupportsDualRumble) {
                    mVibratorManager = vibratorManager;
                }
            }
        }
    }

    /** Updates the axes and buttons mapping of a gamepad device to a standard gamepad format. */
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
     * @return Vendor Id of the gamepad device.
     * It can be zero if gamepad doesn't have a vendor ID.
     */
    public int getVendorId() {
        return mDeviceVendorId;
    }

    /**
     * @return The product ID of the gamepad.
     * It can be zero if gamepad doesn't have a product ID.
     */
    public int getProductId() {
        return mDeviceProductId;
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
     * @return The number of mapped buttons.
     */
    public int getButtonsLength() {
        return mMappings.getButtonsLength();
    }

    /**
     * @return if gamepad support dual rumble
     */
    public boolean supportsDualRumble() {
        return mSupportsDualRumble;
    }

    /**
     * Play a two-channel VibrationEffect with the specified magnitudes on
     * the strong and weak vibration channels. If both magnitudes are zero,
     * cancel vibration.
     */
    public void doVibration(double strongMagnitude, double weakMagnitude) {
        int strong = scaleMagnitude(strongMagnitude);
        int weak = scaleMagnitude(weakMagnitude);
        if (strong == 0 && weak == 0) {
            cancelVibration();
            return;
        }
        CombinedVibration.ParallelCombination effect = CombinedVibration.startParallel();
        if (strong > 0) {
            effect.addVibrator(
                    FF_STRONG_MAGNITUDE_CHANNEL_IDX,
                    VibrationEffect.createOneShot(VIBRATION_DEFAULT_DURATION_MILLIS, strong));
        }
        if (weak > 0) {
            effect.addVibrator(
                    FF_WEAK_MAGNITUDE_CHANNEL_IDX,
                    VibrationEffect.createOneShot(VIBRATION_DEFAULT_DURATION_MILLIS, weak));
        }
        mVibratorManager.vibrate(effect.combine());
    }

    private int scaleMagnitude(double magnitude) {
        magnitude = Math.max(0.0, Math.min(1.0, magnitude));
        return (int) Math.round(magnitude * VIBRATION_MAX_AMPLITUDE);
    }

    /** Stop all vibration for this gamepad. */
    public void cancelVibration() {
        mVibratorManager.cancel();
    }

    /**
     * Reset the axes and buttons data of the gamepad device every time gamepad data access is
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
        // Extra gamepad and joystick buttons use Linux scancodes starting from BTN_TRIGGER_HAPPY
        // but don't have specific Android keycodes and are mapped as KEYCODE_UNKNOWN. Handle the
        // first 16 extra buttons as if they had KEYCODE_BUTTON_# keycodes.
        int keyCode = event.getKeyCode();
        int scanCode = event.getScanCode();
        if (keyCode == KeyEvent.KEYCODE_UNKNOWN
                && scanCode >= MIN_BTN_TRIGGER_HAPPY
                && scanCode <= MAX_BTN_TRIGGER_HAPPY) {
            keyCode = KeyEvent.KEYCODE_BUTTON_1 + scanCode - MIN_BTN_TRIGGER_HAPPY;
        }

        // Ignore the event if it is not for a gamepad key.
        if (!GamepadList.isGamepadEvent(event)) return false;
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
