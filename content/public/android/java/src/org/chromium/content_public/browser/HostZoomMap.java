// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.NonNull;

import org.chromium.base.MathUtils;
import org.chromium.content.browser.HostZoomMapImpl;

/**
 * Implementations of various static methods related to page zoom.
 */
public class HostZoomMap {
    // Preset zoom factors that the +/- buttons can "snap" the zoom level to if user chooses to
    // not use the slider. These zoom factors correspond to the zoom levels that are used on
    // desktop, i.e. {0.50, 0.67, ... 3.00}, excluding the smallest/largest two, since they are
    // of little value on a mobile device.
    public static final double[] AVAILABLE_ZOOM_FACTORS = new double[] {-3.80, -2.20, -1.58, -1.22,
            -0.58, 0.00, 0.52, 1.22, 1.56, 2.22, 3.07, 3.80, 5.03, 6.03};

    // The value of the base for zoom factor, should match |kTextSizeMultiplierRatio|.
    public static final float TEXT_SIZE_MULTIPLIER_RATIO = 1.2f;

    // The current |fontScale| value from Android Configuration. Represents user font size choice.
    // The system font scale acts like the "zoom level" of this component. For the default
    // setting, |fontScale|=1.0; for {Small, Large, XL} the values are {0.85, 1.15, 1.30}.
    // This will be transparently taken into account. If a user has the system font set to
    // XL, then Page Zoom will behave as if it is at 130% while displaying 100% to the user.
    public static float SYSTEM_FONT_SCALE = 1.0f;

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
        // level setting and the desktop site zoom scale. We only do this here and not when applying
        // the default values chosen in the settings flow so that if the user changes their OS-level
        // setting or Request Desktop Site setting, the Chrome setting will continue to display the
        // same value, but adjust accordingly. For example, if the user chooses 150% zoom in
        // settings with font set to XL and navigates to a desktop site that requires 110% zoom,
        // then really that choice is 195% * 1.1 = 214.5%. If the user then switches to default
        // |fontScale| or switches to a mobile site, we would still want the value to be 150% shown
        // to the user, and not the 214.5%.
        HostZoomMapImpl.setZoomLevel(webContents, newZoomLevel,
                adjustZoomLevel(newZoomLevel, SYSTEM_FONT_SCALE,
                        HostZoomMapImpl.getDesktopSiteZoomScale(webContents)));
    }

    /**
     * Get the current zoom level for a given web contents.
     * @param webContents   WebContents to get zoom level for
     * @return double       current zoom level
     */
    public static double getZoomLevel(@NonNull WebContents webContents) {
        assert !webContents.isDestroyed();

        // Just before returning a zoom level from the backend, we must again take into account the
        // system level setting and the desktop site zoom scale. Here we need to do the reverse
        // operation of the above, effectively divide rather than multiply, so we will pass the
        // reciprocal of |SYSTEM_FONT_SCALE| and |DESKTOP_SITE_ZOOM_SCALE| respectively.
        return adjustZoomLevel(HostZoomMapImpl.getZoomLevel(webContents),
                (float) 1 / SYSTEM_FONT_SCALE,
                (float) 1 / HostZoomMapImpl.getDesktopSiteZoomScale(webContents));
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
     * Get the default zoom level for a given browser context handle (e.g. Profile).
     * @param context       BrowserContextHandle to get default for.
     * @return double       default zoom level.
     */
    public static double getDefaultZoomLevel(BrowserContextHandle context) {
        return HostZoomMapImpl.getDefaultZoomLevel(context);
    }

    /**
     * Adjust a given zoom level to account for the OS-level |fontScale| configuration and/or the
     * scaling factor applicable when a site uses the desktop user agent on Android.
     *
     * @param zoomLevel    The zoom level to adjust.
     * @param systemFontScale  User selected font scale value.
     * @param desktopSiteZoomScale The zoom scaling factor applicable for a desktop site.
     * @return double      The adjusted zoom level.
     */
    public static double adjustZoomLevel(
            double zoomLevel, float systemFontScale, float desktopSiteZoomScale) {
        // No calculation to do if the user has set OS-level |fontScale| to 1 (default), and if the
        // desktop site zoom scale is default (1, or 100%).
        if (MathUtils.areFloatsEqual(systemFontScale, 1f)
                && MathUtils.areFloatsEqual(desktopSiteZoomScale, 1f)) {
            return zoomLevel;
        }

        // Convert the zoom factor to a level, e.g. factor = 0.0 should translate to 1.0 (100%).
        // Multiply the level by the OS-level |fontScale| and the desktop site zoom scale. For
        // example, if the user has chosen a Chrome-level zoom of 150%, and a OS-level setting of XL
        // (130%) and the desktop site zoom scale is 110%, then we want to continue to display 150%
        // to the user but actually render 1.5 * 1.3 * 1.1 = 2.145 (~214%) zoom. We must apply this
        // at the zoom level (not factor) to compensate for logarithmic scale.
        double adjustedLevel = systemFontScale * Math.pow(TEXT_SIZE_MULTIPLIER_RATIO, zoomLevel)
                * desktopSiteZoomScale;

        // We do not pass levels to the backend, but factors. So convert back and round.
        double adjustedFactor = Math.log10(adjustedLevel) / Math.log10(TEXT_SIZE_MULTIPLIER_RATIO);

        // At the extremes, this can go over the min/max, so clamp to that range.
        adjustedFactor = MathUtils.clamp((float) adjustedFactor, (float) AVAILABLE_ZOOM_FACTORS[0],
                (float) AVAILABLE_ZOOM_FACTORS[AVAILABLE_ZOOM_FACTORS.length - 1]);

        return MathUtils.roundTwoDecimalPlaces(adjustedFactor);
    }
}