// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.content_public.browser.HostZoomMap.SYSTEM_FONT_SCALE;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.WebContents;

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
        float systemFontScale = SYSTEM_FONT_SCALE;
        // The OS |fontScale| will not be factored in zoom estimation if Page Zoom is disabled; a
        // systemFontScale = 1 will be used in this case.
        if (!ContentFeatureList.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM)) {
            systemFontScale = 1;
        }
        return HostZoomMap.adjustZoomLevel(
                zoomLevel, systemFontScale, (float) desktopSiteZoomScale);
    }

    @NativeMethods
    public interface Natives {
        void setZoomLevel(WebContents webContents, double newZoomLevel, double adjustedZoomLevel);
        double getZoomLevel(WebContents webContents);
        void setDefaultZoomLevel(BrowserContextHandle context, double newDefaultZoomLevel);
        double getDefaultZoomLevel(BrowserContextHandle context);
        double getDesktopSiteZoomScale(WebContents webContents);
    }
}