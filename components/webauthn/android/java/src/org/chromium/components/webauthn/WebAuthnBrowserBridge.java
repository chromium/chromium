// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.RenderFrameHost;

import java.util.List;

/**
 * Provides a bridge from the the Android Web Authentication request handlers
 * to the embedding browser.
 */
@JNINamespace("webauthn")
public class WebAuthnBrowserBridge {
    private long mNativeWebAuthnBrowserBridge;

    /**
     * Provides a list of discoverable credentials for user selection. If this is a conditional UI
     * request, then these credentials become available as options for autofill UI on sign-in input
     * fields. For non-conditional requests, a selection sheet is shown immediately. The callback
     * is invoked when a user selects one of the credentials from the list.
     *
     * @param frameHost The RenderFrameHost for the frame that generated the request.
     * @param credentialList The list of credentials that can be used as autofill suggestions.
     * @param isConditionalRequest Boolean indicating whether this is a conditional UI request or
     *     not.
     * @param getAssertionCallback The callback to be invoked with the credential ID of a selected
     *         credential.
     * @param hybridCallback The callback to be invoked if a user initiates a cross-device hybrid
     *     sign-in.
     */
    public void onCredentialsDetailsListReceived(RenderFrameHost frameHost,
            List<WebAuthnCredentialDetails> credentialList, boolean isConditionalRequest,
            Callback<byte[]> getAssertionCallback, Runnable hybridCallback) {
        assert credentialList != null;
        assert getAssertionCallback != null;
        prepareNativeBrowserBridgeIfRequired();

        WebAuthnCredentialDetails[] credentialArray =
                credentialList.toArray(new WebAuthnCredentialDetails[credentialList.size()]);
        WebAuthnBrowserBridgeJni.get().onCredentialsDetailsListReceived(
                mNativeWebAuthnBrowserBridge, WebAuthnBrowserBridge.this, credentialArray,
                frameHost, isConditionalRequest, getAssertionCallback, hybridCallback);
    }

    /**
     * Provides the C++ side |hasResults| and |fullAssertion| to be consumed during user interaction
     * (i.e. focusing login forms).
     *
     * @param frameHost The RenderFrameHost for the frame that generated the request.
     * @param hasResults The response from credMan whether there are credentials for the
     *         GetAssertion request.
     * @param fullAssertion The CredMan request to trigger UI for credential selection for the
     *         completed conditional request.
     */
    public void onCredManConditionalRequestPending(
            RenderFrameHost frameHost, boolean hasResults, Callback<Boolean> fullAssertion) {
        prepareNativeBrowserBridgeIfRequired();

        WebAuthnBrowserBridgeJni.get().onCredManConditionalRequestPending(
                mNativeWebAuthnBrowserBridge, frameHost, hasResults, fullAssertion);
    }

    /**
     * Notifies the C++ side that the credMan UI is closed.
     *
     * @param frameHost The RenderFrameHost related to this CredMan call
     * @param success true iff user is authenticated
     */
    public void onCredManUiClosed(RenderFrameHost frameHost, boolean success) {
        prepareNativeBrowserBridgeIfRequired();

        WebAuthnBrowserBridgeJni.get().onCredManUiClosed(
                mNativeWebAuthnBrowserBridge, frameHost, success);
    }

    public void onPasswordCredentialReceived(
            RenderFrameHost frameHost, String username, String password) {
        prepareNativeBrowserBridgeIfRequired();

        WebAuthnBrowserBridgeJni.get().onPasswordCredentialReceived(
                mNativeWebAuthnBrowserBridge, frameHost, username, password);
    }

    /**
     * Notifies the native code that an outstanding Conditional UI request initiated through
     * onCredentialsDetailsListReceived has been completed or canceled.
     *
     * @param frameHost The RenderFrameHost for the frame that generated the cancellation.
     */
    public void cleanupRequest(RenderFrameHost frameHost) {
        // This should never be called without a bridge already having been created.
        assert mNativeWebAuthnBrowserBridge != 0;

        WebAuthnBrowserBridgeJni.get().cleanupRequest(mNativeWebAuthnBrowserBridge, frameHost);
    }

    public void destroy() {
        if (mNativeWebAuthnBrowserBridge == 0) return;
        WebAuthnBrowserBridgeJni.get().destroy(mNativeWebAuthnBrowserBridge);
        mNativeWebAuthnBrowserBridge = 0;
    }

    @CalledByNative
    private static String getWebAuthnCredentialDetailsUserName(WebAuthnCredentialDetails cred) {
        return cred.mUserName;
    }

    @CalledByNative
    private static String getWebAuthnCredentialDetailsUserDisplayName(
            WebAuthnCredentialDetails cred) {
        return cred.mUserDisplayName;
    }

    @CalledByNative
    private static byte[] getWebAuthnCredentialDetailsUserId(WebAuthnCredentialDetails cred) {
        return cred.mUserId;
    }

    @CalledByNative
    private static byte[] getWebAuthnCredentialDetailsCredentialId(WebAuthnCredentialDetails cred) {
        return cred.mCredentialId;
    }

    private void prepareNativeBrowserBridgeIfRequired() {
        if (mNativeWebAuthnBrowserBridge == 0) {
            mNativeWebAuthnBrowserBridge =
                    WebAuthnBrowserBridgeJni.get().createNativeWebAuthnBrowserBridge(
                            WebAuthnBrowserBridge.this);
        }
    }

    @NativeMethods
    interface Natives {
        // Native methods are implemented in webauthn_browser_bridge.cc.
        long createNativeWebAuthnBrowserBridge(WebAuthnBrowserBridge caller);
        void onCredentialsDetailsListReceived(long nativeWebAuthnBrowserBridge,
                WebAuthnBrowserBridge caller, WebAuthnCredentialDetails[] credentialList,
                RenderFrameHost frameHost, boolean isConditionalRequest,
                Callback<byte[]> getAssertionCallback, Runnable hybridCallback);
        void onCredManConditionalRequestPending(long nativeWebAuthnBrowserBridge,
                RenderFrameHost frameHost, boolean hasResults, Callback<Boolean> fullAssertion);
        void onCredManUiClosed(
                long nativeWebAuthnBrowserBridge, RenderFrameHost frameHost, boolean success);
        void onPasswordCredentialReceived(long nativeWebAuthnBrowserBridge,
                RenderFrameHost frameHost, String username, String password);
        void cleanupRequest(long nativeWebAuthnBrowserBridge, RenderFrameHost frameHost);
        void destroy(long nativeWebAuthnBrowserBridge);
    }
}
