// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

/**
 * Callback interface for receiving a response from a request to call
 * IsUserVerifyingPlatformAuthenticator.
 */
public interface IsUvpaaResponseCallback {
    public void onIsUserVerifyingPlatformAuthenticatorAvailableResponse(boolean isUVPAA);
}
