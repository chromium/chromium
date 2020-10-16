// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.view.MotionEvent;

/**
 * Interface used by ArImmersiveOverlay to communicate with the underlying
 * compositor. Used to implement WebXR's DOM Overlay mode correctly.
 */
public interface ArCompositorDelegate {
    /**
     * Enables/disables immersive AR mode in the compositor.
     */
    void setOverlayImmersiveArMode(boolean enabled);

    /**
     * Dispatches a touch event that was consumed by the immersive AR overlay.
     * This touch event should be propagated to the View where the DOM Overlay
     * content is displayed so that it can react to the user's actions.
     */
    void dispatchTouchEvent(MotionEvent ev);
}
