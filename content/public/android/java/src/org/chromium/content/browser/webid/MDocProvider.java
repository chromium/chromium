// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * Class for issuing request to the Identity Credentials Manager in GMS core.
 */
@JNINamespace("content")
public class MDocProvider {
    private static final String TAG = "WebIdentityMDoc";
    private final long mMDocProvider;
    private static IdentityCredentialsDelegate sCredentials = new IdentityCredentialsDelegateImpl();

    private MDocProvider(long mDocProvider) {
        mMDocProvider = mDocProvider;
    }

    public static void setDelegateForTesting(IdentityCredentialsDelegate mock) {
        var oldValue = sCredentials;
        sCredentials = mock;
        ResettersForTesting.register(() -> sCredentials = oldValue);
    }

    // Methods that are called by native implementation
    @CalledByNative
    private static MDocProvider create(long mDocProvider) {
        return new MDocProvider(mDocProvider);
    }

    @CalledByNative
    private void destroy() {}

    /**
     * Triggers a request to the Identity Credentials Manager in GMS.
     * @param window The window associated with the request.
     * @param origin The origin of the requester.
     * @param request The request.
     */
    @CalledByNative
    void requestMDoc(WindowAndroid window, String origin, String request) {
        sCredentials.get(window.getActivity().get(), origin, request)
                .then(data
                        -> { MDocProviderJni.get().onReceive(mMDocProvider, new String(data)); },
                        e -> { MDocProviderJni.get().onError(mMDocProvider); });
    }

    @NativeMethods
    interface Natives {
        void onReceive(long nativeMDocProviderAndroid, String mdoc);
        void onError(long nativeMDocProviderAndroid);
    }
}
