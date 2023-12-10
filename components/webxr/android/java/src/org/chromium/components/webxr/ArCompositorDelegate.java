// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

/**
 * Interface used by ArOverlayDelegate to communicate with the underlying
 * compositor. Used to implement WebXR's DOM Overlay mode correctly.
 */
public interface ArCompositorDelegate {
    /** Enables/disables immersive AR mode in the compositor. */
    void setOverlayImmersiveArMode(boolean enabled, boolean domSurfaceNeedsConfiguring);

    /**
     * Dispatches a touch event that was consumed by the immersive AR overlay.
     * This touch event should be propagated to the View where the DOM Overlay
     * content is displayed so that it can react to the user's actions.
     */
    void dispatchTouchEvent(MotionEvent ev);

    /**
     * Returns the ViewGroup that the AR SurfaceView should be parented to. Note
     * that it should *not* be under the same ViewGroup as |dispatchTouchEvent|
     * sends its events to, as an infinite loop can occur. It should however,
     * be positioned under the elements that would cause infobars/prompts to
     * appear, but over any default (e.g. DOM) content.
     */
    @NonNull
    ViewGroup getArSurfaceParent();

    /**
     * Returns whether or not the ViewGroup retrieved from |getArSurfaceParent|
     * should have its visibility toggled in parenting/removing the AR
     * SurfaceView (if, e.g. the AR SurfaceView is the only child).
     */
    boolean shouldToggleArSurfaceParentVisibility();
}
