// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Interface used by ArOverlayDelegate to communicate with the underlying
 * compositor. Used to implement WebXR's DOM Overlay mode correctly.
 */
public interface VrCompositorDelegate {
    /** Enables/disables immersive VR mode in the compositor. */
    void setOverlayImmersiveVrMode(boolean enabled);

    /** Opens a new tab in the current browser. */
    void openNewTab(LoadUrlParams url);
}
