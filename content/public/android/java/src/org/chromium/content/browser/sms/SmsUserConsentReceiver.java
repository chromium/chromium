// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.auth.api.phone.SmsRetriever;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.Task;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * Simple proxy that provides C++ code with a pathway to the Android SMS
 * Retriever to access the SMS User Consent API.
 */
@JNINamespace("content")
@JNIAdditionalImport(Wrappers.class)
public class SmsUserConsentReceiver extends BroadcastReceiver {
    private static final String TAG = "SmsUserConsentRcvr";
    private static final boolean DEBUG = false;
    private final long mSmsProviderAndroid;
    private boolean mDestroyed;
    private Wrappers.SmsRetrieverClientWrapper mClient;
    private Wrappers.WebOTPServiceContext mContext;
    private WindowAndroid mWindowAndroid;

    private SmsUserConsentReceiver(long smsProviderAndroid) {
        mDestroyed = false;
        mSmsProviderAndroid = smsProviderAndroid;

        mContext = new Wrappers.WebOTPServiceContext(ContextUtils.getApplicationContext());

        // A broadcast receiver is registered upon the creation of this class
        // which happens when the SMS Retriever API is used for the first time
        // since chrome last restarted (which, on android, happens frequently).
        // The broadcast receiver is fairly lightweight (e.g. it responds
        // quickly without much computation).
        // If this broadcast receiver becomes more heavyweight, we should make
        // this registration expire after the SMS message is received.
        if (DEBUG) Log.d(TAG, "Registering intent filters.");
        IntentFilter filter = new IntentFilter();
        filter.addAction(SmsRetriever.SMS_RETRIEVED_ACTION);
        mContext.registerReceiver(this, filter);
    }

    @CalledByNative
    private static SmsUserConsentReceiver create(long smsProviderAndroid) {
        if (DEBUG) Log.d(TAG, "Creating SmsUserConsentReceiver.");
        return new SmsUserConsentReceiver(smsProviderAndroid);
    }

    @CalledByNative
    private void destroy() {
        if (DEBUG) Log.d(TAG, "Destroying SmsUserConsentReceiver.");
        mDestroyed = true;
        mContext.unregisterReceiver(this);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        assert mWindowAndroid != null;

        if (DEBUG) Log.d(TAG, "Received something!");

        if (mDestroyed) {
            return;
        }

        if (!SmsRetriever.SMS_RETRIEVED_ACTION.equals(intent.getAction())) {
            return;
        }

        if (intent.getExtras() == null) {
            return;
        }

        final Status status;

        try {
            status = (Status) intent.getParcelableExtra(SmsRetriever.EXTRA_STATUS);
        } catch (Throwable e) {
            if (DEBUG) Log.d(TAG, "Error getting parceable.");
            return;
        }

        switch (status.getStatusCode()) {
            case CommonStatusCodes.SUCCESS:
                Intent consentIntent =
                        intent.getExtras().getParcelable(SmsRetriever.EXTRA_CONSENT_INTENT);
                try {
                    mWindowAndroid.showIntent(consentIntent,
                            (window, resultCode, data) -> onConsentResult(resultCode, data), null);
                } catch (android.content.ActivityNotFoundException e) {
                    if (DEBUG) Log.d(TAG, "Error starting activity for result.");
                }
                break;
            case CommonStatusCodes.TIMEOUT:
                if (DEBUG) Log.d(TAG, "Timeout");
                SmsUserConsentReceiverJni.get().onTimeout(mSmsProviderAndroid);
                break;
        }
    }

    void onConsentResult(int resultCode, Intent data) {
        if (resultCode == Activity.RESULT_OK) {
            String message = data.getStringExtra(SmsRetriever.EXTRA_SMS_MESSAGE);
            SmsUserConsentReceiverJni.get().onReceive(mSmsProviderAndroid, message);
        } else if (resultCode == Activity.RESULT_CANCELED) {
            if (DEBUG) Log.d(TAG, "Activity result cancelled.");
            SmsUserConsentReceiverJni.get().onCancel(mSmsProviderAndroid);
        }
    }

    @CalledByNative
    private void listen(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        Task<Void> task = getClient().startSmsUserConsent(null);

        task.addOnFailureListener(new OnFailureListener() {
            @Override
            public void onFailure(Exception e) {
                Log.e(TAG, "Task failed to start", e);
            }
        });
        if (DEBUG) Log.d(TAG, "Installed task");
    }

    private Wrappers.SmsRetrieverClientWrapper getClient() {
        if (mClient != null) return mClient;
        mClient = new Wrappers.SmsRetrieverClientWrapper(SmsRetriever.getClient(mContext));
        return mClient;
    }

    @VisibleForTesting
    public void setClientForTesting(
            Wrappers.SmsRetrieverClientWrapper client, WindowAndroid windowAndroid) {
        assert mClient == null;
        assert mWindowAndroid == null;
        mWindowAndroid = windowAndroid;
        mClient = client;
        mClient.setContext(mContext);
    }

    @NativeMethods
    interface Natives {
        void onReceive(long nativeSmsProviderGmsUserConsent, String sms);
        void onTimeout(long nativeSmsProviderGmsUserConsent);
        void onCancel(long nativeSmsProviderGmsUserConsent);
    }
}
