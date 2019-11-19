// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.auth.api.phone.SmsRetriever;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.tasks.Task;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Simple proxy that provides C++ code with an access pathway to the Android
 * SMS retriever.
 */
@JNINamespace("content")
@JNIAdditionalImport(Wrappers.class)
public class SmsReceiver extends BroadcastReceiver {
    private static final String TAG = "SmsReceiver";
    private static final boolean DEBUG = false;
    private final long mSmsProviderAndroid;
    private boolean mDestroyed;
    private Wrappers.SmsRetrieverClientWrapper mClient;
    private Wrappers.SmsReceiverContext mContext;

    private SmsReceiver(long smsProviderAndroid) {
        mDestroyed = false;
        mSmsProviderAndroid = smsProviderAndroid;

        mContext = new Wrappers.SmsReceiverContext(ContextUtils.getApplicationContext());

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
    private static SmsReceiver create(long smsProviderAndroid) {
        if (DEBUG) Log.d(TAG, "Creating SmsReceiver.");
        return new SmsReceiver(smsProviderAndroid);
    }

    @CalledByNative
    private void destroy() {
        if (DEBUG) Log.d(TAG, "Destroying SmsReceiver.");
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

        final Status status;

        try {
            status = (Status) intent.getParcelableExtra(SmsRetriever.EXTRA_STATUS);
        } catch (Throwable e) {
            if (DEBUG) Log.d(TAG, "Error getting parceable");
            return;
        }

        switch (status.getStatusCode()) {
            case CommonStatusCodes.SUCCESS:
                String message = intent.getExtras().getString(SmsRetriever.EXTRA_SMS_MESSAGE);
                if (DEBUG) Log.d(TAG, "Got message: %s!", message);
                SmsReceiverJni.get().onReceive(mSmsProviderAndroid, message);
                break;
            case CommonStatusCodes.TIMEOUT:
                if (DEBUG) Log.d(TAG, "Timeout");
                SmsReceiverJni.get().onTimeout(mSmsProviderAndroid);
                break;
        }
    }

    @CalledByNative
    private void listen() {
        Wrappers.SmsRetrieverClientWrapper client = getClient();
        Task<Void> task = client.startSmsRetriever();

        if (DEBUG) Log.d(TAG, "Installed task");
    }

    private Wrappers.SmsRetrieverClientWrapper getClient() {
        if (mClient != null) {
            return mClient;
        }
        mClient = new Wrappers.SmsRetrieverClientWrapper(SmsRetriever.getClient(mContext));
        return mClient;
    }

    @VisibleForTesting
    public void setClientForTesting(Wrappers.SmsRetrieverClientWrapper client) {
        assert mClient == null;
        mClient = client;
        mClient.setContext(mContext);
    }

    @NativeMethods
    interface Natives {
        void onReceive(long nativeSmsProviderAndroid, String sms);
        void onTimeout(long nativeSmsProviderAndroid);
    }
}
