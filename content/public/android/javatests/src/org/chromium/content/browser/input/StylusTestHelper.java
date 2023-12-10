// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;

import android.graphics.RectF;

import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;

/** Helper class containing helper methods and constants to be used in stylus gesture tests. */
class StylusTestHelper {
    static final String FALLBACK_TEXT = "this gesture failed";

    static org.chromium.gfx.mojom.Rect createMojoRect(int x, int y, int width, int height) {
        org.chromium.gfx.mojom.Rect rect = new org.chromium.gfx.mojom.Rect();
        rect.x = x;
        rect.y = y;
        rect.width = width;
        rect.height = height;
        return rect;
    }

    static void assertMojoRectsAreEqual(
            org.chromium.gfx.mojom.Rect expected, org.chromium.gfx.mojom.Rect actual) {
        assertEquals(expected.x, actual.x);
        assertEquals(expected.y, actual.y);
        assertEquals(expected.width, actual.width);
        assertEquals(expected.height, actual.height);
    }

    static String toJavaString(org.chromium.mojo_base.mojom.String16 buffer) {
        StringBuilder string = new StringBuilder();
        for (short c : buffer.data) {
            string.append((char) c);
        }
        return string.toString();
    }

    /**
     * @param left the X value for the left edge of the rectangle.
     * @param top the Y value for the top edge of the rectangle.
     * @param right the X value for the right edge of the rectangle.
     * @param bottom the Y value for the bottom edge of the rectangle.
     * @param webContents the web contents where this rectangle is being used (for converting to
     *         screen coordinates).
     * @return an Android RectF object represented by the four float values in screen coordinates.
     */
    static RectF toScreenRectF(
            float left, float top, float right, float bottom, WebContentsImpl webContents) {
        // Convert from local CSS coordinates to absolute screen coordinates.
        RenderCoordinatesImpl rc = webContents.getRenderCoordinates();
        int[] screenLocation = new int[2];
        webContents.getViewAndroidDelegate().getContainerView().getLocationOnScreen(screenLocation);
        left = rc.fromLocalCssToPix(left) + screenLocation[0];
        top = rc.fromLocalCssToPix(top) + rc.getContentOffsetYPix() + screenLocation[1];
        right = rc.fromLocalCssToPix(right) + screenLocation[0];
        bottom = rc.fromLocalCssToPix(bottom) + rc.getContentOffsetYPixInt() + screenLocation[1];

        return new RectF(left, top, right, bottom);
    }

    private StylusTestHelper() {}
}
