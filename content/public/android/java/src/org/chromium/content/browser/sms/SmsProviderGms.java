// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
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
public class SmsProviderGms {
    private static final String TAG = "SmsProviderGms";
    private static final int MIN_GMS_VERSION_NUMBER_WITH_CODE_BROWSER_BACKEND = 202990000;
    private final long mSmsProviderGmsAndroid;

    private final @GmsBackend int mBackend;
    private SmsUserConsentReceiver mUserConsentReceiver;
    private SmsVerificationReceiver mVerificationReceiver;

    private Wrappers.WebOTPServiceContext mContext;
    private WindowAndroid mWindow;
    private Wrappers.SmsRetrieverClientWrapper mClient;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public SmsProviderGms(
            long smsProviderGmsAndroid,
            @GmsBackend int backend,
            boolean isVerificationBackendAvailable) {
        mSmsProviderGmsAndroid = smsProviderGmsAndroid;
        mBackend = backend;
        mContext = new Wrappers.WebOTPServiceContext(ContextUtils.getApplicationContext(), this);

        // Creates an mVerificationReceiver regardless of the backend to support requests from
        // remote devices.
        if (isVerificationBackendAvailable) {
            mVerificationReceiver = new SmsVerificationReceiver(this, mContext);
        }

        if (mBackend == GmsBackend.AUTO || mBackend == GmsBackend.USER_CONSENT) {
            mUserConsentReceiver = new SmsUserConsentReceiver(this, mContext);
        }

        Log.i(TAG, "construction successfull %s, %s", mVerificationReceiver, mUserConsentReceiver);
    }

    public void setUserConsentReceiverForTesting(SmsUserConsentReceiver userConsentReceiver) {
        var oldValue = mUserConsentReceiver;
        mUserConsentReceiver = userConsentReceiver;
        ResettersForTesting.register(() -> mUserConsentReceiver = oldValue);
    }

    public void setVerificationReceiverForTesting(SmsVerificationReceiver verificationReceiver) {
        var oldValue = mVerificationReceiver;
        mVerificationReceiver = verificationReceiver;
        ResettersForTesting.register(() -> mVerificationReceiver = oldValue);
    }

    public SmsUserConsentReceiver getUserConsentReceiverForTesting() {
        return mUserConsentReceiver;
    }

    public SmsVerificationReceiver getVerificationReceiverForTesting() {
        return mVerificationReceiver;
    }

    // Methods that are called by native implementation
    @CalledByNative
    private static SmsProviderGms create(long smsProviderGmsAndroid, @GmsBackend int backend) {
        Log.d(TAG, "Creating SmsProviderGms");
        boolean isVerificationBackendAvailable =
                GoogleApiAvailability.getInstance()
                                .isGooglePlayServicesAvailable(
                                        ContextUtils.getApplicationContext(),
                                        MIN_GMS_VERSION_NUMBER_WITH_CODE_BROWSER_BACKEND)
                        == ConnectionResult.SUCCESS;
        return new SmsProviderGms(smsProviderGmsAndroid, backend, isVerificationBackendAvailable);
    }

    @CalledByNative
    private void destroy() {
        if (mVerificationReceiver != null) mVerificationReceiver.destroy();
        if (mUserConsentReceiver != null) mUserConsentReceiver.destroy();
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void listen(WindowAndroid window, boolean isLocalRequest) {
        mWindow = window;

        // Using the verification receiver is preferable but also start user consent receiver in
        // case the verification receiver fails. i.e. if the start of the verification retriever has
        // not been successful and the SMS arrives, we fall back to the user consent receiver to
        // handle it. Note that starting both receiver means that we may end up using the user
        // consent receiver even when the preferred verification backend is available but slow. But
        // this is acceptable given that handling SMS should be done in timely manner.
        // If the SMS retrieval request is made from a remote device, e.g. desktop, we only proceed
        // with the verification receiver because the user consent receiver introduces too much user
        // friction. In addition, we do not apply the fallback logic in such case.
        boolean shouldUseVerificationReceiver =
                mVerificationReceiver != null
                        && (!isLocalRequest || mBackend != GmsBackend.USER_CONSENT);
        boolean shouldUseUserConsentReceiver =
                mUserConsentReceiver != null
                        && isLocalRequest
                        && mBackend != GmsBackend.VERIFICATION
                        && window != null;
        if (shouldUseVerificationReceiver) mVerificationReceiver.listen(isLocalRequest);
        if (shouldUseUserConsentReceiver) mUserConsentReceiver.listen(window);
    }

    /**
     * Destroys the user consent receiver if the verification receiver succeeded with a local
     * request.
     *
     * @param isLocalRequest Represents whether this request is from local device or not
     */
    public void verificationReceiverSucceeded(boolean isLocalRequest) {
        if (!isLocalRequest) return;
        Log.d(TAG, "DestroyUserConsentReceiver");
        if (mUserConsentReceiver != null) mUserConsentReceiver.destroy();
    }

    /**
     * Destroys the verification receiver if it failed with a local request.
     *
     * @param isLocalRequest Represents whether this request is from local device or not
     */
    public void verificationReceiverFailed(boolean isLocalRequest) {
        if (!isLocalRequest) return;
        Log.d(TAG, "DestroyVerificationReceiver");
        if (mVerificationReceiver != null) mVerificationReceiver.destroy();
    }

    void onMethodNotAvailable(boolean isLocalRequest) {
        assert (mBackend != GmsBackend.USER_CONSENT || !isLocalRequest);

        // Note on caching method availability status: It is possible to cache the fact that calling
        // into verification backend has failed and avoid trying it again on subsequent calls. But
        // since that can change at runtime (e.g., if Chrome becomes default browser) then we may
        // need to invalidate that cached status. To simplify things we simply attempt the
        // verification method first on *each request* and fallback to the user consent method if it
        // fails. The initial call to verification is expected to be cheap so this should not have
        // any noticeable impact.
        // Note that the fallback logic is only applicable if the SMS retrieval request is made from
        // a local device.
        if (mBackend == GmsBackend.VERIFICATION || !isLocalRequest) onNotAvailable();
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
        mClient =
                new Wrappers.SmsRetrieverClientWrapper(
                        mUserConsentReceiver != null ? mUserConsentReceiver.createClient() : null,
                        mVerificationReceiver != null
                                ? mVerificationReceiver.createClient()
                                : null);

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
