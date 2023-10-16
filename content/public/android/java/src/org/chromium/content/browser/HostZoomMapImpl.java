// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.content_public.browser.HostZoomMap.TEXT_SIZE_MULTIPLIER_RATIO;
import static org.chromium.content_public.browser.HostZoomMap.getSystemFontScale;
import static org.chromium.content_public.browser.HostZoomMap.setSystemFontScale;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.MathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.SiteZoomInfo;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;

/**
 * Implementations of {@link HostZoomMap}
 */
@JNINamespace("content")
public class HostZoomMapImpl {
    // Private constructor to prevent unwanted construction.
    private HostZoomMapImpl() {}

    /**
     * Set a new zoom level for the given web contents.
     * @param webContents   WebContents to update
     * @param newZoomLevel  double - new zoom level
     */
    public static void setZoomLevel(
            WebContents webContents, double newZoomLevel, double adjustedZoomLevel) {
        HostZoomMapImplJni.get().setZoomLevel(webContents, newZoomLevel, adjustedZoomLevel);
    }

    /**
     * Set a new zoom level for the given host.
     * @param host   host url to update.
     * @param level  double - new zoom level.
     * @param browserContextHandle BrowserContextHandle to update host zoom for.
     */
    public static void setZoomLevelForHost(
            BrowserContextHandle browserContextHandle, String host, double level) {
        HostZoomMapImplJni.get().setZoomLevelForHost(browserContextHandle, host, level);
    }

    /**
     * Gets zoom levels for all hosts.
     * @param browserContextHandle BrowserContextHandle to get host zooms for.
     */
    public static HashMap<String, Double> getAllHostZoomLevels(
            BrowserContextHandle browserContextHandle) {
        SiteZoomInfo[] siteZoomInfoList =
                HostZoomMapImplJni.get().getAllHostZoomLevels(browserContextHandle);
        HashMap<String, Double> hostToZoomLevel = new HashMap<>();
        if (siteZoomInfoList != null) {
            for (int i = 0; i < siteZoomInfoList.length; i++) {
                hostToZoomLevel.put(siteZoomInfoList[i].host, siteZoomInfoList[i].zoomLevel);
            }
        }
        return hostToZoomLevel;
    }

    /**
     * Get the current zoom level for a given web contents.
     * @param webContents   WebContents to get zoom level for
     * @return double       current zoom level
     */
    public static double getZoomLevel(WebContents webContents) {
        return HostZoomMapImplJni.get().getZoomLevel(webContents);
    }

    /**
     * Set the default zoom level for a given browser context handle (e.g. Profile).
     * @param context       BrowserContextHandle to update default for.
     * @param newDefaultZoomLevel   double, new default value.
     */
    public static void setDefaultZoomLevel(
            BrowserContextHandle context, double newDefaultZoomLevel) {
        HostZoomMapImplJni.get().setDefaultZoomLevel(context, newDefaultZoomLevel);
    }

    /**
     * Get the default zoom level for a given browser context handle (e.g. Profile).
     * @param context       BrowserContextHandle to get default for.
     * @return double       default zoom level.
     */
    public static double getDefaultZoomLevel(BrowserContextHandle context) {
        return HostZoomMapImplJni.get().getDefaultZoomLevel(context);
    }

    /**
     * Get the page zoom scaling factor to use when a desktop or mobile user agent is used.
     * @param webContents The {@link WebContents} to get the desktop site zoom scale for.
     * @return The desktop site zoom scale.
     */
    public static float getDesktopSiteZoomScale(WebContents webContents) {
        return (float) HostZoomMapImplJni.get().getDesktopSiteZoomScale(webContents);
    }

    @CalledByNative
    public static double getAdjustedZoomLevel(double zoomLevel, double desktopSiteZoomScale) {
        float systemFontScale = getSystemFontScale();
        // The OS |fontScale| will not be factored in zoom estimation if Page Zoom is disabled; a
        // systemFontScale = 1 will be used in this case.
        if (!ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM)
                || !shouldAdjustForOSLevel()) {
            systemFontScale = 1;
        }
        return adjustZoomLevel(zoomLevel, systemFontScale, (float) desktopSiteZoomScale);
    }

    @CalledByNative
    public static SiteZoomInfo buildSiteZoomInfo(String host, double zoomLevel) {
        return new SiteZoomInfo(host, zoomLevel);
    }

    /**
     * Returns true when the field trial param to adjust zoom for OS-level font setting is
     * true, false otherwise.
     * @return bool True if zoom should be adjusted.
     */
    public static boolean shouldAdjustForOSLevel() {
        return ContentFeatureMap.getInstance().getFieldTrialParamByFeatureAsBoolean(
                ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM,
                ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_PARAM, true);
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
                        && MathUtils.areFloatsEqual(desktopSiteZoomScale, 1f)
                || !shouldAdjustForOSLevel()) {
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
        return MathUtils.roundTwoDecimalPlaces(adjustedFactor);
    }

    @CalledByNativeForTesting
    public static void setSystemFontScaleForTesting(float scale) {
        var oldValue = getSystemFontScale();
        setSystemFontScale(scale);
        ResettersForTesting.register(() -> setSystemFontScale(oldValue));
    }

    @NativeMethods
    public interface Natives {
        void setZoomLevel(WebContents webContents, double newZoomLevel, double adjustedZoomLevel);
        double getZoomLevel(WebContents webContents);
        void setDefaultZoomLevel(BrowserContextHandle context, double newDefaultZoomLevel);
        double getDefaultZoomLevel(BrowserContextHandle context);
        double getDesktopSiteZoomScale(WebContents webContents);
        SiteZoomInfo[] getAllHostZoomLevels(BrowserContextHandle context);
        void setZoomLevelForHost(BrowserContextHandle context, String host, double level);
    }
}
