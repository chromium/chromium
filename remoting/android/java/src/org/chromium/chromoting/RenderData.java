// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Matrix;
import android.graphics.PointF;

/**
 * This class stores UI configuration that will be used when rendering the remote desktop.
 */
public class RenderData {
    /** Stores pan and zoom configuration and converts image coordinates to screen coordinates. */
    public Matrix transform = new Matrix();

    public int screenWidth;
    public int screenHeight;
    public int imageWidth;
    public int imageHeight;

    /** Determines whether the local cursor should be drawn. */
    public boolean drawCursor;

    /**
     * Specifies the position, in image coordinates, at which the cursor image will be drawn.
     * This will normally be at the location of the most recently injected motion event.
     */
    private PointF mCursorPosition = new PointF();

    /**
     * Returns the position of the rendered cursor.
     *
     * @return A point representing the current position.
     */
    public PointF getCursorPosition() {
        return new PointF(mCursorPosition.x, mCursorPosition.y);
    }

    /**
     * Sets the position of the cursor which is used for rendering.
     *
     * @param newX The new value of the x coordinate.
     * @param newY The new value of the y coordinate
     * @return True if the cursor position has changed.
     */
    public boolean setCursorPosition(float newX, float newY) {
        boolean cursorMoved = false;
        if (newX != mCursorPosition.x) {
            mCursorPosition.x = newX;
            cursorMoved = true;
        }
        if (newY != mCursorPosition.y) {
            mCursorPosition.y = newY;
            cursorMoved = true;
        }

        return cursorMoved;
    }

    /**
     * Indicates whether all information required to render the canvas has been set.
     *
     * @return True if both screen and image dimensions have been set.
     */
    public boolean initialized() {
        return imageWidth != 0 && imageHeight != 0 && screenWidth != 0 && screenHeight != 0;
    }
}
