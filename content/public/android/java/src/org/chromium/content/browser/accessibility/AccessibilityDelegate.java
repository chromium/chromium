// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.graphics.Rect;
import android.view.View;
import android.view.ViewStructure;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;

/**
 * Provides some information/functionality to {@link WebContentsAccessibilityImpl} without directly
 * relying on {@link WebContents}. This enables {@link WebContentsAccessibilityImpl} to be used
 * without an instance of {@link WebContents}.
 */
public interface AccessibilityDelegate {
    /**
     * @return The {@link View} that contains the content that accessibility is used for.
     */
    View getContainerView();

    String getProductVersion();

    boolean isIncognito();

    /**
     * This can return null for situations where {@link WebContentsAccessibility} needs to work
     * without relying on {@link WebContents}. For example, by implementing
     * {@link #getNativeAXTree()}, it's possible to build WebContentsAccessibilityImpl based on an
     * accessibility tree that's not a live WebContents.
     */
    @Nullable
    WebContents getWebContents();

    AccessibilityCoordinates getAccessibilityCoordinates();

    /**
     * Requests an accessibility snapshot of the content that is currently being shown.
     * The ViewStructure starting with |root| will be populated and then |doneCallback| will
     * be called.
     */
    void requestAccessibilitySnapshot(ViewStructure root, Runnable doneCallback);

    /**
     * @return Native pointer to the accessibility tree snapshot.
     */
    default long getNativeAXTree() {
        return 0;
    }

    default void setOnScrollPositionChangedCallback(Runnable onScrollCallback) {}

    /**
     * This provides an opportunity to override the behavior when a click action occurs. If it
     * returns false, the click will be sent to WebContents.
     * @param nodeRect The Rect where the click action happened for.
     * @return Whether this event was handled.
     */
    default boolean performClick(Rect nodeRect) {
        return false;
    }

    /**
     * Called when the content needs to be scrolled to make a region visible.
     * @param nodeRect The designated Rect that should become visible.
     * @return Whether this delegate performed the scroll. If false, the scroll request will be sent
     * to WebContents.
     */
    default boolean scrollToMakeNodeVisible(Rect nodeRect) {
        return false;
    }

    /**
     * Used for providing the size and scrolling position, as well as conversion between local CSS
     * and physical pixels.
     */
    interface AccessibilityCoordinates {
        /**
         * @return Local CSS converted to physical coordinates
         */
        float fromLocalCssToPix(float css);

        /**
         * @return The Physical on-screen Y offset amount below the browser controls.
         */
        float getContentOffsetYPix();

        /**
         * @return Horizontal scroll offset in physical pixels.
         */
        float getScrollXPix();

        /**
         * @return Vertical scroll offset in physical pixels.
         */
        float getScrollYPix();

        /**
         * @return Render-reported width of the viewport in physical pixels (approx, integer).
         */
        int getLastFrameViewportWidthPixInt();

        /**
         * @return Render-reported height of the viewport in physical pixels (approx, integer).
         */
        int getLastFrameViewportHeightPixInt();

        /**
         * @return Width of the content in CSS pixels.
         */
        float getContentWidthCss();

        /**
         * @return Height of the content in CSS pixels.
         */
        float getContentHeightCss();

        /**
         * @return Horizontal scroll offset in CSS pixels.
         */
        float getScrollX();

        /**
         * @return Vertical scroll offset in CSS pixels.
         */
        float getScrollY();
    }
}
