// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

/**
 * Public implementation recommending the CredMan UI Mode. Configuration may be replaced by a
 * browser downstream implementation.
 */
class CredManUiModeRecommender {
    /**
     * Returns a recommendation on whether to use a custom UI over CredMan calls.
     *
     * @return True only iff the browser downstream identifies an advantage of custom UI.
     */
    boolean recommendsCustomUi() {
        return false; // Use the platform CredMan APIs if available.
    }
}
