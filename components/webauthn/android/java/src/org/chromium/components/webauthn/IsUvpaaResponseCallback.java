// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.build.annotations.NullMarked;

/**
 * Callback interface for receiving a response from a request to call
 * IsUserVerifyingPlatformAuthenticator.
 */
@NullMarked
public interface IsUvpaaResponseCallback {
    public void onIsUserVerifyingPlatformAuthenticatorAvailableResponse(boolean isUVPAA);
}
