// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.os.Bundle;

import com.google.android.gms.auth.api.phone.SmsRetriever;
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
class SmsUserConsentFakes {
    private static final String TAG = "WebOTPService";

    /**
     * Fakes com.google.android.gms.auth.api.phone.SmsRetrieverClient.
     **/
    static class FakeSmsUserConsentRetrieverClient extends Wrappers.SmsRetrieverClientWrapper {
        @CalledByNative("FakeSmsUserConsentRetrieverClient")
        private static FakeSmsUserConsentRetrieverClient create() {
            Log.v(TAG, "FakeSmsUserConsentRetrieverClient.create");
            return new FakeSmsUserConsentRetrieverClient();
        }

        private FakeSmsUserConsentRetrieverClient() {
            super(null);
        }

        @CalledByNative("FakeSmsUserConsentRetrieverClient")
        private Task<Void> triggerUserConsentSms(String sms) {
            Wrappers.WebOTPServiceContext context = super.getContext();
            if (context == null) {
                Log.v(TAG,
                        "FakeSmsUserConsentRetrieverClient.triggerUserConsentSms failed: "
                                + "no context was set");
                return Tasks.forResult(null);
            }

            Intent intent = new Intent(SmsRetriever.SMS_RETRIEVED_ACTION);
            Bundle bundle = new Bundle();
            bundle.putParcelable(SmsRetriever.EXTRA_STATUS, new Status(CommonStatusCodes.SUCCESS));
            bundle.putString(SmsRetriever.EXTRA_SMS_MESSAGE, sms);
            intent.putExtras(bundle);

            BroadcastReceiver receiver = context.getRegisteredReceiver();
            try {
                ((SmsUserConsentReceiver) receiver).onConsentResult(Activity.RESULT_OK, intent);
            } catch (ClassCastException e) {
                Log.v(TAG,
                        "FakeSmsUserConsentRetrieverClient.triggerUserConsentSms failed: "
                                + "receiver must be an instance of SmsUserConsentReceiver");
            }
            return Tasks.forResult(null);
        }

        @CalledByNative("FakeSmsUserConsentRetrieverClient")
        private Task<Void> triggerTimeout() {
            Wrappers.WebOTPServiceContext context = super.getContext();
            if (context == null) {
                Log.v(TAG,
                        "FakeSmsUserConsentRetrieverClient.triggerTimeout failed: "
                                + "no context was set");
                return Tasks.forResult(null);
            }

            Intent intent = new Intent(SmsRetriever.SMS_RETRIEVED_ACTION);
            Bundle bundle = new Bundle();
            bundle.putParcelable(SmsRetriever.EXTRA_STATUS, new Status(CommonStatusCodes.TIMEOUT));
            intent.putExtras(bundle);

            BroadcastReceiver receiver = context.getRegisteredReceiver();
            receiver.onReceive(context, intent);
            return Tasks.forResult(null);
        }

        // ---------------------------------------------------------------------
        // SmsRetrieverClient overrides:

        @Override
        public Task<Void> startSmsRetriever() {
            return Tasks.forResult(null);
        }
    }

    /**
     * Sets SmsRetrieverClient to SmsUserConsentReceiver to allow faking user
     * consented SMSes from android client.
     **/
    @CalledByNative
    private static void setUserConsentClientForTesting(SmsUserConsentReceiver receiver,
            Wrappers.SmsRetrieverClientWrapper client, WindowAndroid windowAndroid) {
        receiver.setClientForTesting(client, windowAndroid);
    }
}
