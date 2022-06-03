// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.NonNull;

import org.chromium.content.browser.HostZoomMapImpl;

/**
 * Implementations of various static methods related to page zoom.
 */
public class HostZoomMap {
    // Private constructor to prevent unwanted construction.
    private HostZoomMap() {}

    /**
     * Set a new zoom level for the given web contents.
     * @param webContents   WebContents to update
     * @param newZoomLevel  double - new zoom level
     */
    public static void setZoomLevel(@NonNull WebContents webContents, double newZoomLevel) {
        assert !webContents.isDestroyed();
        HostZoomMapImpl.setZoomLevel(webContents, newZoomLevel);
    }

    /**
     * Get the current zoom level for a given web contents.
     * @param webContents   WebContents to get zoom level for
     * @return double       current zoom level
     */
    public static double getZoomLevel(@NonNull WebContents webContents) {
        assert !webContents.isDestroyed();
        return HostZoomMapImpl.getZoomLevel(webContents);
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
}