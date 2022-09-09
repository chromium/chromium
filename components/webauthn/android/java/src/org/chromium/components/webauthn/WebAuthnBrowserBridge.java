// Copyright 2022 The Chromium Authors. All rights reserved.
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
     * Provides a list of credentials for WebAuthn Conditional UI. These credentials become
     * available as options for autofill UI on sign-in input fields. The callback is invoked when
     * a user selects one of the credentials from the list.
     *
     * @param credentialList The list of credentials that can be used as autofill suggestions.
     * @param callback The callback to be invoked with the credential ID of a selected credential.
     */
    public void onCredentialsDetailsListReceived(RenderFrameHost frameHost,
            List<WebAuthnCredentialDetails> credentialList, Callback<byte[]> callback) {
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
                frameHost, callback);
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
                RenderFrameHost frameHost, Callback<byte[]> callback);
    }
}
