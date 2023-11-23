// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.util.ArraySet;
import android.view.InputDevice;

import org.chromium.content.browser.DeviceUtilsImpl;

/** A utility class that has helper methods for device configuration. */
public final class DeviceUtils {
    private DeviceUtils() {}

    /** Appends the switch specifying which user agent should be used for this device. */
    public static void addDeviceSpecificUserAgentSwitch() {
        DeviceUtilsImpl.addDeviceSpecificUserAgentSwitch();
    }

    /** Appends or removes the switch specifying which user agent should be used for this device. */
    public static void updateDeviceSpecificUserAgentSwitch(boolean isTablet) {
        DeviceUtilsImpl.updateDeviceSpecificUserAgentSwitch(isTablet);
    }

    /**
     * @return A set of {@link InputDevice} source types of connected accessories including
     *     keyboard, mouse, touchpad, trackball and stylus devices.
     */
    public static ArraySet<Integer> getConnectedDevices() {
        int[] deviceIds = InputDevice.getDeviceIds();
        ArraySet<Integer> deviceSources = new ArraySet<>();
        for (int deviceId : deviceIds) {
            if (isDeviceOfSourceType(deviceId, InputDevice.SOURCE_KEYBOARD)) {
                deviceSources.add(InputDevice.SOURCE_KEYBOARD);
            } else if (isDeviceOfSourceType(deviceId, InputDevice.SOURCE_MOUSE)) {
                deviceSources.add(InputDevice.SOURCE_MOUSE);
            } else if (isDeviceOfSourceType(deviceId, InputDevice.SOURCE_TOUCHPAD)) {
                deviceSources.add(InputDevice.SOURCE_TOUCHPAD);
            } else if (isDeviceOfSourceType(deviceId, InputDevice.SOURCE_TRACKBALL)) {
                deviceSources.add(InputDevice.SOURCE_TRACKBALL);
            } else if (isDeviceOfSourceType(deviceId, InputDevice.SOURCE_STYLUS)
                    || isDeviceOfSourceType(deviceId, InputDevice.SOURCE_BLUETOOTH_STYLUS)) {
                deviceSources.add(InputDevice.SOURCE_STYLUS);
            }
        }
        return deviceSources;
    }

    private static boolean isDeviceOfSourceType(int deviceId, int sourceType) {
        var device = InputDevice.getDevice(deviceId);
        if (device == null) return false;
        return (device.getSources() & sourceType) == sourceType;
    }
}
