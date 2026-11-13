// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content.browser.RenderCoordinatesImpl;

/** Provides dimension/coordinate information of the view rendered by content layer. */
@NullMarked
public interface RenderCoordinates {
    /** Returns the {@link RenderCoordinates} associated with the given {@link WebContents}. */
    static RenderCoordinates fromWebContents(WebContents webContents) {
        return RenderCoordinatesImpl.fromWebContents(webContents);
    }

    /** Returns the horizontal scroll offset in physical pixels (approx, integer). */
    int getScrollXPixInt();

    /** Returns the vertical scroll offset in physical pixels (approx, integer). */
    int getScrollYPixInt();

    /** Returns the approximate width of the content in physical pixels (integer). */
    int getContentWidthPixInt();

    /** Returns the approximate height of the content in physical pixels (integer). */
    int getContentHeightPixInt();

    /** Returns the render-reported width of the viewport in physical pixels (approx, integer). */
    int getLastFrameViewportWidthPixInt();

    /** Returns the render-reported height of the viewport in physical pixels (approx, integer). */
    int getLastFrameViewportHeightPixInt();

    /** Returns the maximum possible vertical scroll in physical pixels (approx, integer). */
    int getMaxVerticalScrollPixInt();

    /** Returns the maximum possible horizontal scroll in physical pixels (approx, integer). */
    int getMaxHorizontalScrollPixInt();

    /**
     * Returns the physical on-screen Y offset amount below the top browser controls (approx,
     * integer).
     */
    int getContentOffsetTopPixInt();

    /**
     * Returns the physical on-screen height of the bottom browser controls currently shown (approx,
     * integer).
     */
    int getContentOffsetBottomPixInt();

    /** Returns the current page scale factor (approx, integer). */
    int getPageScaleFactorInt();

    /** Returns the current page scale factor. */
    float getPageScaleFactor();

    /** Returns the minimum page scale factor. */
    float getMinPageScaleFactor();

    /** Returns the given local CSS value {@code css} converted to physical pixels. */
    float fromLocalCssToPix(float css);
}
