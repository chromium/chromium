// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

/**
 * Interface used by ChromeActivity to communicate with AR code that is only
 * available if |enable_arcore| is set to true at build time.
 */
public interface ArDelegate {
    /**
     * Used to let AR immersive mode intercept the Back button to exit immersive mode.
     */
    public boolean onBackPressed();

    /**
     * Used to query if there is an active immersive AR Session.
     */
    public boolean hasActiveArSession();
}
