// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
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
    public static void setZoomLevel(WebContents webContents, double newZoomLevel) {
        HostZoomMapImplJni.get().setZoomLevel(webContents, newZoomLevel);
    }

    /**
     * Get the current zoom level for a given web contents.
     * @param webContents   WebContents to get zoom level for
     * @return double       current zoom level
     */
    public static double getZoomLevel(WebContents webContents) {
        return HostZoomMapImplJni.get().getZoomLevel(webContents);
    }

    @NativeMethods
    interface Natives {
        void setZoomLevel(WebContents webContents, double newZoomLevel);
        double getZoomLevel(WebContents webContents);
    }
}