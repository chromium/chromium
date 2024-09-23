// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.RenderFrameHost;

import java.util.List;

/**
 * Provides a bridge from the the Android Web Authentication request handlers to the embedding
 * browser.
 */
@JNINamespace("webauthn")
public class WebauthnBrowserBridge {
    /** Owner of the bridge should implement this interface and cache the bridge. */
    public interface Provider {
        WebauthnBrowserBridge getBridge();
    }

    private long mNativeWebauthnBrowserBridge;

    /**
     * Provides a list of discoverable credentials for user selection. If this is a conditional UI
     * request, then these credentials become available as options for autofill UI on sign-in input
     * fields. For non-conditional requests, a selection sheet is shown immediately. The callback is
     * invoked when a user selects one of the credentials from the list.
     *
     * @param frameHost The RenderFrameHost for the frame that generated the request.
     * @param credentialList The list of credentials that can be used as autofill suggestions.
     * @param isConditionalRequest Boolean indicating whether this is a conditional UI request or
     *     not.
     * @param getAssertionCallback The callback to be invoked with the credential ID of a selected
     *     credential.
     * @param hybridCallback The callback to be invoked if a user initiates a cross-device hybrid
     *     sign-in.
     */
    public void onCredentialsDetailsListReceived(
            RenderFrameHost frameHost,
            List<WebauthnCredentialDetails> credentialList,
            boolean isConditionalRequest,
            Callback<byte[]> getAssertionCallback,
            Runnable hybridCallback) {
        assert credentialList != null;
        assert getAssertionCallback != null;
        prepareNativeBrowserBridgeIfRequired();

        WebauthnCredentialDetails[] credentialArray =
                credentialList.toArray(new WebauthnCredentialDetails[credentialList.size()]);
        WebauthnBrowserBridgeJni.get()
                .onCredentialsDetailsListReceived(
                        mNativeWebauthnBrowserBridge,
                        WebauthnBrowserBridge.this,
                        credentialArray,
                        frameHost,
                        isConditionalRequest,
                        getAssertionCallback,
                        hybridCallback);
    }

    /**
     * Provides the C++ side |hasResults| and |fullAssertion| to be consumed during user interaction
     * (i.e. focusing login forms).
     *
     * @param frameHost The RenderFrameHost for the frame that generated the request.
     * @param hasResults The response from credMan whether there are credentials for the
     *     GetAssertion request.
     * @param fullAssertion The CredMan request to trigger UI for credential selection for the
     *     completed conditional request.
     */
    public void onCredManConditionalRequestPending(
            RenderFrameHost frameHost, boolean hasResults, Callback<Boolean> fullAssertion) {
        prepareNativeBrowserBridgeIfRequired();

        WebauthnBrowserBridgeJni.get()
                .onCredManConditionalRequestPending(
                        mNativeWebauthnBrowserBridge, frameHost, hasResults, fullAssertion);
    }

    /**
     * Notifies the C++ side that the credMan UI is closed.
     *
     * @param frameHost The RenderFrameHost related to this CredMan call
     * @param success true iff user is authenticated
     */
    public void onCredManUiClosed(RenderFrameHost frameHost, boolean success) {
        prepareNativeBrowserBridgeIfRequired();

        WebauthnBrowserBridgeJni.get()
                .onCredManUiClosed(mNativeWebauthnBrowserBridge, frameHost, success);
    }

    public void onPasswordCredentialReceived(
            RenderFrameHost frameHost, String username, String password) {
        prepareNativeBrowserBridgeIfRequired();

        WebauthnBrowserBridgeJni.get()
                .onPasswordCredentialReceived(
                        mNativeWebauthnBrowserBridge, frameHost, username, password);
    }

    /**
     * Notifies the native code that an outstanding Conditional UI request initiated through
     * onCredentialsDetailsListReceived has been completed or canceled.
     *
     * @param frameHost The RenderFrameHost for the frame that generated the cancellation.
     */
    public void cleanupRequest(RenderFrameHost frameHost) {
        // This should never be called without a bridge already having been created.
        assert mNativeWebauthnBrowserBridge != 0;

        WebauthnBrowserBridgeJni.get().cleanupRequest(mNativeWebauthnBrowserBridge, frameHost);
    }

    /**
     * Notifies the native code that an outstanding Android Credential Management
     * prepareGetCredential request has been canceled.
     *
     * @param frameHost The RenderFrameHost for the frame that generated the cancellation.
     */
    public void cleanupCredManRequest(RenderFrameHost frameHost) {
        // This should never be called without a bridge already having been created.
        assert mNativeWebauthnBrowserBridge != 0;

        WebauthnBrowserBridgeJni.get()
                .cleanupCredManRequest(mNativeWebauthnBrowserBridge, frameHost);
    }

    public void destroy() {
        if (mNativeWebauthnBrowserBridge == 0) return;
        WebauthnBrowserBridgeJni.get().destroy(mNativeWebauthnBrowserBridge);
        mNativeWebauthnBrowserBridge = 0;
    }

    @CalledByNative
    private static String getWebauthnCredentialDetailsUserName(WebauthnCredentialDetails cred) {
        return cred.mUserName;
    }

    @CalledByNative
    private static String getWebauthnCredentialDetailsUserDisplayName(
            WebauthnCredentialDetails cred) {
        return cred.mUserDisplayName;
    }

    @CalledByNative
    private static byte[] getWebauthnCredentialDetailsUserId(WebauthnCredentialDetails cred) {
        return cred.mUserId;
    }

    @CalledByNative
    private static byte[] getWebauthnCredentialDetailsCredentialId(WebauthnCredentialDetails cred) {
        return cred.mCredentialId;
    }

    private void prepareNativeBrowserBridgeIfRequired() {
        if (mNativeWebauthnBrowserBridge == 0) {
            mNativeWebauthnBrowserBridge =
                    WebauthnBrowserBridgeJni.get()
                            .createNativeWebauthnBrowserBridge(WebauthnBrowserBridge.this);
        }
    }

    @NativeMethods
    interface Natives {
        // Native methods are implemented in webauthn_browser_bridge.cc.
        long createNativeWebauthnBrowserBridge(WebauthnBrowserBridge caller);

        void onCredentialsDetailsListReceived(
                long nativeWebauthnBrowserBridge,
                WebauthnBrowserBridge caller,
                WebauthnCredentialDetails[] credentialList,
                RenderFrameHost frameHost,
                boolean isConditionalRequest,
                Callback<byte[]> getAssertionCallback,
                Runnable hybridCallback);

        void onCredManConditionalRequestPending(
                long nativeWebauthnBrowserBridge,
                RenderFrameHost frameHost,
                boolean hasResults,
                Callback<Boolean> fullAssertion);

        void onCredManUiClosed(
                long nativeWebauthnBrowserBridge, RenderFrameHost frameHost, boolean success);

        void onPasswordCredentialReceived(
                long nativeWebauthnBrowserBridge,
                RenderFrameHost frameHost,
                String username,
                String password);

        void cleanupRequest(long nativeWebauthnBrowserBridge, RenderFrameHost frameHost);

        void cleanupCredManRequest(long nativeWebauthnBrowserBridge, RenderFrameHost frameHost);

        void destroy(long nativeWebauthnBrowserBridge);
    }
}
