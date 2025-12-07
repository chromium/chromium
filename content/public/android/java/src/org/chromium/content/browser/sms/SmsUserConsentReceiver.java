// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import com.google.android.gms.auth.api.phone.SmsRetriever;
import com.google.android.gms.auth.api.phone.SmsRetrieverClient;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.Task;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** Encapsulates logic to retrieve OTP code via SMS User Consent API. */
@NullMarked
public class SmsUserConsentReceiver extends BroadcastReceiver {
    private static final String TAG = "SmsUserConsentRcvr";
    private static final boolean DEBUG = false;
    private final SmsProviderGms mProvider;
    private boolean mDestroyed;
    private final Wrappers.WebOTPServiceContext mContext;

    public SmsUserConsentReceiver(SmsProviderGms provider, Wrappers.WebOTPServiceContext context) {
        mDestroyed = false;
        mProvider = provider;
        mContext = context;

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
        ContextUtils.registerExportedBroadcastReceiver(
                mContext, this, filter, SmsRetriever.SEND_PERMISSION);
    }

    public SmsRetrieverClient createClient() {
        return SmsRetriever.getClient(mContext);
    }

    public void destroy() {
        if (mDestroyed) return;
        if (DEBUG) Log.d(TAG, "Destroying SmsUserConsentReceiver.");
        mDestroyed = true;
        mContext.unregisterReceiver(this);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
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

        final Status status = IntentUtils.safeGetParcelableExtra(intent, SmsRetriever.EXTRA_STATUS);
        if (status == null) {
            return;
        }

        switch (status.getStatusCode()) {
            case CommonStatusCodes.SUCCESS:
                assert mProvider.getWindow() != null;

                Intent consentIntent =
                        intent.getExtras().getParcelable(SmsRetriever.EXTRA_CONSENT_INTENT);
                try {
                    mProvider.getWindow().showIntent(consentIntent, this::onConsentResult, null);
                } catch (android.content.ActivityNotFoundException e) {
                    if (DEBUG) Log.d(TAG, "Error starting activity for result.");
                }
                break;
            case CommonStatusCodes.TIMEOUT:
                if (DEBUG) Log.d(TAG, "Timeout");
                mProvider.onTimeout();
                break;
        }
    }

    void onConsentResult(int resultCode, @Nullable Intent data) {
        if (resultCode == Activity.RESULT_OK) {
            assumeNonNull(data);
            String message = data.getStringExtra(SmsRetriever.EXTRA_SMS_MESSAGE);
            mProvider.onReceive(message, GmsBackend.USER_CONSENT);
        } else if (resultCode == Activity.RESULT_CANCELED) {
            if (DEBUG) Log.d(TAG, "Activity result cancelled.");
            mProvider.onCancel();
        }
    }

    public void listen(WindowAndroid windowAndroid) {
        Task<Void> task = mProvider.getClient().startSmsUserConsent(null);

        task.addOnFailureListener(
                new OnFailureListener() {
                    @Override
                    public void onFailure(Exception e) {
                        Log.e(TAG, "Task failed to start", e);
                    }
                });
        if (DEBUG) Log.d(TAG, "Installed task");
    }
}
