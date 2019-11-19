// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

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

@JNINamespace("content")
@JNIAdditionalImport(Wrappers.class)
class Fakes {
    private static final String TAG = "SmsReceiver";

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
        private Task<Void> triggerSms(String sms) {
            Wrappers.SmsReceiverContext context = super.getContext();
            if (context == null) {
                Log.v(TAG, "FakeSmsRetrieverClient.triggerSms failed: no context was set");
                return Tasks.forResult(null);
            }

            Intent intent = new Intent(SmsRetriever.SMS_RETRIEVED_ACTION);
            Bundle bundle = new Bundle();
            bundle.putParcelable(SmsRetriever.EXTRA_STATUS, new Status(CommonStatusCodes.SUCCESS));
            bundle.putString(SmsRetriever.EXTRA_SMS_MESSAGE, sms);
            intent.putExtras(bundle);

            BroadcastReceiver receiver = context.getRegisteredReceiver();
            receiver.onReceive(context, intent);
            return Tasks.forResult(null);
        }

        @CalledByNative("FakeSmsRetrieverClient")
        private Task<Void> triggerTimeout() {
            Wrappers.SmsReceiverContext context = super.getContext();
            if (context == null) {
                Log.v(TAG, "FakeSmsRetrieverClient.triggerTimeout failed: no context was set");
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
     * Sets SmsRetrieverClient to SmsReceiver to allow faking SMSes from android
     * client.
     **/
    @CalledByNative
    private static void setClientForTesting(
            SmsReceiver receiver, Wrappers.SmsRetrieverClientWrapper client) {
        receiver.setClientForTesting(client);
    }
}
