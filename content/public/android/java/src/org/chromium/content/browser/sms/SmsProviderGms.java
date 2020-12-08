// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * Simple proxy that provides C++ code with a pathway to the GMS OTP code
 * retrieving APIs.
 * It can operate in three different backend modes which is determined on
 * construction:
 *  1. User Consent: use the user consent API (older method with sub-optimal UX)
 *  2. Verification: use the browser code API (newer method)
 *  3. Auto: prefers to use the verification method but if it is not available
 *           automatically falls back to using the User Consent method.
 *
 */
@JNINamespace("content")
@JNIAdditionalImport(Wrappers.class)
public class SmsProviderGms {
    private static final String TAG = "SmsProviderGms";
    private final long mSmsProviderGmsAndroid;

    private final @GmsBackend int mBackend;
    private SmsUserConsentReceiver mUserConsentReceiver;
    private SmsVerificationReceiver mVerificationReceiver;

    private Wrappers.WebOTPServiceContext mContext;
    private WindowAndroid mWindow;
    private Wrappers.SmsRetrieverClientWrapper mClient;

    private SmsProviderGms(long smsProviderGmsAndroid, @GmsBackend int backend) {
        mSmsProviderGmsAndroid = smsProviderGmsAndroid;
        mBackend = backend;

        mContext = new Wrappers.WebOTPServiceContext(ContextUtils.getApplicationContext());

        // For AUTO, create both receivers since we may need to fallback to using the user consent
        // backend when verification backend is not available.
        if (mBackend == GmsBackend.AUTO || mBackend == GmsBackend.VERIFICATION) {
            mVerificationReceiver = new SmsVerificationReceiver(this, mContext);
        }

        if (mBackend == GmsBackend.AUTO || mBackend == GmsBackend.USER_CONSENT) {
            mUserConsentReceiver = new SmsUserConsentReceiver(this, mContext);
        }

        assert (mVerificationReceiver != null || mUserConsentReceiver != null);
        Log.i(TAG, "construction successfull %s, %s", mVerificationReceiver, mUserConsentReceiver);
    }

    // Methods that are called by native implementation
    @CalledByNative
    private static SmsProviderGms create(long smsProviderGmsAndroid, @GmsBackend int backend) {
        Log.d(TAG, "Creating SmsProviderGms");
        return new SmsProviderGms(smsProviderGmsAndroid, backend);
    }

    @CalledByNative
    private void destroy() {
        if (mVerificationReceiver != null) mVerificationReceiver.destroy();
        if (mUserConsentReceiver != null) mUserConsentReceiver.destroy();
    }

    @CalledByNative
    private void listen(WindowAndroid window) {
        mWindow = window;
        switch (mBackend) {
            case GmsBackend.AUTO:
            case GmsBackend.VERIFICATION:
                mVerificationReceiver.listen(window);
                break;
            case GmsBackend.USER_CONSENT:
                mUserConsentReceiver.listen(window);
                break;
        }
    }

    void onMethodNotAvailable() {
        assert (mBackend == GmsBackend.AUTO || mBackend == GmsBackend.VERIFICATION);

        // Note on caching method availability status: It is possible to cache the fact that calling
        // into verification backend has failed and avoid trying it again on subsequent calls. But
        // since that can change at runtime (e.g., if Chrome becomes default browser) then we may
        // need to invalidate that cached status. To simplify things we simply attempt the
        // verification method first on *each request* and fallback to the user consent method if it
        // fails. The initial call to verification is expected to be cheap so this should not have
        // any noticeable impact.

        if (mBackend == GmsBackend.AUTO) {
            Log.d(TAG, "Retry using user consent API.");
            mUserConsentReceiver.listen(mWindow);
        } else {
            onNotAvailable();
        }
    }

    // --------- Callbacks for receivers

    void onReceive(String sms, @GmsBackend int backend) {
        SmsProviderGmsJni.get().onReceive(mSmsProviderGmsAndroid, sms, backend);
    }
    void onTimeout() {
        SmsProviderGmsJni.get().onTimeout(mSmsProviderGmsAndroid);
    }
    void onCancel() {
        SmsProviderGmsJni.get().onCancel(mSmsProviderGmsAndroid);
    }
    void onNotAvailable() {
        SmsProviderGmsJni.get().onNotAvailable(mSmsProviderGmsAndroid);
    }

    public WindowAndroid getWindow() {
        return mWindow;
    }

    public Wrappers.SmsRetrieverClientWrapper getClient() {
        if (mClient != null) {
            return mClient;
        }
        mClient = new Wrappers.SmsRetrieverClientWrapper(
                mUserConsentReceiver != null ? mUserConsentReceiver.createClient() : null,
                mVerificationReceiver != null ? mVerificationReceiver.createClient() : null);

        return mClient;
    }

    @CalledByNative
    private void setClientAndWindow(
            Wrappers.SmsRetrieverClientWrapper client, WindowAndroid window) {
        assert mClient == null;
        assert mWindow == null;
        mClient = client;
        mWindow = window;

        mClient.setContext(mContext);
    }

    @NativeMethods
    interface Natives {
        void onReceive(long nativeSmsProviderGms, String sms, @GmsBackend int backend);
        void onTimeout(long nativeSmsProviderGms);
        void onCancel(long nativeSmsProviderGms);
        void onNotAvailable(long nativeSmsProviderGms);
    }
}