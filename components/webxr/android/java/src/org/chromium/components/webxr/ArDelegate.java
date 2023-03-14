// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.base.supplier.ObservableSupplier;

/**
 * Interface used by XrDelegate to communicate with AR code that is only
 * available if |enable_arcore| is set to true at build time.
 */
/*package*/ interface ArDelegate {
    /**
     * Used to let AR immersive mode intercept the Back button to exit immersive mode.
     */
    boolean onBackPressed();

    /**
     * Used to query and notify if there is an active immersive AR Session.
     */
    ObservableSupplier<Boolean> getHasActiveArSessionSupplier();
}
