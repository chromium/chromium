// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.gamepad;

import android.os.Build;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;

import java.util.BitSet;

/** Class to manage mapping information related to each supported gamepad controller device. */
@JNINamespace("content")
abstract class GamepadMappings {
    @VisibleForTesting
    static final String NVIDIA_SHIELD_DEVICE_NAME_PREFIX = "NVIDIA Corporation NVIDIA Controller";

    @VisibleForTesting
    static final String MICROSOFT_XBOX_PAD_DEVICE_NAME = "Microsoft X-Box 360 pad";

    @VisibleForTesting
    static final String PS_DUALSHOCK_3_SIXAXIS_DEVICE_NAME = "Sony PLAYSTATION(R)3 Controller";

    @VisibleForTesting static final String SAMSUNG_EI_GP20_DEVICE_NAME = "Samsung Game Pad EI-GP20";
    @VisibleForTesting static final String AMAZON_FIRE_DEVICE_NAME = "Amazon Fire Game Controller";

    @VisibleForTesting static final int SONY_VENDOR_ID = 0x054c;
    @VisibleForTesting static final int PS_DUALSHOCK_4_PRODUCT_ID = 0x05c4;
    @VisibleForTesting static final int PS_DUALSHOCK_4_SLIM_PRODUCT_ID = 0x09cc;
    @VisibleForTesting static final int PS_DUALSHOCK_4_USB_RECEIVER_PRODUCT_ID = 0x0ba0;
    static final int PS_DUAL_SENSE_PRODUCT_ID = 0x0ce6;

    @VisibleForTesting static final int MICROSOFT_VENDOR_ID = 0x045e;
    @VisibleForTesting static final int XBOX_ONE_S_2016_FIRMWARE_PRODUCT_ID = 0x02e0;
    @VisibleForTesting static final int XBOX_SERIES_X_BLUETOOTH_PRODUCT_ID = 0x0b13;

    @VisibleForTesting static final int BROADCOM_VENDOR_ID = 0x0a5c;
    @VisibleForTesting static final int SNAKEBYTE_IDROIDCON_PRODUCT_ID = 0x8502;

    @VisibleForTesting static final int GOOGLE_VENDOR_ID = 0x18d1;
    @VisibleForTesting static final int STADIA_CONTROLLER_PRODUCT_ID = 0x9400;

    private static final float BUTTON_AXIS_DEADZONE = 0.01f;

    public static GamepadMappings getMappings(InputDevice device, int[] axes, BitSet buttons) {
        GamepadMappings mappings = getMappings(device.getVendorId(), device.getProductId(), axes);
        if (mappings == null) {
            mappings = getMappings(device.getName());
        }
        if (mappings == null) {
            mappings = new UnknownGamepadMappings(axes, buttons);
        }
        return mappings;
    }

    @VisibleForTesting
    static GamepadMappings getMappings(int vendorId, int productId, int[] axes) {
        if (vendorId == SONY_VENDOR_ID) {
            if (productId == PS_DUALSHOCK_4_PRODUCT_ID
                    || productId == PS_DUALSHOCK_4_SLIM_PRODUCT_ID
                    || productId == PS_DUALSHOCK_4_USB_RECEIVER_PRODUCT_ID) {
                // Android 9 included improvements for PS3 and PS4 gamepads that changed the
                // KeyEvent and MotionEvent codes for some buttons and axes. Use an alternate
                // mapping for versions of Android that include these improvements.
                if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                    return new XboxCompatibleGamepadMappings();
                }
                return new Ps4Ps5GamepadMappings();
            }
            if (productId == PS_DUAL_SENSE_PRODUCT_ID) {
                // Android 12 includes a new driver for PS5 gamepads. Use an alternate mapping for
                // versions of Android without this driver.
                if (android.os.Build.VERSION.SDK_INT <= Build.VERSION_CODES.R) {
                    return new Ps4Ps5GamepadMappings();
                }
            }
        }
        if (vendorId == MICROSOFT_VENDOR_ID) {
            // Microsoft released a firmware update for the Xbox One S gamepad that modified the
            // button and axis assignments. With the new firmware, these gamepads work correctly in
            // Android using the default mapping, but a custom mapping is still required for the old
            // firmware. Both gamepads return the same device name, so we must compare hardware IDs
            // to distinguish them.
            if (productId == XBOX_ONE_S_2016_FIRMWARE_PRODUCT_ID) {
                return new XboxOneS2016FirmwareMappings();
            }
            if (productId == XBOX_SERIES_X_BLUETOOTH_PRODUCT_ID) {
                return new XboxSeriesXBluetoothMappings();
            }
        }
        if (vendorId == BROADCOM_VENDOR_ID && productId == SNAKEBYTE_IDROIDCON_PRODUCT_ID) {
            return new SnakebyteIDroidConMappings(axes);
        }
        if (vendorId == GOOGLE_VENDOR_ID && productId == STADIA_CONTROLLER_PRODUCT_ID) {
            return new StadiaControllerMappings();
        }
        return null;
    }

    @VisibleForTesting
    static GamepadMappings getMappings(String deviceName) {
        if (deviceName.startsWith(NVIDIA_SHIELD_DEVICE_NAME_PREFIX)
                || deviceName.equals(MICROSOFT_XBOX_PAD_DEVICE_NAME)) {
            return new XboxCompatibleGamepadMappings();
        } else if (deviceName.equals(PS_DUALSHOCK_3_SIXAXIS_DEVICE_NAME)) {
            // Android 9 included improvements for Sony PlayStation gamepads that changed the
            // KeyEvent and MotionEvent codes for some buttons and axes. Use an alternate mapping
            // for versions of Android that include these improvements.
            if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                return new Dualshock3SixAxisGamepadMappings();
            }
            return new Dualshock3SixAxisGamepadMappingsPreP();
        } else if (deviceName.equals(SAMSUNG_EI_GP20_DEVICE_NAME)) {
            return new SamsungEIGP20GamepadMappings();
        } else if (deviceName.equals(AMAZON_FIRE_DEVICE_NAME)) {
            return new AmazonFireGamepadMappings();
        }
        return null;
    }

    @VisibleForTesting
    static GamepadMappings getUnknownGamepadMappings(int[] axes, BitSet buttons) {
        return new UnknownGamepadMappings(axes, buttons);
    }

    /**
     * Method that specifies whether the mappings are standard or not.
     * It should be overridden in subclasses that don't provide standard
     * mappings.
     */
    public boolean isStandard() {
        return true;
    }

    /**
     * Returns the number of mapped buttons. Subclasses which support more or fewer buttons (e.g. no
     * meta button) should override this.
     */
    public int getButtonsLength() {
        return CanonicalButtonIndex.COUNT;
    }

    /**
     * Method implemented by subclasses to perform mapping from raw axes and buttons
     * to canonical axes and buttons.
     */
    public abstract void mapToStandardGamepad(
            float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons);

    private static void mapCommonXYABButtons(float[] mappedButtons, float[] rawButtons) {
        float a = rawButtons[KeyEvent.KEYCODE_BUTTON_A];
        float b = rawButtons[KeyEvent.KEYCODE_BUTTON_B];
        float x = rawButtons[KeyEvent.KEYCODE_BUTTON_X];
        float y = rawButtons[KeyEvent.KEYCODE_BUTTON_Y];
        mappedButtons[CanonicalButtonIndex.PRIMARY] = a;
        mappedButtons[CanonicalButtonIndex.SECONDARY] = b;
        mappedButtons[CanonicalButtonIndex.TERTIARY] = x;
        mappedButtons[CanonicalButtonIndex.QUATERNARY] = y;
    }

    private static void mapCommonStartSelectMetaButtons(float[] mappedButtons, float[] rawButtons) {
        float start = rawButtons[KeyEvent.KEYCODE_BUTTON_START];
        float select = rawButtons[KeyEvent.KEYCODE_BUTTON_SELECT];
        float mode = rawButtons[KeyEvent.KEYCODE_BUTTON_MODE];
        mappedButtons[CanonicalButtonIndex.START] = start;
        mappedButtons[CanonicalButtonIndex.BACK_SELECT] = select;
        mappedButtons[CanonicalButtonIndex.META] = mode;
    }

    private static void mapCommonThumbstickButtons(float[] mappedButtons, float[] rawButtons) {
        float thumbL = rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBL];
        float thumbR = rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBR];
        mappedButtons[CanonicalButtonIndex.LEFT_THUMBSTICK] = thumbL;
        mappedButtons[CanonicalButtonIndex.RIGHT_THUMBSTICK] = thumbR;
    }

    /**
     * Method for mapping the L1/R1 buttons to lower shoulder buttons, rather than
     * upper shoulder as the user would normally expect. Please think twice before
     * using this, as it can easily confuse the user. It is only really useful if
     * the controller completely lacks a second set of shoulder buttons.
     */
    private static void mapUpperTriggerButtonsToBottomShoulder(
            float[] mappedButtons, float[] rawButtons) {
        float l1 = rawButtons[KeyEvent.KEYCODE_BUTTON_L1];
        float r1 = rawButtons[KeyEvent.KEYCODE_BUTTON_R1];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = l1;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = r1;
    }

    private static void mapTriggerButtonsToTopShoulder(float[] mappedButtons, float[] rawButtons) {
        float l1 = rawButtons[KeyEvent.KEYCODE_BUTTON_L1];
        float r1 = rawButtons[KeyEvent.KEYCODE_BUTTON_R1];
        mappedButtons[CanonicalButtonIndex.LEFT_SHOULDER] = l1;
        mappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER] = r1;
    }

    private static void mapCommonDpadButtons(float[] mappedButtons, float[] rawButtons) {
        float dpadDown = rawButtons[KeyEvent.KEYCODE_DPAD_DOWN];
        float dpadUp = rawButtons[KeyEvent.KEYCODE_DPAD_UP];
        float dpadLeft = rawButtons[KeyEvent.KEYCODE_DPAD_LEFT];
        float dpadRight = rawButtons[KeyEvent.KEYCODE_DPAD_RIGHT];
        mappedButtons[CanonicalButtonIndex.DPAD_DOWN] = dpadDown;
        mappedButtons[CanonicalButtonIndex.DPAD_UP] = dpadUp;
        mappedButtons[CanonicalButtonIndex.DPAD_LEFT] = dpadLeft;
        mappedButtons[CanonicalButtonIndex.DPAD_RIGHT] = dpadRight;
    }

    private static void mapXYAxes(float[] mappedAxes, float[] rawAxes) {
        mappedAxes[CanonicalAxisIndex.LEFT_STICK_X] = rawAxes[MotionEvent.AXIS_X];
        mappedAxes[CanonicalAxisIndex.LEFT_STICK_Y] = rawAxes[MotionEvent.AXIS_Y];
    }

    private static void mapRXAndRYAxesToRightStick(float[] mappedAxes, float[] rawAxes) {
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_X] = rawAxes[MotionEvent.AXIS_RX];
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y] = rawAxes[MotionEvent.AXIS_RY];
    }

    private static void mapZAndRZAxesToRightStick(float[] mappedAxes, float[] rawAxes) {
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_X] = rawAxes[MotionEvent.AXIS_Z];
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y] = rawAxes[MotionEvent.AXIS_RZ];
    }

    private static void mapPedalAxesToBottomShoulder(float[] mappedButtons, float[] rawAxes) {
        float lTrigger = rawAxes[MotionEvent.AXIS_BRAKE];
        float rTrigger = rawAxes[MotionEvent.AXIS_GAS];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = lTrigger;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = rTrigger;
    }

    private static void mapTriggerAxesToBottomShoulder(float[] mappedButtons, float[] rawAxes) {
        float lTrigger = rawAxes[MotionEvent.AXIS_LTRIGGER];
        float rTrigger = rawAxes[MotionEvent.AXIS_RTRIGGER];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = lTrigger;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = rTrigger;
    }

    private static void mapZAxisToBottomShoulder(float[] mappedButtons, float[] rawAxes) {
        float z = rawAxes[MotionEvent.AXIS_Z];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = z > BUTTON_AXIS_DEADZONE ? z : 0.0f;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = -z > BUTTON_AXIS_DEADZONE ? -z : 0.0f;
    }

    private static void mapLowerTriggerButtonsToBottomShoulder(
            float[] mappedButtons, float[] rawButtons) {
        float l2 = rawButtons[KeyEvent.KEYCODE_BUTTON_L2];
        float r2 = rawButtons[KeyEvent.KEYCODE_BUTTON_R2];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = l2;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = r2;
    }

    @VisibleForTesting
    static float negativeAxisValueAsButton(float input) {
        return (input < -0.5f) ? 1.f : 0.f;
    }

    @VisibleForTesting
    static float positiveAxisValueAsButton(float input) {
        return (input > 0.5f) ? 1.f : 0.f;
    }

    private static void mapHatAxisToDpadButtons(float[] mappedButtons, float[] rawAxes) {
        float hatX = rawAxes[MotionEvent.AXIS_HAT_X];
        float hatY = rawAxes[MotionEvent.AXIS_HAT_Y];
        mappedButtons[CanonicalButtonIndex.DPAD_LEFT] = negativeAxisValueAsButton(hatX);
        mappedButtons[CanonicalButtonIndex.DPAD_RIGHT] = positiveAxisValueAsButton(hatX);
        mappedButtons[CanonicalButtonIndex.DPAD_UP] = negativeAxisValueAsButton(hatY);
        mappedButtons[CanonicalButtonIndex.DPAD_DOWN] = positiveAxisValueAsButton(hatY);
    }

    private static class AmazonFireGamepadMappings extends GamepadMappings {

        /**
         * Method for mapping Amazon Fire gamepad axis and button values
         * to standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapPedalAxesToBottomShoulder(mappedButtons, rawAxes);
            mapHatAxisToDpadButtons(mappedButtons, rawAxes);

            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
        }
    }

    private static class XboxCompatibleGamepadMappings extends GamepadMappings {

        /**
         * Method for mapping Xbox 360-compatible gamepad axis and button values
         * to standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapTriggerAxesToBottomShoulder(mappedButtons, rawAxes);
            mapHatAxisToDpadButtons(mappedButtons, rawAxes);

            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
        }
    }

    private static class SnakebyteIDroidConMappings extends GamepadMappings {
        private final boolean mAnalogMode;

        public SnakebyteIDroidConMappings(int[] axes) {
            // Digital mode has X, Y, Z, RZ, HAT_X, HAT_Y
            // Analog mode has X, Y, Z, RX, RY, HAT_X, HAT_Y
            mAnalogMode = arrayContains(axes, MotionEvent.AXIS_RX);
        }

        private static boolean arrayContains(int[] array, int element) {
            for (int e : array) {
                if (e == element) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public int getButtonsLength() {
            // No meta button.
            return CanonicalButtonIndex.COUNT - 1;
        }

        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapXYAxes(mappedAxes, rawAxes);
            mapHatAxisToDpadButtons(mappedButtons, rawAxes);

            // On older versions of Android the thumbstick buttons are incorrectly mapped to C and
            // Z. Support either.
            float thumbL = rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBL];
            float thumbR = rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBR];
            float c = rawButtons[KeyEvent.KEYCODE_BUTTON_C];
            float z = rawButtons[KeyEvent.KEYCODE_BUTTON_Z];
            mappedButtons[CanonicalButtonIndex.LEFT_THUMBSTICK] = Math.max(thumbL, c);
            mappedButtons[CanonicalButtonIndex.RIGHT_THUMBSTICK] = Math.max(thumbR, z);

            if (mAnalogMode) {
                mapZAxisToBottomShoulder(mappedButtons, rawAxes);
                mapRXAndRYAxesToRightStick(mappedAxes, rawAxes);
            } else {
                mapLowerTriggerButtonsToBottomShoulder(mappedButtons, rawButtons);
                mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
            }
        }
    }

    private static class XboxOneS2016FirmwareMappings extends GamepadMappings {
        private boolean mLeftTriggerActivated;
        private boolean mRightTriggerActivated;

        /**
         * Method for mapping Xbox One S controller (in Bluetooth mode) to
         * standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            mappedButtons[CanonicalButtonIndex.PRIMARY] = rawButtons[KeyEvent.KEYCODE_BUTTON_A];
            mappedButtons[CanonicalButtonIndex.SECONDARY] = rawButtons[KeyEvent.KEYCODE_BUTTON_B];
            mappedButtons[CanonicalButtonIndex.TERTIARY] = rawButtons[KeyEvent.KEYCODE_BUTTON_C];
            mappedButtons[CanonicalButtonIndex.QUATERNARY] = rawButtons[KeyEvent.KEYCODE_BUTTON_X];

            mappedButtons[CanonicalButtonIndex.LEFT_SHOULDER] =
                    rawButtons[KeyEvent.KEYCODE_BUTTON_Y];
            mappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER] =
                    rawButtons[KeyEvent.KEYCODE_BUTTON_Z];

            mappedButtons[CanonicalButtonIndex.BACK_SELECT] =
                    rawButtons[KeyEvent.KEYCODE_BUTTON_L1];
            mappedButtons[CanonicalButtonIndex.START] = rawButtons[KeyEvent.KEYCODE_BUTTON_R1];

            mappedButtons[CanonicalButtonIndex.LEFT_THUMBSTICK] =
                    rawButtons[KeyEvent.KEYCODE_BUTTON_L2];
            mappedButtons[CanonicalButtonIndex.RIGHT_THUMBSTICK] =
                    rawButtons[KeyEvent.KEYCODE_BUTTON_R2];

            // The left and right triggers on the Xbox One S controller
            // are exposed as AXIS_Z and AXIS_RZ respectively. However,
            // these nominally idle at -1 rather than 0, like other triggers.
            // Unfortunately, the -1 value is only reported upon the first
            // activation of each trigger axis. In order to prevent idling at
            // 0.5 before trigger activation, we only expose trigger values
            // when we've seen them report a non-zero value at least once.
            if (rawAxes[MotionEvent.AXIS_Z] != 0) {
                mLeftTriggerActivated = true;
            }
            if (rawAxes[MotionEvent.AXIS_RZ] != 0) {
                mRightTriggerActivated = true;
            }
            if (mLeftTriggerActivated) {
                mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] =
                        (rawAxes[MotionEvent.AXIS_Z] + 1) / 2;
            } else {
                mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = 0.f;
            }
            if (mRightTriggerActivated) {
                mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] =
                        (rawAxes[MotionEvent.AXIS_RZ] + 1) / 2;
            } else {
                mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = 0.f;
            }

            mapHatAxisToDpadButtons(mappedButtons, rawAxes);
            mapXYAxes(mappedAxes, rawAxes);
            mapRXAndRYAxesToRightStick(mappedAxes, rawAxes);
        }

        @Override
        public int getButtonsLength() {
            // No meta button.
            return CanonicalButtonIndex.COUNT - 1;
        }
    }

    private static class XboxSeriesXBluetoothMappings extends GamepadMappings {
        private static final int BUTTON_INDEX_SHARE = CanonicalButtonIndex.COUNT;

        /**
         * Method for mapping Xbox Series X controller (in Bluetooth mode) to
         * standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapHatAxisToDpadButtons(mappedButtons, rawAxes);
            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
            mapPedalAxesToBottomShoulder(mappedButtons, rawAxes);
            mappedButtons[BUTTON_INDEX_SHARE] = rawButtons[KeyEvent.KEYCODE_MEDIA_RECORD];
        }

        @Override
        public int getButtonsLength() {
            // Include the Share button.
            return CanonicalButtonIndex.COUNT + 1;
        }
    }

    private static class Dualshock3SixAxisGamepadMappingsPreP extends GamepadMappings {
        /**
         * Method for mapping DualShock 3 and SIXAXIS gamepad inputs to standard gamepad button and
         * axis values. This mapping function should only be used on Android 8 and earlier.
         */
        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            // On DualShock 3 and SIXAXIS, X/Y has higher priority.
            float a = rawButtons[KeyEvent.KEYCODE_BUTTON_A];
            float b = rawButtons[KeyEvent.KEYCODE_BUTTON_B];
            float x = rawButtons[KeyEvent.KEYCODE_BUTTON_X];
            float y = rawButtons[KeyEvent.KEYCODE_BUTTON_Y];
            mappedButtons[CanonicalButtonIndex.PRIMARY] = x;
            mappedButtons[CanonicalButtonIndex.SECONDARY] = y;
            mappedButtons[CanonicalButtonIndex.TERTIARY] = a;
            mappedButtons[CanonicalButtonIndex.QUATERNARY] = b;

            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonDpadButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapTriggerAxesToBottomShoulder(mappedButtons, rawAxes);

            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
        }
    }

    private static class Dualshock3SixAxisGamepadMappings extends GamepadMappings {
        /**
         * Method for mapping DualShock 3 and SIXAXIS gamepad inputs to standard gamepad button and
         * axis values. This mapping function should only be used on Android 10+.
         */
        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapCommonDpadButtons(mappedButtons, rawButtons);
            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
            mapTriggerAxesToBottomShoulder(mappedButtons, rawAxes);
        }
    }

    static class Ps4Ps5GamepadMappings extends GamepadMappings {
        // Scale input from [-1, 1] to [0, 1] uniformly.
        private static float scaleRxRy(float input) {
            return 1.f - ((1.f - input) / 2.f);
        }

        /**
         * Method for mapping DualShock 4 and DualSense gamepad inputs to standard gamepad button
         * and axis values. This mapping function should only be used for DualShock 4 on Android 9
         * and earlier and DualSense on Android 11 and earlier.
         */
        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            float a = rawButtons[KeyEvent.KEYCODE_BUTTON_A];
            float b = rawButtons[KeyEvent.KEYCODE_BUTTON_B];
            float c = rawButtons[KeyEvent.KEYCODE_BUTTON_C];
            float x = rawButtons[KeyEvent.KEYCODE_BUTTON_X];
            mappedButtons[CanonicalButtonIndex.PRIMARY] = b;
            mappedButtons[CanonicalButtonIndex.SECONDARY] = c;
            mappedButtons[CanonicalButtonIndex.TERTIARY] = a;
            mappedButtons[CanonicalButtonIndex.QUATERNARY] = x;

            float y = rawButtons[KeyEvent.KEYCODE_BUTTON_Y];
            float z = rawButtons[KeyEvent.KEYCODE_BUTTON_Z];
            mappedButtons[CanonicalButtonIndex.LEFT_SHOULDER] = y;
            mappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER] = z;

            float rx = rawAxes[MotionEvent.AXIS_RX];
            float ry = rawAxes[MotionEvent.AXIS_RY];
            mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = scaleRxRy(rx);
            mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = scaleRxRy(ry);

            float share = rawButtons[KeyEvent.KEYCODE_BUTTON_L2];
            float options = rawButtons[KeyEvent.KEYCODE_BUTTON_R2];
            mappedButtons[CanonicalButtonIndex.BACK_SELECT] = share;
            mappedButtons[CanonicalButtonIndex.START] = options;

            float thumbL = rawButtons[KeyEvent.KEYCODE_BUTTON_SELECT];
            float thumbR = rawButtons[KeyEvent.KEYCODE_BUTTON_START];
            mappedButtons[CanonicalButtonIndex.LEFT_THUMBSTICK] = thumbL;
            mappedButtons[CanonicalButtonIndex.RIGHT_THUMBSTICK] = thumbR;

            float mode = rawButtons[KeyEvent.KEYCODE_BUTTON_MODE];
            mappedButtons[CanonicalButtonIndex.META] = mode;

            mapHatAxisToDpadButtons(mappedButtons, rawAxes);
            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
        }
    }

    private static class SamsungEIGP20GamepadMappings extends GamepadMappings {
        /**
         * Method for mapping Samsung GamePad EI-GP20 axis and button values
         * to standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapUpperTriggerButtonsToBottomShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapHatAxisToDpadButtons(mappedButtons, rawAxes);

            mapXYAxes(mappedAxes, rawAxes);
            mapRXAndRYAxesToRightStick(mappedAxes, rawAxes);
        }
    }

    private static class StadiaControllerMappings extends GamepadMappings {
        private static final int BUTTON_INDEX_ASSISTANT = CanonicalButtonIndex.COUNT;
        private static final int BUTTON_INDEX_CAPTURE = CanonicalButtonIndex.COUNT + 1;

        /**
         * Method for mapping Stadia Controller axis and button values to
         * standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapPedalAxesToBottomShoulder(mappedButtons, rawAxes);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapHatAxisToDpadButtons(mappedButtons, rawAxes);
            mappedButtons[BUTTON_INDEX_ASSISTANT] = rawButtons[KeyEvent.KEYCODE_BUTTON_1];
            mappedButtons[BUTTON_INDEX_CAPTURE] = rawButtons[KeyEvent.KEYCODE_BUTTON_2];

            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
        }

        @Override
        public int getButtonsLength() {
            // Include the Assistant and Capture buttons.
            return CanonicalButtonIndex.COUNT + 2;
        }
    }

    private static class UnknownGamepadMappings extends GamepadMappings {
        private int mLeftTriggerAxis = -1;
        private int mRightTriggerAxis = -1;
        private int mRightStickXAxis = -1;
        private int mRightStickYAxis = -1;
        private boolean mUseHatAxes;
        private final boolean mHasMetaButton;

        UnknownGamepadMappings(int[] axes, BitSet buttons) {
            mHasMetaButton = buttons.get(KeyEvent.KEYCODE_BUTTON_MODE);

            int hatAxesFound = 0;

            for (int axis : axes) {
                switch (axis) {
                    case MotionEvent.AXIS_LTRIGGER:
                    case MotionEvent.AXIS_BRAKE:
                        mLeftTriggerAxis = axis;
                        break;
                    case MotionEvent.AXIS_RTRIGGER:
                    case MotionEvent.AXIS_GAS:
                    case MotionEvent.AXIS_THROTTLE:
                        mRightTriggerAxis = axis;
                        break;
                    case MotionEvent.AXIS_RX:
                    case MotionEvent.AXIS_Z:
                        mRightStickXAxis = axis;
                        break;
                    case MotionEvent.AXIS_RY:
                    case MotionEvent.AXIS_RZ:
                        mRightStickYAxis = axis;
                        break;
                    case MotionEvent.AXIS_HAT_X:
                        hatAxesFound++;
                        break;
                    case MotionEvent.AXIS_HAT_Y:
                        hatAxesFound++;
                        break;
                    default:
                        break;
                }
            }

            if (hatAxesFound == 2) {
                mUseHatAxes = true;
            }
        }

        @Override
        public boolean isStandard() {
            // These mappings should not be considered standard
            return false;
        }

        @Override
        public int getButtonsLength() {
            return mHasMetaButton ? CanonicalButtonIndex.COUNT : CanonicalButtonIndex.COUNT - 1;
        }

        @Override
        public void mapToStandardGamepad(
                float[] mappedAxes, float[] mappedButtons, float[] rawAxes, float[] rawButtons) {
            // These are shared among all gamepads intended for use with Android
            // that we tested so far.
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapXYAxes(mappedAxes, rawAxes);

            if (mLeftTriggerAxis != -1 && mRightTriggerAxis != -1) {
                float lTrigger = rawAxes[mLeftTriggerAxis];
                float rTrigger = rawAxes[mRightTriggerAxis];
                mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = lTrigger;
                mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = rTrigger;
            } else {
                // Devices without analog triggers use digital buttons
                mapLowerTriggerButtonsToBottomShoulder(mappedButtons, rawButtons);
            }

            if (mRightStickXAxis != -1 && mRightStickYAxis != -1) {
                float rX = rawAxes[mRightStickXAxis];
                float rY = rawAxes[mRightStickYAxis];
                mappedAxes[CanonicalAxisIndex.RIGHT_STICK_X] = rX;
                mappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y] = rY;
            }

            if (mUseHatAxes) {
                mapHatAxisToDpadButtons(mappedButtons, rawAxes);
            } else {
                mapCommonDpadButtons(mappedButtons, rawButtons);
            }
        }
    }
}
