// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.RenderCoordinatesImpl;

/** Provides dimension/coordinate information of the view rendered by content layer. */
public interface RenderCoordinates {
    /**
     * @return {@link Coord} instance associated with the given {@link WebContents}.
     */
    static RenderCoordinates fromWebContents(WebContents webContents) {
        return RenderCoordinatesImpl.fromWebContents(webContents);
    }

    /**
     * @return Horizontal scroll offset in physical pixels (approx, integer).
     */
    int getScrollXPixInt();

    /**
     * @return Vertical scroll offset in physical pixels (approx, integer).
     */
    int getScrollYPixInt();

    /**
     * @return Approximate width of the content in physical pixels (integer).
     */
    int getContentWidthPixInt();

    /**
     * @return Approximate height of the content in physical pixels (integer).
     */
    int getContentHeightPixInt();

    /**
     * @return Render-reported width of the viewport in physical pixels (approx, integer).
     */
    int getLastFrameViewportWidthPixInt();

    /**
     * @return Render-reported height of the viewport in physical pixels (approx, integer).
     */
    int getLastFrameViewportHeightPixInt();

    /**
     * @return Maximum possible vertical scroll in physical pixels (approx, integer).
     */
    int getMaxVerticalScrollPixInt();

    /**
     * @return Maximum possible horizontal scroll in physical pixels (approx, integer).
     */
    int getMaxHorizontalScrollPixInt();

    /**
     * @return The Physical on-screen Y offset amount below the browser controls.
     */
    int getContentOffsetYPixInt();

    /**
     * @return Current page scale factor (approx, integer).
     */
    int getPageScaleFactorInt();

    /**
     * @return Current page scale factor.
     */
    float getPageScaleFactor();

    /**
     * @return Minimum page scale factor.
     */
    float getMinPageScaleFactor();
}
