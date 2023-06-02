// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;

/**
 * Acts as a bridge from InternalAuthenticator declared in
 * //components/webauthn/android/internal_authenticator_android.h to AuthenticatorImpl.
 *
 * The origin associated with requests on InternalAuthenticator should be set by calling
 * setEffectiveOrigin() first.
 */
public class InternalAuthenticator {
    private long mNativeInternalAuthenticatorAndroid;
    private final AuthenticatorImpl mAuthenticator;

    private InternalAuthenticator(long nativeInternalAuthenticatorAndroid,
            WebAuthenticationDelegate.IntentSender intentSender, RenderFrameHost renderFrameHost) {
        mNativeInternalAuthenticatorAndroid = nativeInternalAuthenticatorAndroid;
        mAuthenticator = new AuthenticatorImpl(intentSender, renderFrameHost);
    }

    @VisibleForTesting
    public static InternalAuthenticator createForTesting(
            WebAuthenticationDelegate.IntentSender intentSender, RenderFrameHost renderFrameHost) {
        return new InternalAuthenticator(-1, intentSender, renderFrameHost);
    }

    @CalledByNative
    public static InternalAuthenticator create(
            long nativeInternalAuthenticatorAndroid, RenderFrameHost renderFrameHost) {
        return new InternalAuthenticator(
                nativeInternalAuthenticatorAndroid, /* intentSender= */ null, renderFrameHost);
    }

    @CalledByNative
    public void clearNativePtr() {
        mNativeInternalAuthenticatorAndroid = 0;
    }

    @CalledByNative
    public void setEffectiveOrigin(Origin origin) {
        mAuthenticator.setEffectiveOrigin(origin);
    }

    @CalledByNative
    public void setPaymentOptions(ByteBuffer payment) {
        mAuthenticator.setPaymentOptions(PaymentOptions.deserialize(payment));
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process.
     */
    @CalledByNative
    public void makeCredential(ByteBuffer optionsByteBuffer) {
        mAuthenticator.makeCredential(
                PublicKeyCredentialCreationOptions.deserialize(optionsByteBuffer),
                (status, response, domExceptionDetails) -> {
                    // DOMExceptions can only be passed through the webAuthenticationProxy
                    // extensions API, which doesn't exist on Android.
                    assert status != AuthenticatorStatus.ERROR_WITH_DOM_EXCEPTION_DETAILS
                            && domExceptionDetails == null;
                    if (mNativeInternalAuthenticatorAndroid != 0) {
                        InternalAuthenticatorJni.get().invokeMakeCredentialResponse(
                                mNativeInternalAuthenticatorAndroid, status.intValue(),
                                response == null ? null : response.serialize());
                    }
                });
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process.
     */
    @CalledByNative
    public void getAssertion(ByteBuffer optionsByteBuffer) {
        mAuthenticator.getAssertion(
                PublicKeyCredentialRequestOptions.deserialize(optionsByteBuffer),
                (status, response, domExceptionDetails) -> {
                    // DOMExceptions can only be passed through the webAuthenticationProxy
                    // extensions API, which doesn't exist on Android.
                    assert status != AuthenticatorStatus.ERROR_WITH_DOM_EXCEPTION_DETAILS
                            && domExceptionDetails == null;
                    if (mNativeInternalAuthenticatorAndroid != 0) {
                        InternalAuthenticatorJni.get().invokeGetAssertionResponse(
                                mNativeInternalAuthenticatorAndroid, status.intValue(),
                                response == null ? null : response.serialize());
                    }
                });
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process. The response will be passed through
     * |invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse()|.
     */
    @CalledByNative
    public void isUserVerifyingPlatformAuthenticatorAvailable() {
        mAuthenticator.isUserVerifyingPlatformAuthenticatorAvailable((isUVPAA) -> {
            if (mNativeInternalAuthenticatorAndroid != 0) {
                InternalAuthenticatorJni.get()
                        .invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                                mNativeInternalAuthenticatorAndroid, isUVPAA);
            }
        });
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process.
     */
    @CalledByNative
    public boolean isGetMatchingCredentialIdsSupported() {
        return mAuthenticator.isGetMatchingCredentialIdsSupported();
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process. The response will be passed through
     * |invokeGetMatchingCredentialIdsResponse()|.
     */
    @CalledByNative
    public void getMatchingCredentialIds(
            String relyingPartyId, byte[][] credentialIds, boolean requireThirdPartyPayment) {
        mAuthenticator.getMatchingCredentialIds(relyingPartyId, credentialIds,
                requireThirdPartyPayment, (matchingCredentialIds) -> {
                    if (mNativeInternalAuthenticatorAndroid != 0) {
                        InternalAuthenticatorJni.get().invokeGetMatchingCredentialIdsResponse(
                                mNativeInternalAuthenticatorAndroid,
                                matchingCredentialIds.toArray(new byte[0][]));
                    }
                });
    }

    @CalledByNative
    public void cancel() {
        mAuthenticator.cancel();
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void invokeMakeCredentialResponse(
                long nativeInternalAuthenticatorAndroid, int status, ByteBuffer byteBuffer);
        void invokeGetAssertionResponse(
                long nativeInternalAuthenticatorAndroid, int status, ByteBuffer byteBuffer);
        void invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                long nativeInternalAuthenticatorAndroid, boolean isUVPAA);
        void invokeGetMatchingCredentialIdsResponse(
                long nativeInternalAuthenticatorAndroid, byte[][] matchingCredentialIds);
    }
}
