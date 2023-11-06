// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.ui.base.WindowAndroid;

/** Class for issuing request to the Identity Credentials Manager in GMS core. */
@JNINamespace("content")
public class DigitalCredentialProvider {
    private static final String TAG = "DigitalCredentialProvider";
    private final long mDigitalCredentialProvider;
    private static IdentityCredentialsDelegate sCredentials = new IdentityCredentialsDelegateImpl();

    private DigitalCredentialProvider(long dcProvider) {
        mDigitalCredentialProvider = dcProvider;
    }

    public static void setDelegateForTesting(IdentityCredentialsDelegate mock) {
        var oldValue = sCredentials;
        sCredentials = mock;
        ResettersForTesting.register(() -> sCredentials = oldValue);
    }

    // Methods that are called by native implementation
    @CalledByNative
    private static DigitalCredentialProvider create(long dcProvider) {
        return new DigitalCredentialProvider(dcProvider);
    }

    @CalledByNative
    private void destroy() {}

    /**
     * Triggers a request to the Identity Credentials Manager in GMS.
     *
     * @param window The window associated with the request.
     * @param origin The origin of the requester.
     * @param request The request.
     */
    @CalledByNative
    void requestDigitalCredential(WindowAndroid window, String origin, String request) {
        sCredentials
                .get(window.getActivity().get(), origin, request)
                .then(
                        data -> {
                            DigitalCredentialProviderJni.get()
                                    .onReceive(mDigitalCredentialProvider, new String(data));
                        },
                        e -> {
                            DigitalCredentialProviderJni.get().onError(mDigitalCredentialProvider);
                        });
    }

    @NativeMethods
    interface Natives {
        void onReceive(long nativeDigitalCredentialProviderAndroid, String dc);

        void onError(long nativeDigitalCredentialProviderAndroid);
    }
}
