// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.components.webauthn.WebauthnLogger.log;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.CredentialInfo;
import org.chromium.blink.mojom.CredentialType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.mojo_base.mojom.String16;

import java.util.List;

/**
 * Provides a bridge from the the Android Web Authentication request handlers to the embedding
 * browser.
 */
@JNINamespace("webauthn")
@NullMarked
public class WebauthnBrowserBridge {
    private static final String TAG = "WebauthnBrowserBridge";

    /** Owner of the bridge should implement this interface and cache the bridge. */
    public interface Provider {
        @Nullable WebauthnBrowserBridge getBridge();
    }

    private long mNativeWebauthnBrowserBridge;

    /**
     * Provides a list of discoverable credentials for user selection. If this is a conditional UI
     * request, then these credentials become available as options for autofill UI on sign-in input
     * fields. For non-conditional requests, a selection sheet is shown immediately. The callback is
     * invoked when a user selects one of the credentials from the list.
     *
     * <p>TODO(https://crbug.com/433543129): This interface has gotten a bit overly complicated. It
     * should be simplified by combining the callbacks into just two: one for credentials being
     * returned (password or passkey), and another for other actions.
     *
     * @param frameHost The RenderFrameHost for the frame that generated the request.
     * @param credentialList The list of credentials that can be used as autofill suggestions.
     * @param mediationType Value indicating whether the credentials are for a modal, conditional,
     *     or immediate request.
     * @param passkeyCallback The callback to be invoked with the credential ID of a selected
     *     WebAuthn credential.
     * @param passwordCallback The callback to be invoked with a selected password credential.
     * @param hybridCallback The callback to be invoked if a user initiates a cross-device hybrid
     *     sign-in.
     * @param rejectImmediateCallback The callback to be invoked if an immediate request will be
     *     completed without a credential.
     */
    public void onCredentialsDetailsListReceived(
            @Nullable RenderFrameHost frameHost,
            List<WebauthnCredentialDetails> credentialList,
            @AssertionMediationType int mediationType,
            Callback<byte[]> passkeyCallback,
            @Nullable Callback<CredentialInfo> passwordCallback,
            @Nullable Runnable hybridCallback,
            @Nullable Callback<Integer> rejectImmediateCallback) {
        assert credentialList != null;
        assert passkeyCallback != null;
        log(
                TAG,
                "onCredentialsDetailsListReceived, mediationType: %d, number of credentials:"
                        + " %d",
                mediationType,
                credentialList.size());
        prepareNativeBrowserBridgeIfRequired();

        WebauthnCredentialDetails[] credentialArray =
                credentialList.toArray(new WebauthnCredentialDetails[credentialList.size()]);
        WebauthnBrowserBridgeJni.get()
                .onCredentialsDetailsListReceived(
                        mNativeWebauthnBrowserBridge,
                        credentialArray,
                        frameHost,
                        mediationType,
                        passkeyCallback,
                        passwordCallback,
                        hybridCallback,
                        rejectImmediateCallback);
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
            @Nullable RenderFrameHost frameHost,
            boolean hasResults,
            Callback<Boolean> fullAssertion) {
        log(TAG, "onCredManConditionalRequestPending with hasResults: %b", hasResults);
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
    public void onCredManUiClosed(@Nullable RenderFrameHost frameHost, boolean success) {
        log(TAG, "onCredManUiClosed with success: %b", success);
        prepareNativeBrowserBridgeIfRequired();

        WebauthnBrowserBridgeJni.get()
                .onCredManUiClosed(mNativeWebauthnBrowserBridge, frameHost, success);
    }

    public void onPasswordCredentialReceived(
            @Nullable RenderFrameHost frameHost,
            @Nullable String username,
            @Nullable String password) {
        log(TAG, "onPasswordCredentialReceived");
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
    public void cleanupRequest(@Nullable RenderFrameHost frameHost) {
        log(TAG, "cleanupRequest");
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
    public void cleanupCredManRequest(@Nullable RenderFrameHost frameHost) {
        log(TAG, "cleanupCredManRequest");
        // This should never be called without a bridge already having been created.
        assert mNativeWebauthnBrowserBridge != 0;

        WebauthnBrowserBridgeJni.get()
                .cleanupCredManRequest(mNativeWebauthnBrowserBridge, frameHost);
    }

    public void destroy() {
        log(TAG, "destroy");
        if (mNativeWebauthnBrowserBridge == 0) return;
        WebauthnBrowserBridgeJni.get().destroy(mNativeWebauthnBrowserBridge);
        mNativeWebauthnBrowserBridge = 0;
    }

    public static @Nullable String16 stringToMojoString16(@Nullable String javaString) {
        if (javaString == null) {
            return null;
        }
        short[] data = new short[javaString.length()];
        for (int i = 0; i < data.length; i++) {
            data[i] = (short) javaString.charAt(i);
        }
        String16 mojoString = new String16();
        mojoString.data = data;
        return mojoString;
    }

    @CalledByNative
    private static @Nullable String getWebauthnCredentialDetailsUserName(
            WebauthnCredentialDetails cred) {
        return cred.mUserName;
    }

    @CalledByNative
    private static @Nullable String getWebauthnCredentialDetailsUserDisplayName(
            WebauthnCredentialDetails cred) {
        return cred.mUserDisplayName;
    }

    @CalledByNative
    private static byte @Nullable [] getWebauthnCredentialDetailsUserId(
            WebauthnCredentialDetails cred) {
        return cred.mUserId;
    }

    @CalledByNative
    private static byte @Nullable [] getWebauthnCredentialDetailsCredentialId(
            WebauthnCredentialDetails cred) {
        return cred.mCredentialId;
    }

    @CalledByNative
    private static CredentialInfo createPasswordCredentialInfo(String username, String password) {
        CredentialInfo passwordCredential = new CredentialInfo();
        passwordCredential.type = CredentialType.PASSWORD;
        String16 name = stringToMojoString16(username);
        passwordCredential.name = name;
        passwordCredential.id = name;
        passwordCredential.password = stringToMojoString16(password);
        return passwordCredential;
    }

    private void prepareNativeBrowserBridgeIfRequired() {
        if (mNativeWebauthnBrowserBridge == 0) {
            log(TAG, "prepareNativeBrowserBridgeIfRequired");
            mNativeWebauthnBrowserBridge =
                    WebauthnBrowserBridgeJni.get().createNativeWebauthnBrowserBridge(this);
        }
    }

    @NativeMethods
    interface Natives {
        // Native methods are implemented in webauthn_browser_bridge.cc.
        long createNativeWebauthnBrowserBridge(WebauthnBrowserBridge self);

        void onCredentialsDetailsListReceived(
                long nativeWebauthnBrowserBridge,
                WebauthnCredentialDetails[] credentialList,
                @Nullable RenderFrameHost frameHost,
                @AssertionMediationType int mediationType,
                Callback<byte[]> getAssertionCallback,
                @Nullable Callback<CredentialInfo> passwordCallback,
                @Nullable Runnable hybridCallback,
                @Nullable Callback<Integer> rejectImmediateCallback);

        void onCredManConditionalRequestPending(
                long nativeWebauthnBrowserBridge,
                @Nullable RenderFrameHost frameHost,
                boolean hasResults,
                Callback<Boolean> fullAssertion);

        void onCredManUiClosed(
                long nativeWebauthnBrowserBridge,
                @Nullable RenderFrameHost frameHost,
                boolean success);

        void onPasswordCredentialReceived(
                long nativeWebauthnBrowserBridge,
                @Nullable RenderFrameHost frameHost,
                @Nullable String username,
                @Nullable String password);

        void cleanupRequest(long nativeWebauthnBrowserBridge, @Nullable RenderFrameHost frameHost);

        void cleanupCredManRequest(
                long nativeWebauthnBrowserBridge, @Nullable RenderFrameHost frameHost);

        void destroy(long nativeWebauthnBrowserBridge);
    }
}
