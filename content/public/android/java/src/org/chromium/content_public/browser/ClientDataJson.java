// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.content.browser.ClientDataJsonImpl;
import org.chromium.url.Origin;

/** A utility class for WebAuthn to process the clientDataJson data structure in the API. */
public final class ClientDataJson {
    private ClientDataJson() {}

    /**
     * Builds the CollectedClientData[1] dictionary with the given values, serializes it to JSON,
     * and returns the resulting string.
     * [1] https://w3c.github.io/webauthn/#dictdef-collectedclientdata
     * @param clientDataRequestType The type of the client data request.
     * @param callerOrigin The origin of the API caller.
     * @param challenge The challenge provided to the WebAuthn API.
     * @param isCrossOrigin Whether the origin of the caller frame is different from its
     * @param paymentOptions The Payment parameters passed into calls to GetAssertion for Secure
     *        Payment Confirmation.
     * @param relyingPartyId The id of the relying party, which is an origin.
     * @param topOrigin The origin of the page.
     * @return The string of the JSON, can be null when error happens.
     */
    @Nullable
    public static String buildClientDataJson(
            @ClientDataRequestType int clientDataRequestType,
            @NonNull String callerOrigin,
            @NonNull byte[] challenge,
            boolean isCrossOrigin,
            @Nullable PaymentOptions paymentOptions,
            @Nullable String relyingPartyId,
            @Nullable Origin topOrigin) {
        assert challenge != null;
        return ClientDataJsonImpl.buildClientDataJson(
                clientDataRequestType,
                callerOrigin,
                challenge,
                isCrossOrigin,
                paymentOptions,
                relyingPartyId,
                topOrigin);
    }
}
