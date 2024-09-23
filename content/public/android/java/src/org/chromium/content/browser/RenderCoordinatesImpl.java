// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.ResettersForTesting;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

/**
 * Cached copy of all positions and scales (CSS-to-DIP-to-physical pixels)
 * reported from the renderer.
 * Provides wrappers and a utility class to help with coordinate transforms on the client side.
 * Provides the internally-visible set of update methods.
 *
 * Unless stated otherwise, all coordinates are in CSS (document) coordinate space.
 */
public class RenderCoordinatesImpl implements RenderCoordinates {
    private static RenderCoordinatesImpl sInstanceForTesting;

    // Scroll offset from the native in CSS.
    private float mScrollXCss;
    private float mScrollYCss;

    // Content size from native in CSS.
    private float mContentWidthCss;
    private float mContentHeightCss;

    // Last-frame render-reported viewport size in CSS.
    private float mLastFrameViewportWidthCss;
    private float mLastFrameViewportHeightCss;

    // Cached page scale factor from native.
    private float mPageScaleFactor = 1.0f;
    private float mMinPageScaleFactor = 1.0f;
    private float mMaxPageScaleFactor = 1.0f;

    // Cached device density.
    private float mDeviceScaleFactor = 1.0f;

    private float mTopContentOffsetYPix;

    public static RenderCoordinatesImpl fromWebContents(WebContents webContents) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return ((WebContentsImpl) webContents).getRenderCoordinates();
    }

    // TODO(crbug.com/40850475): Mocking |#fromWebContents()| may be a better option, when
    // available.
    public static void setInstanceForTesting(RenderCoordinatesImpl instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    // Internally-visible set of update methods (used by WebContentsImpl).
    public void reset() {
        mScrollXCss = mScrollYCss = 0;
        mPageScaleFactor = 1.0f;
    }

    @Override
    public int getScrollXPixInt() {
        return (int) Math.floor(getScrollXPix());
    }

    @Override
    public int getScrollYPixInt() {
        return (int) Math.floor(getScrollYPix());
    }

    @Override
    public int getContentOffsetYPixInt() {
        return (int) Math.floor(getContentOffsetYPix());
    }

    @Override
    public int getContentWidthPixInt() {
        return (int) Math.ceil(getContentWidthPix());
    }

    @Override
    public int getContentHeightPixInt() {
        return (int) Math.ceil(getContentHeightPix());
    }

    @Override
    public int getLastFrameViewportWidthPixInt() {
        return (int) Math.ceil(getLastFrameViewportWidthPix());
    }

    @Override
    public int getLastFrameViewportHeightPixInt() {
        return (int) Math.ceil(getLastFrameViewportHeightPix());
    }

    @Override
    public int getMaxVerticalScrollPixInt() {
        return (int) Math.floor(getMaxVerticalScrollPix());
    }

    @Override
    public int getMaxHorizontalScrollPixInt() {
        return (int) Math.floor(getMaxHorizontalScrollPix());
    }

    void updateContentSizeCss(float contentWidthCss, float contentHeightCss) {
        mContentWidthCss = contentWidthCss;
        mContentHeightCss = contentHeightCss;
    }

    public void setDeviceScaleFactor(float dipScale) {
        mDeviceScaleFactor = dipScale;
    }

    public void updateFrameInfo(
            float contentWidthCss,
            float contentHeightCss,
            float viewportWidthCss,
            float viewportHeightCss,
            float minPageScaleFactor,
            float maxPageScaleFactor,
            float contentOffsetYPix) {
        mMinPageScaleFactor = minPageScaleFactor;
        mMaxPageScaleFactor = maxPageScaleFactor;
        mTopContentOffsetYPix = contentOffsetYPix;

        updateContentSizeCss(contentWidthCss, contentHeightCss);
        mLastFrameViewportWidthCss = viewportWidthCss;
        mLastFrameViewportHeightCss = viewportHeightCss;
    }

    public void updateScrollInfo(float pageScaleFactor, float scrollXCss, float scrollYCss) {
        mPageScaleFactor = pageScaleFactor;
        mScrollXCss = scrollXCss;
        mScrollYCss = scrollYCss;
    }

    /**
     * @return Horizontal scroll offset in CSS pixels.
     */
    public float getScrollX() {
        return mScrollXCss;
    }

    /**
     * @return Vertical scroll offset in CSS pixels.
     */
    public float getScrollY() {
        return mScrollYCss;
    }

    /**
     * @return Horizontal scroll offset in physical pixels.
     */
    public float getScrollXPix() {
        return fromLocalCssToPix(mScrollXCss);
    }

    /**
     * @return Vertical scroll offset in physical pixels.
     */
    public float getScrollYPix() {
        return fromLocalCssToPix(mScrollYCss);
    }

    /**
     * @return Width of the content in CSS pixels.
     */
    public float getContentWidthCss() {
        return mContentWidthCss;
    }

    /**
     * @return Height of the content in CSS pixels.
     */
    public float getContentHeightCss() {
        return mContentHeightCss;
    }

    /**
     * @return The Physical on-screen Y offset amount below the browser controls.
     */
    public float getContentOffsetYPix() {
        return mTopContentOffsetYPix;
    }

    /**
     * @return Current page scale factor (maps CSS pixels to DIP pixels).
     */
    @Override
    public float getPageScaleFactor() {
        return mPageScaleFactor;
    }

    /**
     * @return Current page scale factor (approx, integer).
     */
    @Override
    public int getPageScaleFactorInt() {
        return (int) Math.floor(mPageScaleFactor);
    }

    /**
     * @return Minimum page scale factor to be used with the content.
     */
    @Override
    public float getMinPageScaleFactor() {
        return mMinPageScaleFactor;
    }

    /**
     * @return Maximum page scale factor to be used with the content.
     */
    public float getMaxPageScaleFactor() {
        return mMaxPageScaleFactor;
    }

    /**
     * @return Current device scale factor (maps DIP pixels to physical pixels).
     */
    public float getDeviceScaleFactor() {
        return mDeviceScaleFactor;
    }

    /**
     * @return Local CSS converted to physical coordinates.
     */
    public float fromLocalCssToPix(float css) {
        return css * mPageScaleFactor * mDeviceScaleFactor;
    }

    // Private methods

    // Approximate width of the content in physical pixels.
    private float getContentWidthPix() {
        return fromLocalCssToPix(mContentWidthCss);
    }

    // Approximate height of the content in physical pixels.
    private float getContentHeightPix() {
        return fromLocalCssToPix(mContentHeightCss);
    }

    // Render-reported width of the viewport in physical pixels (approximate).
    private float getLastFrameViewportWidthPix() {
        return fromLocalCssToPix(mLastFrameViewportWidthCss);
    }

    // Render-reported height of the viewport in physical pixels (approximate).
    private float getLastFrameViewportHeightPix() {
        return fromLocalCssToPix(mLastFrameViewportHeightCss);
    }

    // Maximum possible horizontal scroll in physical pixels.
    private float getMaxHorizontalScrollPix() {
        return getContentWidthPix() - getLastFrameViewportWidthPix();
    }

    // Maximum possible vertical scroll in physical pixels.
    private float getMaxVerticalScrollPix() {
        return getContentHeightPix() - getLastFrameViewportHeightPix();
    }

    /**
     * @return whether the first frame info was passed in and cached. Rendered content
     *     area dimension, page scale factor, etc. is available if true.
     */
    public boolean frameInfoUpdatedForTesting() {
        return mContentWidthCss != 0.f || mContentHeightCss != 0.f;
    }
}
