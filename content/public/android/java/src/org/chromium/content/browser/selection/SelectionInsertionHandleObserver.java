// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

/**
 * An interface for observing selection/insertion touch handles.
 */
public interface SelectionInsertionHandleObserver {
    /**
     * Process with handle drag started and handle moving events.
     * @param x The x coordinate of the middle point of selection/insertion bound cooresponding to
     *          the dragging handle.
     * @param y The y coordinate of the middle point of selection/insertion bound cooresponding to
     *          the dragging handle.
     */
    void handleDragStartedOrMoved(float x, float y);

    /**
     * Process with handle drag stopped event.
     */
    void handleDragStopped();
}
