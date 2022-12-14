// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.RenderFrameHost;

import java.util.List;

/**
 * Provides a bridge from the the Android Web Authentication request handlers
 * to the embedding browser.
 */
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
     * @param callback The callback to be invoked with the credential ID of a selected credential.
     */
    public void onCredentialsDetailsListReceived(RenderFrameHost frameHost,
            List<WebAuthnCredentialDetails> credentialList, boolean isConditionalRequest,
            Callback<byte[]> callback) {
        assert credentialList != null;
        assert callback != null;

        if (mNativeWebAuthnBrowserBridge == 0) {
            mNativeWebAuthnBrowserBridge =
                    WebAuthnBrowserBridgeJni.get().createNativeWebAuthnBrowserBridge(
                            WebAuthnBrowserBridge.this);
        }

        WebAuthnCredentialDetails[] credentialArray =
                credentialList.toArray(new WebAuthnCredentialDetails[credentialList.size()]);
        WebAuthnBrowserBridgeJni.get().onCredentialsDetailsListReceived(
                mNativeWebAuthnBrowserBridge, WebAuthnBrowserBridge.this, credentialArray,
                frameHost, isConditionalRequest, callback);
    }

    /**
     * Cancels an outstanding Conditional UI request that was initiated through
     * onCredentialsDetailsListReceived. This causes the callback to be invoked with an
     * empty credential.
     *
     * @param frameHost The RenderFrameHost for the frame that generated the cancellation.
     */
    public void cancelRequest(RenderFrameHost frameHost) {
        // This should never be called without a bridge already having been created.
        assert mNativeWebAuthnBrowserBridge != 0;

        WebAuthnBrowserBridgeJni.get().cancelRequest(mNativeWebAuthnBrowserBridge, frameHost);
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

    @NativeMethods
    interface Natives {
        // Native methods are implemented in webauthn_browser_bridge.cc.
        long createNativeWebAuthnBrowserBridge(WebAuthnBrowserBridge caller);
        void onCredentialsDetailsListReceived(long nativeWebAuthnBrowserBridge,
                WebAuthnBrowserBridge caller, WebAuthnCredentialDetails[] credentialList,
                RenderFrameHost frameHost, boolean isConditionalRequest, Callback<byte[]> callback);
        void cancelRequest(long nativeWebAuthnBrowserBridge, RenderFrameHost frameHost);
    }
}
