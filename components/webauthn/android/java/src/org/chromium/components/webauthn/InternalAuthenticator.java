// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;

/**
 * Acts as a bridge from InternalAuthenticator declared in
 * //components/webauthn/android/internal_authenticator_android.h to AuthenticatorImpl.
 *
 * <p>The origin associated with requests on InternalAuthenticator should be set by calling
 * setEffectiveOrigin() first.
 */
@JNINamespace("webauthn")
public class InternalAuthenticator {
    private long mNativeInternalAuthenticatorAndroid;
    private final AuthenticatorImpl mAuthenticator;

    private InternalAuthenticator(
            long nativeInternalAuthenticatorAndroid,
            Context context,
            WebContents webContents,
            FidoIntentSender intentSender,
            RenderFrameHost renderFrameHost,
            Origin topOrigin) {
        mNativeInternalAuthenticatorAndroid = nativeInternalAuthenticatorAndroid;
        WebauthnModeProvider.getInstance().setGlobalWebauthnMode(WebauthnMode.CHROME);
        mAuthenticator =
                new AuthenticatorImpl(
                        context,
                        webContents,
                        intentSender,
                        /* createConfirmationUiDelegate= */ null,
                        renderFrameHost,
                        topOrigin);
    }

    public static InternalAuthenticator createForTesting(
            Context context,
            FidoIntentSender intentSender,
            RenderFrameHost renderFrameHost,
            Origin topOrigin) {
        return new InternalAuthenticator(
                -1, context, /* webContents= */ null, intentSender, renderFrameHost, topOrigin);
    }

    @CalledByNative
    public static InternalAuthenticator create(
            long nativeInternalAuthenticatorAndroid, RenderFrameHost renderFrameHost) {
        final WebContents webContents = WebContentsStatics.fromRenderFrameHost(renderFrameHost);
        final WindowAndroid window = webContents.getTopLevelNativeWindow();
        final Context context = window.getActivity().get();
        final Origin topOrigin = webContents.getMainFrame().getLastCommittedOrigin();
        return new InternalAuthenticator(
                nativeInternalAuthenticatorAndroid,
                context,
                webContents,
                new AuthenticatorImpl.WindowIntentSender(window),
                renderFrameHost,
                topOrigin);
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
                        InternalAuthenticatorJni.get()
                                .invokeMakeCredentialResponse(
                                        mNativeInternalAuthenticatorAndroid,
                                        status,
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
                        InternalAuthenticatorJni.get()
                                .invokeGetAssertionResponse(
                                        mNativeInternalAuthenticatorAndroid,
                                        status,
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
        mAuthenticator.isUserVerifyingPlatformAuthenticatorAvailable(
                (isUVPAA) -> {
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
        return GmsCoreUtils.isGetMatchingCredentialIdsSupported();
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process. The response will be passed through
     * |invokeGetMatchingCredentialIdsResponse()|.
     */
    @CalledByNative
    public void getMatchingCredentialIds(
            String relyingPartyId, byte[][] credentialIds, boolean requireThirdPartyPayment) {
        mAuthenticator.getMatchingCredentialIds(
                relyingPartyId,
                credentialIds,
                requireThirdPartyPayment,
                (matchingCredentialIds) -> {
                    if (mNativeInternalAuthenticatorAndroid != 0) {
                        InternalAuthenticatorJni.get()
                                .invokeGetMatchingCredentialIdsResponse(
                                        mNativeInternalAuthenticatorAndroid,
                                        matchingCredentialIds.toArray(new byte[0][]));
                    }
                });
    }

    @CalledByNative
    public void cancel() {
        mAuthenticator.cancel();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
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
