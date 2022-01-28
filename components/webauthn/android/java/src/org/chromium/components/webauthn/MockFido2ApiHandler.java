// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.url.Origin;

/** A mock Fido2ApiHandler that returns NOT_IMPLEMENTED for all calls. */
public class MockFido2ApiHandler extends Fido2ApiHandler {
    @Override
    protected void makeCredential(PublicKeyCredentialCreationOptions options,
            WebAuthenticationDelegate.IntentSender intentSender, RenderFrameHost frameHost,
            Origin origin, @WebAuthenticationDelegate.Support int supportLevel,
            MakeCredentialResponseCallback callback, FidoErrorResponseCallback errorCallback) {
        errorCallback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
    }

    @Override
    protected void getAssertion(PublicKeyCredentialRequestOptions options,
            WebAuthenticationDelegate.IntentSender intentSender, RenderFrameHost frameHost,
            Origin origin, PaymentOptions payment,
            @WebAuthenticationDelegate.Support int supportLevel,
            GetAssertionResponseCallback callback, FidoErrorResponseCallback errorCallback) {
        errorCallback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
    }

    @Override
    protected void isUserVerifyingPlatformAuthenticatorAvailable(
            WebAuthenticationDelegate.IntentSender intentSender, RenderFrameHost frameHost,
            @WebAuthenticationDelegate.Support int supportLevel, IsUvpaaResponseCallback callback) {
        callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
    }
}
