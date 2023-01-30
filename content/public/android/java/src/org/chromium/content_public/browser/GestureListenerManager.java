// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency;
import org.chromium.content.browser.GestureListenerManagerImpl;

/**
 * Manages the {@link GestureStateListener} instances observing various gesture
 * events notifications from content layer.
 */
public interface GestureListenerManager {
    /**
     * @param webContents {@link WebContents} object.
     * @return {@link GestureListenerManager} object used for the give WebContents.
     *         Creates one if not present.
     */
    static GestureListenerManager fromWebContents(WebContents webContents) {
        return GestureListenerManagerImpl.fromWebContents(webContents);
    }

    /**
     * Add a listener that gets alerted on gesture state changes. This is the same as calling {@link
     * #addListener(GestureStateListener, @RootScrollOffsetUpdateFrequency.EnumType int)} with
     * `NONE` as the second argument.
     *
     * @param listener Listener to add.
     */
    void addListener(GestureStateListener listener);

    /**
     * Add a listener that gets alerted on gesture state and potentially scroll offset changes based
     * on the `frequency` provided.
     *
     * WARNING: attaching a listener with a frequency higher than `NONE` results in extra IPC that
     * impacts rendering performance. Only attach listeners when absolutely necessary and remove as
     * soon as possible. See {@link RootScrollOffsetUpdateFrequency} for details.
     *
     * @param listener Listener to add.
     * @param frequency The {@link RootScrollOffsetUpdateFrequency} this listener needs.
     */
    void addListener(
            GestureStateListener listener, @RootScrollOffsetUpdateFrequency.EnumType int frequency);

    /**
     * Removes a listener that was added to watch for gesture state changes.
     * @param listener Listener to remove.
     */
    void removeListener(GestureStateListener listener);

    /** Returns whether the provided listener has been added. */
    boolean hasListener(GestureStateListener listener);

    /**
     * @return Whether a scroll targeting web content is in progress.
     */
    boolean isScrollInProgress();

    /**
     * Enable or disable multi-touch zoom support.
     * @param supportsMultiTouchZoom {@code true} if the feature is enabled.
     */
    void updateMultiTouchZoomSupport(boolean supportsMultiTouchZoom);

    /**
     * Enable or disable double tap support.
     * @param supportsDoubleTap {@code true} if the feature is enabled.
     */
    void updateDoubleTapSupport(boolean supportsDoubleTap);
}
