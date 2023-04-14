// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * Class for issuing intents to the android framework.
 */
@JNINamespace("content")
public class MDocProviderAndroid {
    private static final String TAG = "WebIdentityMDoc";

    private static final String MDOC_CREDENTIAL_PROVIDER_SERVICE_ACTION =
            "org.chromium.chrome.MDocCredentialProviderService";
    private final long mMDocProviderAndroid;
    private WindowAndroid mWindow;

    static final String READER_PUBLIC_KEY = "ReaderPublicKey";
    static final String DOCUMENT_TYPE = "DocumentType";
    static final String REQUESTED_ELEMENTS_NAMESPACE = "RequestedElementsNamespace";
    static final String REQUESTED_ELEMENTS_NAME = "RequestedElementsName";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String MDOC_RESPONSE_KEY = "MDoc";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public MDocProviderAndroid(long mDocProviderAndroid) {
        mMDocProviderAndroid = mDocProviderAndroid;
    }

    // Methods that are called by native implementation
    @CalledByNative
    private static MDocProviderAndroid create(long mDocProviderAndroid) {
        return new MDocProviderAndroid(mDocProviderAndroid);
    }

    @CalledByNative
    private void destroy() {}

    /**
     * Triggers a mdoc request intent.  If no application has registered to receive these intents,
     * this will fail silently.
     * @param window The window associated with the request.
     * @param readerPublicKey The base64-encoded reader HPKE public key.
     * @param documentType The document type.
     * @param requestedElementsNamespace The namespace of the requested element.
     * @param requestedElementsName The name of the requested element.
     */
    @CalledByNative
    void requestMDoc(WindowAndroid window, String readerPublicKey, String documentType,
            String requestedElementsNamespace, String requestedElementsName) {
        mWindow = window;
        Intent intent = new Intent(MDOC_CREDENTIAL_PROVIDER_SERVICE_ACTION);
        intent.putExtra(READER_PUBLIC_KEY, readerPublicKey);
        intent.putExtra(DOCUMENT_TYPE, documentType);
        intent.putExtra(REQUESTED_ELEMENTS_NAMESPACE, requestedElementsNamespace);
        intent.putExtra(REQUESTED_ELEMENTS_NAME, requestedElementsName);

        WindowAndroid.IntentCallback callback = new WindowAndroid.IntentCallback() {
            @Override
            public void onIntentCompleted(int resultCode, Intent data) {
                if (resultCode == Activity.RESULT_OK) {
                    String mdoc = IntentUtils.safeGetStringExtra(data, MDOC_RESPONSE_KEY);
                    MDocProviderAndroidJni.get().onReceive(mMDocProviderAndroid, mdoc);
                } else {
                    MDocProviderAndroidJni.get().onError(mMDocProviderAndroid);
                }
            }
        };

        if (!mWindow.showIntent(intent, callback, null)) {
            MDocProviderAndroidJni.get().onError(mMDocProviderAndroid);
        }
    }

    @NativeMethods
    interface Natives {
        void onReceive(long nativeMDocProviderAndroid, String mdoc);
        void onError(long nativeMDocProviderAndroid);
    }
}
