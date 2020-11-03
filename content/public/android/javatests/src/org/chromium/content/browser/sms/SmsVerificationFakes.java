// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.os.Bundle;

import com.google.android.gms.auth.api.phone.SmsCodeRetriever;
import com.google.android.gms.auth.api.phone.SmsRetrieverStatusCodes;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.base.WindowAndroid;

@JNINamespace("content")
@JNIAdditionalImport(Wrappers.class)
class SmsVerificationFakes {
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
            super(null);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerSmsVerificationSms(String sms) {
            Intent intent = new Intent(SmsCodeRetriever.SMS_CODE_RETRIEVED_ACTION);
            Bundle bundle = new Bundle();
            bundle.putParcelable(
                    SmsCodeRetriever.EXTRA_STATUS, new Status(CommonStatusCodes.SUCCESS));
            bundle.putString(SmsCodeRetriever.EXTRA_SMS_CODE_LINE, sms);
            intent.putExtras(bundle);

            deliverIntent(intent);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerTimeout() {
            Intent intent = new Intent(SmsCodeRetriever.SMS_CODE_RETRIEVED_ACTION);
            Bundle bundle = new Bundle();
            bundle.putParcelable(
                    SmsCodeRetriever.EXTRA_STATUS, new Status(CommonStatusCodes.TIMEOUT));
            intent.putExtras(bundle);

            deliverIntent(intent);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerUserDeniesPermission(WindowAndroid window) {
            Wrappers.WebOTPServiceContext context = super.getContext();
            if (context == null) {
                Log.v(TAG,
                        "FakeSmsRetrieverClient.triggerUserDeniesPermission failed: "
                                + "no context was set");
                return;
            }

            SmsVerificationReceiver receiver =
                    (SmsVerificationReceiver) context.getRegisteredReceiver();
            receiver.onPermissionDone(window, Activity.RESULT_CANCELED);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerUserGrantsPermission(WindowAndroid window) {
            Wrappers.WebOTPServiceContext context = super.getContext();
            if (context == null) {
                Log.v(TAG,
                        "FakeSmsRetrieverClient.triggerUserGrantsPermission failed: "
                                + "no context was set");
                return;
            }

            SmsVerificationReceiver receiver =
                    (SmsVerificationReceiver) context.getRegisteredReceiver();
            receiver.onPermissionDone(window, Activity.RESULT_OK);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private void triggerFailure(String type) {
            Wrappers.WebOTPServiceContext context = super.getContext();
            if (context == null) {
                Log.v(TAG,
                        "FakeSmsRetrieverClient.triggerFailure failed:"
                                + "no context was set");
                return;
            }

            SmsVerificationReceiver receiver =
                    (SmsVerificationReceiver) context.getRegisteredReceiver();

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
                Log.v(TAG,
                        "FakeSmsRetrieverClient.triggerFailure failed:"
                                + "invalid failure type " + type);
                return;
            }

            ApiException e = new ApiException(new Status(code));

            receiver.onRetrieverTaskFailure(null, e);
        }

        private void deliverIntent(Intent intent) {
            Wrappers.WebOTPServiceContext context = super.getContext();
            if (context == null) {
                Log.v(TAG,
                        "FakeSmsRetrieverClient.deliverIntent failed: "
                                + "no context was set");
                return;
            }

            BroadcastReceiver receiver = context.getRegisteredReceiver();
            receiver.onReceive(context, intent);
        }

        // ---------------------------------------------------------------------
        // SmsRetrieverClient overrides:

        @Override
        public Task<Void> startSmsCodeBrowserRetriever() {
            return Tasks.forResult(null);
        }
    }

    /**
     * Sets SmsRetrieverClient to WebOTPService to allow faking SMSes from android
     * client.
     **/
    @CalledByNative
    private static void setClientForTesting(
            SmsVerificationReceiver receiver, Wrappers.SmsRetrieverClientWrapper client) {
        receiver.setClientForTesting(client);
    }
}
