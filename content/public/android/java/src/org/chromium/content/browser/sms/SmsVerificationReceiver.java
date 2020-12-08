// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import com.google.android.gms.auth.api.phone.SmsCodeBrowserClient;
import com.google.android.gms.auth.api.phone.SmsCodeRetriever;
import com.google.android.gms.auth.api.phone.SmsRetriever;
import com.google.android.gms.auth.api.phone.SmsRetrieverStatusCodes;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.ResolvableApiException;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.tasks.Task;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.base.WindowAndroid;

/**
 * Encapsulates logic to retrieve OTP code via SMS Browser Code API.
 * See also:
 * https://developers.google.com/android/reference/com/google/android/gms/auth/api/phone/SmsCodeBrowserClient
 *
 * TODO(majidvp): rename legacy Verification name to more appropriate name (
 *  e.g., BrowserCode.
 */
public class SmsVerificationReceiver extends BroadcastReceiver {
    private static final int CODE_PERMISSION_REQUEST = 1;
    private static final String TAG = "SmsVerification";
    private static final boolean DEBUG = false;
    private final SmsProviderGms mProvider;
    private boolean mDestroyed;
    private Wrappers.WebOTPServiceContext mContext;
    private enum BackendAvailability {
        AVAILABLE,
        API_NOT_CONNECTED,
        PLATFORM_NOT_SUPPORTED,
        API_NOT_AVAILABLE,
        NUM_ENTRIES
    }

    public SmsVerificationReceiver(SmsProviderGms provider, Wrappers.WebOTPServiceContext context) {
        if (DEBUG) Log.d(TAG, "Creating SmsVerificationReceiver.");

        mDestroyed = false;
        mProvider = provider;
        mContext = context;

        // A broadcast receiver is registered upon the creation of this class which happens when the
        // SMS Retriever API or SMS Browser Code API is used for the first time since chrome last
        // restarted (which, on android, happens frequently). The broadcast receiver is fairly
        // lightweight (e.g. it responds quickly without much computation). If this broadcast
        // receiver becomes more heavyweight, we should make this registration expire after the SMS
        // message is received.
        if (DEBUG) Log.i(TAG, "Registering intent filters.");
        IntentFilter filter = new IntentFilter();
        filter.addAction(SmsCodeRetriever.SMS_CODE_RETRIEVED_ACTION);

        mContext.registerReceiver(this, filter);
    }

    public SmsCodeBrowserClient createClient() {
        return SmsCodeRetriever.getBrowserClient(mContext);
    }

    public void destroy() {
        if (DEBUG) Log.d(TAG, "Destroying SmsVerificationReceiver.");
        mDestroyed = true;
        mContext.unregisterReceiver(this);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (DEBUG) Log.d(TAG, "Received something!");

        if (mDestroyed) {
            return;
        }

        if (!SmsCodeRetriever.SMS_CODE_RETRIEVED_ACTION.equals(intent.getAction())) {
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
                String message = intent.getExtras().getString(SmsCodeRetriever.EXTRA_SMS_CODE_LINE);
                if (DEBUG) Log.d(TAG, "Got message: %s!", message);
                mProvider.onReceive(message, GmsBackend.VERIFICATION);
                break;
            case CommonStatusCodes.TIMEOUT:
                if (DEBUG) Log.d(TAG, "Timeout");
                mProvider.onTimeout();
                break;
        }
    }

    public void onPermissionDone(WindowAndroid window, int resultCode) {
        if (resultCode == Activity.RESULT_OK) {
            // We have been granted permission to use the SmsCoderetriever so
            // restart the process.
            if (DEBUG) Log.d(TAG, "The one-time permission was granted");
            listen(window);
        } else {
            mProvider.onCancel();
            if (DEBUG) Log.d(TAG, "The one-time permission was rejected");
        }
    }

    /*
     * Handles failure for the `SmsCodeBrowserClient.startSmsCodeRetriever()`
     * task.
     */
    public void onRetrieverTaskFailure(WindowAndroid window, Exception e) {
        if (DEBUG) Log.d(TAG, "Task failed. Attempting recovery.", e);
        BackendAvailability availability = BackendAvailability.AVAILABLE;
        ApiException exception = (ApiException) e;
        if (exception.getStatusCode() == SmsRetrieverStatusCodes.API_NOT_CONNECTED) {
            availability = BackendAvailability.API_NOT_CONNECTED;
            mProvider.onMethodNotAvailable();
            Log.d(TAG, "update GMS services.");
        } else if (exception.getStatusCode() == SmsRetrieverStatusCodes.PLATFORM_NOT_SUPPORTED) {
            availability = BackendAvailability.PLATFORM_NOT_SUPPORTED;
            mProvider.onMethodNotAvailable();
            Log.d(TAG, "old android platform.");
        } else if (exception.getStatusCode() == SmsRetrieverStatusCodes.API_NOT_AVAILABLE) {
            availability = BackendAvailability.API_NOT_AVAILABLE;
            mProvider.onMethodNotAvailable();
            Log.d(TAG, "not the default browser.");
        } else if (exception.getStatusCode() == SmsRetrieverStatusCodes.USER_PERMISSION_REQUIRED) {
            mProvider.onCancel();
            Log.d(TAG, "user permission is required.");
        } else if (exception.getStatusCode() == CommonStatusCodes.RESOLUTION_REQUIRED) {
            if (exception instanceof ResolvableApiException) {
                // This occurs if the default browser is in NONE permission
                // state. Resolve it by calling PendingIntent.send() method.
                // This shows the consent dialog to user so they grant
                // one-time permission. The dialog result will be received
                // via `onPermissionDone()`
                ResolvableApiException rex = (ResolvableApiException) exception;
                try {
                    PendingIntent resolutionIntent = rex.getResolution();
                    window.showIntent(resolutionIntent, new WindowAndroid.IntentCallback() {
                        @Override
                        public void onIntentCompleted(
                                WindowAndroid w, int resultCode, Intent data) {
                            onPermissionDone(w, resultCode);
                        }
                    }, null);
                } catch (Exception ex) {
                    Log.e(TAG, "Cannot launch user permission", ex);
                }
            }
        } else {
            Log.w(TAG, "Unexpected exception", e);
        }
        reportBackendAvailability(availability);
    }

    public void listen(WindowAndroid window) {
        Wrappers.SmsRetrieverClientWrapper client = mProvider.getClient();
        Task<Void> task = client.startSmsCodeBrowserRetriever();

        task.addOnSuccessListener(
                unused -> { this.reportBackendAvailability(BackendAvailability.AVAILABLE); });
        task.addOnFailureListener((Exception e) -> { this.onRetrieverTaskFailure(window, e); });

        if (DEBUG) Log.d(TAG, "Installed task");
    }

    public void reportBackendAvailability(BackendAvailability availability) {
        if (DEBUG) Log.d(TAG, "Backend availability: %d", availability.ordinal());
        RecordHistogram.recordEnumeratedHistogram("Blink.Sms.BackendAvailability",
                availability.ordinal(), BackendAvailability.NUM_ENTRIES.ordinal());
    }
}
