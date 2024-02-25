// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.os.Bundle;

import com.google.android.gms.auth.api.phone.SmsCodeRetriever;
import com.google.android.gms.auth.api.phone.SmsRetriever;
import com.google.android.gms.auth.api.phone.SmsRetrieverStatusCodes;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;

@JNINamespace("content")
class SmsProviderFakes {
    private static final String TAG = "WebOTPService";

    /**
     * Fakes com.google.android.gms.auth.api.phone.SmsRetrieverClient.
     **/
    static class FakeSmsRetrieverClient extends Wrappers.SmsRetrieverClientWrapper {
        @CalledByNative("FakeSmsRetrieverClient")
        private static FakeSmsRetrieverClient create() {
            Log.v(TAG, "FakeSmsRetrieverClient.create");
            return new FakeSmsRetrieverClient();
        }

        private FakeSmsRetrieverClient() {
            super(null, null);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerVerificationSms(String sms) {
            Intent intent = new Intent(SmsCodeRetriever.SMS_CODE_RETRIEVED_ACTION);
            Bundle bundle = new Bundle();
            bundle.putParcelable(
                    SmsCodeRetriever.EXTRA_STATUS, new Status(CommonStatusCodes.SUCCESS));
            bundle.putString(SmsCodeRetriever.EXTRA_SMS_CODE_LINE, sms);
            intent.putExtras(bundle);

            Wrappers.WebOTPServiceContext context = super.getContext();
            assert context != null;
            BroadcastReceiver receiver = context.createVerificationReceiverForTesting();
            receiver.onReceive(context, intent);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerUserConsentSms(String sms) {
            Intent intent = new Intent(SmsRetriever.SMS_RETRIEVED_ACTION);
            Bundle bundle = new Bundle();
            bundle.putParcelable(SmsRetriever.EXTRA_STATUS, new Status(CommonStatusCodes.SUCCESS));
            bundle.putString(SmsRetriever.EXTRA_SMS_MESSAGE, sms);
            intent.putExtras(bundle);

            Wrappers.WebOTPServiceContext context = super.getContext();
            assert context != null;

            SmsUserConsentReceiver receiver = context.getRegisteredUserConsentReceiver();
            try {
                receiver.onConsentResult(Activity.RESULT_OK, intent);
            } catch (ClassCastException e) {
                Log.v(
                        TAG,
                        "FakeSmsUserConsentRetrieverClient.triggerUserConsentSms failed: "
                                + "receiver must be an instance of SmsUserConsentReceiver");
            }
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerVerificationTimeout() {
            Wrappers.WebOTPServiceContext context = super.getContext();
            assert context != null;
            Intent intent = new Intent(SmsCodeRetriever.SMS_CODE_RETRIEVED_ACTION);
            Bundle bundle = new Bundle();
            bundle.putParcelable(
                    SmsCodeRetriever.EXTRA_STATUS, new Status(CommonStatusCodes.TIMEOUT));
            intent.putExtras(bundle);

            BroadcastReceiver receiver = context.createVerificationReceiverForTesting();
            receiver.onReceive(context, intent);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerUserConsentTimeout() {
            Wrappers.WebOTPServiceContext context = super.getContext();
            assert context != null;

            Intent intent = new Intent(SmsRetriever.SMS_RETRIEVED_ACTION);
            Bundle bundle = new Bundle();
            bundle.putParcelable(SmsRetriever.EXTRA_STATUS, new Status(CommonStatusCodes.TIMEOUT));
            intent.putExtras(bundle);

            BroadcastReceiver receiver = context.getRegisteredUserConsentReceiver();
            receiver.onReceive(context, intent);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerUserDeniesPermission(boolean isLocalRequest) {
            Wrappers.WebOTPServiceContext context = super.getContext();
            assert context != null;

            SmsVerificationReceiver receiver = context.createVerificationReceiverForTesting();
            receiver.onPermissionDone(Activity.RESULT_CANCELED, isLocalRequest);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerUserGrantsPermission(boolean isLocalRequest) {
            Wrappers.WebOTPServiceContext context = super.getContext();
            if (context == null) {
                Log.v(
                        TAG,
                        "FakeSmsRetrieverClient.triggerUserGrantsPermission failed: "
                                + "no context was set");
                return;
            }

            SmsVerificationReceiver receiver =
                    (SmsVerificationReceiver) context.createVerificationReceiverForTesting();
            receiver.onPermissionDone(Activity.RESULT_OK, isLocalRequest);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerFailure(String type, boolean isLocalRequest) {
            Wrappers.WebOTPServiceContext context = super.getContext();
            assert context != null;

            SmsVerificationReceiver receiver = context.createVerificationReceiverForTesting();
            Log.i(TAG, "receiver %s", receiver);

            int code;
            if (type.equals("API_NOT_CONNECTED")) {
                code = SmsRetrieverStatusCodes.API_NOT_CONNECTED;
            } else if (type.equals("PLATFORM_NOT_SUPPORTED")) {
                code = SmsRetrieverStatusCodes.PLATFORM_NOT_SUPPORTED;
            } else if (type.equals("API_NOT_AVAILABLE")) {
                code = SmsRetrieverStatusCodes.API_NOT_AVAILABLE;
            } else if (type.equals("USER_PERMISSION_REQUIRED")) {
                code = SmsRetrieverStatusCodes.USER_PERMISSION_REQUIRED;
            } else {
                Log.v(
                        TAG,
                        "FakeSmsRetrieverClient.triggerFailure failed:"
                                + "invalid failure type "
                                + type);
                return;
            }

            ApiException e = new ApiException(new Status(code));

            receiver.onRetrieverTaskFailure(isLocalRequest, e);
        }

        // ---------------------------------------------------------------------
        // SmsRetrieverClientWrapper overrides:

        @Override
        public Task<Void> startSmsCodeBrowserRetriever() {
            return Tasks.forResult(null);
        }

        @Override
        public Task<Void> startSmsUserConsent(String senderAddress) {
            return Tasks.forResult(null);
        }
    }
}
