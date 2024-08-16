// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.content.Context;
import android.os.Bundle;

import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.Origin;

/** A mock Fido2CredentialRequest that returns NOT_IMPLEMENTED for all calls. */
public class MockFido2CredentialRequest extends Fido2CredentialRequest {
    private static final AuthenticationContextProvider STUB_PROVIDER =
            new AuthenticationContextProvider() {

                @Override
                public Context getContext() {
                    return null;
                }

                @Override
                public RenderFrameHost getRenderFrameHost() {
                    return null;
                }

                @Override
                public FidoIntentSender getIntentSender() {
                    return null;
                }

                @Override
                public WebContents getWebContents() {
                    return null;
                }
            };

    public MockFido2CredentialRequest() {
        super(STUB_PROVIDER);
    }

    @Override
    public void handleMakeCredentialRequest(
            PublicKeyCredentialCreationOptions options,
            byte[] maybeClientDataHash,
            Bundle browserOptions,
            Origin origin,
            Origin topOrigin,
            MakeCredentialResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        errorCallback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
    }

    @Override
    public void handleGetAssertionRequest(
            PublicKeyCredentialRequestOptions options,
            byte[] maybeClientDataHash,
            Origin callerOrigin,
            Origin topOrigin,
            PaymentOptions payment,
            GetAssertionResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        errorCallback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
    }

    @Override
    public void handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
            IsUvpaaResponseCallback callback) {
        callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
    }
}
