// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.annotation.IntDef;

import com.google.android.gms.auth.api.phone.SmsCodeBrowserClient;
import com.google.android.gms.auth.api.phone.SmsCodeRetriever;
import com.google.android.gms.auth.api.phone.SmsRetriever;
import com.google.android.gms.auth.api.phone.SmsRetrieverStatusCodes;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.ResolvableApiException;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.tasks.Task;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content.browser.sms.Wrappers.WebOTPServiceContext;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Encapsulates logic to retrieve OTP code via SMS Browser Code API. See also:
 * https://developers.google.com/android/reference/com/google/android/gms/auth/api/phone/SmsCodeBrowserClient
 *
 * <p>TODO(majidvp): rename legacy Verification name to more appropriate name ( e.g., BrowserCode.
 */
public class SmsVerificationReceiver extends BroadcastReceiver {
    private static final String TAG = "SmsVerification";
    private static final boolean DEBUG = false;
    private final SmsProviderGms mProvider;
    private boolean mDestroyed;
    private Wrappers.WebOTPServiceContext mContext;

    @IntDef({
        BackendAvailability.AVAILABLE,
        BackendAvailability.API_NOT_CONNECTED,
        BackendAvailability.PLATFORM_NOT_SUPPORTED,
        BackendAvailability.API_NOT_AVAILABLE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BackendAvailability {
        int AVAILABLE = 0;
        int API_NOT_CONNECTED = 1;
        int PLATFORM_NOT_SUPPORTED = 2;
        int API_NOT_AVAILABLE = 3;
        int NUM_ENTRIES = 4;
    }

    public SmsVerificationReceiver(SmsProviderGms provider, WebOTPServiceContext context) {
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

        // The SEND_PERMISSION permission is not documented to held by the sender of this broadcast,
        // but it's coming from the same place the UserConsent (SmsRetriever.SMS_RETRIEVED_ACTION)
        // broadcast is coming from, so the sender will be holding this permission. This prevents
        // other apps from spoofing verification codes.
        ContextUtils.registerExportedBroadcastReceiver(
                mContext, this, filter, SmsRetriever.SEND_PERMISSION);
    }

    public SmsCodeBrowserClient createClient() {
        return SmsCodeRetriever.getBrowserClient(mContext);
    }

    public void destroy() {
        if (mDestroyed) return;
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

    public void onPermissionDone(int resultCode, boolean isLocalRequest) {
        if (resultCode == Activity.RESULT_OK) {
            // We have been granted permission to use the SmsCoderetriever so restart the process.
            // |listen| will record the backend availability so no need to do it here.
            if (DEBUG) Log.d(TAG, "The one-time permission was granted");
            listen(isLocalRequest);
        } else {
            cancelRequestAndReportBackendAvailability();
            if (DEBUG) Log.d(TAG, "The one-time permission was rejected");
        }
    }

    /*
     * Handles failure for the `SmsCodeBrowserClient.startSmsCodeRetriever()`
     * task.
     */
    public void onRetrieverTaskFailure(boolean isLocalRequest, Exception e) {
        if (DEBUG) Log.d(TAG, "Task failed. Attempting recovery.", e);
        ApiException exception = (ApiException) e;
        if (exception.getStatusCode() == SmsRetrieverStatusCodes.API_NOT_CONNECTED) {
            reportBackendAvailability(BackendAvailability.API_NOT_CONNECTED);
            mProvider.onMethodNotAvailable(isLocalRequest);
            Log.d(TAG, "update GMS services.");
        } else if (exception.getStatusCode() == SmsRetrieverStatusCodes.PLATFORM_NOT_SUPPORTED) {
            reportBackendAvailability(BackendAvailability.PLATFORM_NOT_SUPPORTED);
            mProvider.onMethodNotAvailable(isLocalRequest);
            Log.d(TAG, "old android platform.");
        } else if (exception.getStatusCode() == SmsRetrieverStatusCodes.API_NOT_AVAILABLE) {
            reportBackendAvailability(BackendAvailability.API_NOT_AVAILABLE);
            mProvider.onMethodNotAvailable(isLocalRequest);
            Log.d(TAG, "not the default browser.");
        } else if (exception.getStatusCode() == SmsRetrieverStatusCodes.USER_PERMISSION_REQUIRED) {
            cancelRequestAndReportBackendAvailability();
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
                    mProvider
                            .getWindow()
                            .showIntent(
                                    resolutionIntent,
                                    new WindowAndroid.IntentCallback() {
                                        @Override
                                        public void onIntentCompleted(int resultCode, Intent data) {
                                            // Backend availability will be recorded inside
                                            // |onPermissionDone|.
                                            onPermissionDone(resultCode, isLocalRequest);
                                        }
                                    },
                                    null);
                } catch (Exception ex) {
                    cancelRequestAndReportBackendAvailability();
                    Log.e(TAG, "Cannot launch user permission", ex);
                }
            }
        } else {
            Log.w(TAG, "Unexpected exception", e);
        }
    }

    public void listen(boolean isLocalRequest) {
        Wrappers.SmsRetrieverClientWrapper client = mProvider.getClient();
        Task<Void> task = client.startSmsCodeBrowserRetriever();

        task.addOnSuccessListener(
                unused -> {
                    this.reportBackendAvailability(BackendAvailability.AVAILABLE);
                    mProvider.verificationReceiverSucceeded(isLocalRequest);
                });
        task.addOnFailureListener(
                (Exception e) -> {
                    this.onRetrieverTaskFailure(isLocalRequest, e);
                    mProvider.verificationReceiverFailed(isLocalRequest);
                });

        if (DEBUG) Log.d(TAG, "Installed task");
    }

    public void reportBackendAvailability(@BackendAvailability int availability) {
        if (DEBUG) Log.d(TAG, "Backend availability: %d", availability);
        RecordHistogram.recordEnumeratedHistogram(
                "Blink.Sms.BackendAvailability", availability, BackendAvailability.NUM_ENTRIES);
    }

    // Handles the case when the backend is available but user has previously denied to grant the
    // permission or we cannot launch user permission.
    private void cancelRequestAndReportBackendAvailability() {
        reportBackendAvailability(BackendAvailability.AVAILABLE);
        mProvider.onCancel();
    }
}
