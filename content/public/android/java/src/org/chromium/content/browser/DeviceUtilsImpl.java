// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.base.StrictModeContext;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayUtil;

/** A utility class that has helper methods for device configuration. */
@NullMarked
public class DeviceUtilsImpl {
    private DeviceUtilsImpl() {}

    /**
     * Adds/removes the user agent command line switch according to the current display size.
     *
     * <p>See {@link
     * org.chromium.content_public.browser.DeviceUtils#updateDeviceSpecificUserAgentSwitch(Context)}
     * for details.
     */
    public static void updateDeviceSpecificUserAgentSwitch(Context context) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (isDisplayTabletSized(context)) {
                CommandLine.getInstance().removeSwitch(ContentSwitches.USE_MOBILE_UA);
            } else {
                CommandLine.getInstance().appendSwitch(ContentSwitches.USE_MOBILE_UA);
            }
        }
    }

    private static boolean isDisplayTabletSized(Context context) {
        // We do not use DisplayUtil.getCurrentSmallestScreenWidth() because it crashes on obtaining
        // the WindowManager with an application context when the strict mode is enabled.
        int smallestWidth = DisplayUtil.getCurrentSmallestScreenWidthAllowingFallback(context);
        return smallestWidth >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
    }
}
