// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.url.Origin;

/**
 * Android implementation of the Authenticator service defined in
 * //third_party/blink/public/mojom/webauth/authenticator.mojom.
 */
public class Fido2ApiHandler {
    private static Fido2ApiHandler sInstance;
    private static final String GMSCORE_PACKAGE_NAME = "com.google.android.gms";
    public static final int GMSCORE_MIN_VERSION = 16890000;

    @VisibleForTesting
    public static void overrideInstanceForTesting(Fido2ApiHandler instance) {
        sInstance = instance;
    }

    /**
     * @return The Fido2ApiHandler for use during the lifetime of the browser process.
     */
    public static Fido2ApiHandler getInstance() {
        ThreadUtils.checkUiThread();
        if (sInstance == null) {
            // The Fido2 APIs can only be used on GmsCore v2020w12+.
            // This check is only if sInstance is null since some tests may
            // override sInstance for testing.
            assert PackageUtils.getPackageVersion(
                    ContextUtils.getApplicationContext(), GMSCORE_PACKAGE_NAME)
                    >= GMSCORE_MIN_VERSION;

            sInstance = new Fido2ApiHandler();
        }
        return sInstance;
    }

    protected void makeCredential(PublicKeyCredentialCreationOptions options,
            WebAuthenticationDelegate.IntentSender intentSender, RenderFrameHost frameHost,
            Origin origin, @WebAuthenticationDelegate.Support int supportLevel,
            MakeCredentialResponseCallback callback, FidoErrorResponseCallback errorCallback) {
        new Fido2CredentialRequest(intentSender, supportLevel)
                .handleMakeCredentialRequest(options, frameHost, origin, callback, errorCallback);
    }

    protected void getAssertion(PublicKeyCredentialRequestOptions options,
            WebAuthenticationDelegate.IntentSender intentSender, RenderFrameHost frameHost,
            Origin origin, PaymentOptions payment,
            @WebAuthenticationDelegate.Support int supportLevel,
            GetAssertionResponseCallback callback, FidoErrorResponseCallback errorCallback) {
        new Fido2CredentialRequest(intentSender, supportLevel)
                .handleGetAssertionRequest(
                        options, frameHost, origin, payment, callback, errorCallback);
    }

    protected void isUserVerifyingPlatformAuthenticatorAvailable(
            WebAuthenticationDelegate.IntentSender intentSender, RenderFrameHost frameHost,
            @WebAuthenticationDelegate.Support int supportLevel, IsUvpaaResponseCallback callback) {
        new Fido2CredentialRequest(intentSender, supportLevel)
                .handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(frameHost, callback);
    }
}
