// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * Interface used by ChromeActivity to communicate with AR code that is only
 * available if |enable_arcore| is set to true at build time.
 */
public interface ArDelegate extends BackPressHandler {
    /**
     * Used to let AR immersive mode intercept the Back button to exit immersive mode.
     */
    boolean onBackPressed();

    /**
     * Used to query if there is an active immersive AR Session.
     */
    boolean hasActiveArSession();
}
