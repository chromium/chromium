// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.NonNull;

import org.chromium.content.browser.HostZoomMapImpl;

import java.util.Map;

/** Implementations of various static methods related to page zoom. */
public class HostZoomMap {
    // Preset zoom factors that the +/- buttons can "snap" the zoom level to if user chooses to
    // not use the slider. These zoom factors correspond to the zoom levels that are used on
    // desktop, i.e. {0.50, 0.67, ... 3.00}, excluding the smallest/largest two, since they are
    // of little value on a mobile device.
    public static final double[] AVAILABLE_ZOOM_FACTORS =
            new double[] {
                -3.80, -2.20, -1.58, -1.22, -0.58, 0.00, 0.52, 1.22, 1.56, 2.22, 3.07, 3.80, 5.03,
                6.03
            };

    // The value of the base for zoom factor, should match |kTextSizeMultiplierRatio|.
    public static final float TEXT_SIZE_MULTIPLIER_RATIO = 1.2f;

    // The current |fontScale| value from Android Configuration. Represents user font size choice.
    // The system font scale acts like the "zoom level" of this component. For the default
    // setting, |fontScale|=1.0; for {Small, Large, XL} the values are {0.85, 1.15, 1.30}.
    // This will be transparently taken into account. If a user has the system font set to
    // XL, then Page Zoom will behave as if it is at 130% while displaying 100% to the user.
    private static float sSystemFontScale = 1.0f;

    // Private constructor to prevent unwanted construction.
    private HostZoomMap() {}

    /**
     * Set a new zoom level for the given web contents.
     * @param webContents   WebContents to update
     * @param newZoomLevel  double - new zoom level
     */
    public static void setZoomLevel(@NonNull WebContents webContents, double newZoomLevel) {
        assert !webContents.isDestroyed();

        // Just before sending the zoom level to the backend, we will take into account the system
        // level setting. We only do this here and not when applying the default values chosen in
        // the settings flow so that if the user changes their OS-level setting, the Chrome setting
        // will continue to display the same value, but adjust accordingly. For example, if the user
        // chooses 150% zoom in settings with font set to XL, then really that choice is 195%. If
        // the user then switches to default |fontScale|, we would still want the value to be 150%
        // shown to the user, and not the 195%.
        HostZoomMapImpl.setZoomLevel(
                webContents,
                newZoomLevel,
                HostZoomMapImpl.adjustZoomLevel(newZoomLevel, sSystemFontScale));
    }

    /** Get the current system font scale */
    public static float getSystemFontScale() {
        return sSystemFontScale;
    }

    /**
     * Set the current system font scale
     * @param newSystemFontScale   float, new value.
     */
    public static void setSystemFontScale(float newSystemFontScale) {
        sSystemFontScale = newSystemFontScale;
    }

    /**
     * Get the current zoom level for a given web contents.
     * @param webContents   WebContents to get zoom level for
     * @return double       current zoom level
     */
    public static double getZoomLevel(@NonNull WebContents webContents) {
        assert !webContents.isDestroyed();

        // Just before returning a zoom level from the backend, we must again take into account the
        // system level setting. Here we need to do the reverse operation of the above, effectively
        // divide rather than multiply, so we will pass the reciprocal of |sSystemFontScale|.
        return HostZoomMapImpl.adjustZoomLevel(
                HostZoomMapImpl.getZoomLevel(webContents), (float) 1 / sSystemFontScale);
    }

    /**
     * Get the zoom levels for all hosts.
     * @param browserContextHandle BrowserContextHandle to get zoom level for.
     * @return  HashMap<String, Double> map containing the host name as a string to the zoom factor
     *         (called zoom level on the c++ side) as a double.
     */
    public static Map<String, Double> getAllHostZoomLevels(
            BrowserContextHandle browserContextHandle) {
        return HostZoomMapImpl.getAllHostZoomLevels(browserContextHandle);
    }

    /**
     * Set the zoom level to the given level for the given host.
     * @param host  host to set zoom level for.
     * @param level new zoom level (as a zoom factor as described in PageZoomUtils.java).
     * @param browserContextHandle  BrowserContextHandle to set zoom level for.
     */
    public static void setZoomLevelForHost(
            BrowserContextHandle browserContextHandle, String host, double level) {
        HostZoomMapImpl.setZoomLevelForHost(browserContextHandle, host, level);
    }

    /**
     * Set the default zoom level for a given browser context handle (e.g. Profile).
     * @param context       BrowserContextHandle to update default for.
     * @param newDefaultZoomLevel   double, new default value.
     */
    public static void setDefaultZoomLevel(
            BrowserContextHandle context, double newDefaultZoomLevel) {
        HostZoomMapImpl.setDefaultZoomLevel(context, newDefaultZoomLevel);
    }

    /**
     * Returns true when the field trial param to adjust zoom for OS-level font setting is
     * true, false otherwise.
     * @return bool True if zoom should be adjusted.
     */
    public static boolean shouldAdjustForOSLevel() {
        return HostZoomMapImpl.shouldAdjustForOSLevel();
    }

    /**
     * Get the default zoom level for a given browser context handle (e.g. Profile).
     * @param context       BrowserContextHandle to get default for.
     * @return double       default zoom level.
     */
    public static double getDefaultZoomLevel(BrowserContextHandle context) {
        return HostZoomMapImpl.getDefaultZoomLevel(context);
    }
}
