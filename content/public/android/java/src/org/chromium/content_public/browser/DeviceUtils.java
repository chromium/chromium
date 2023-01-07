// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.DeviceUtilsImpl;

/**
 * A utility class that has helper methods for device configuration.
 */
public final class DeviceUtils {
    private DeviceUtils() {}

    /**
     * Appends the switch specifying which user agent should be used for this device.
     */
    public static void addDeviceSpecificUserAgentSwitch() {
        DeviceUtilsImpl.addDeviceSpecificUserAgentSwitch();
    }
}
