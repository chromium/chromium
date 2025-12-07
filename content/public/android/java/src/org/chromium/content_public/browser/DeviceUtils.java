// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Context;
import android.util.ArraySet;
import android.view.InputDevice;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content.browser.DeviceUtilsImpl;

/** A utility class that has helper methods for device configuration. */
@NullMarked
public final class DeviceUtils {
    private DeviceUtils() {}

    /**
     * Adds/removes the user agent command line switch according to the current display size.
     *
     * <p>You should pass a Context associated with the current window (e.g. Activity) to check the
     * correct display. If you pass a Context not associated with a window (e.g. Application), this
     * method will fall back to the default display. Ideally it should be an error to pass a context
     * not associated with a window, but at this moment, this method is often called from browser
     * initialization classes where UI is not ready yet, so we allow the fallback.
     *
     * @param context The context used to look up the current window.
     */
    public static void updateDeviceSpecificUserAgentSwitch(Context context) {
        DeviceUtilsImpl.updateDeviceSpecificUserAgentSwitch(context);
    }

    /**
     * @return A set of {@link InputDevice} source types of connected accessories including
     *     keyboard, mouse, touchpad, trackball. Note: stylus is not included since this can only be
     *     detected during a MotionEvent. (See
     *     https://developer.android.com/reference/android/view/InputDevice#SOURCE_STYLUS)
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
