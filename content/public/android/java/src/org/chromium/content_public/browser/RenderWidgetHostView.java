// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.Callback;

/**
 * The Android interface to allow communicating with the RenderWidgetHostViewImpl and the native
 * RenderWidgetHostViewAndroid object.  This object allows the browser to access and control the
 * renderer's top level View.
 */
public interface RenderWidgetHostView {
    /**
     * If the view is ready to draw contents to the screen. In hardware mode,
     * the initialization of the surface texture may not occur until after the
     * view has been added to the layout. This method will return {@code true}
     * once the texture is actually ready.
     */
    boolean isReady();

    /**
     * Get the Background color from underlying RenderWidgetHost for this WebContent.
     */
    int getBackgroundColor();

    /**
     * Requests an image snapshot of the content and stores it in the specified folder.
     *
     * @param width The width of the resulting bitmap, or 0 for "auto."
     * @param height The height of the resulting bitmap, or 0 for "auto."
     * @param path The folder in which to store the screenshot.
     * @param callback May be called synchronously, or at a later point, to deliver the bitmap
     *                 result (or a failure code).
     */
    void writeContentBitmapToDiskAsync(
            int width, int height, String path, Callback<String> callback);

    /**
     * Notifies that the Visual Viewport inset has changed its bottom value.
     */
    void onViewportInsetBottomChanged();
}
