// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Handler;

import androidx.annotation.RequiresApi;

import com.google.android.gms.auth.api.phone.SmsCodeBrowserClient;
import com.google.android.gms.auth.api.phone.SmsCodeRetriever;
import com.google.android.gms.auth.api.phone.SmsRetrieverClient;
import com.google.android.gms.tasks.Task;

class Wrappers {
    // Prevent instantiation.
    private Wrappers() {}

    /** Wraps com.google.android.gms.auth.api.phone.SmsRetrieverClient. */
    static class SmsRetrieverClientWrapper {
        // Used for user consent flow.
        private final SmsRetrieverClient mSmsRetrieverClient;
        // Used for browser code flow.
        private final SmsCodeBrowserClient mSmsCodeBrowserClient;
        private WebOTPServiceContext mContext;

        public SmsRetrieverClientWrapper(
                SmsRetrieverClient smsRetrieverClient, SmsCodeBrowserClient smsCodeBrowserClient) {
            mSmsRetrieverClient = smsRetrieverClient;
            mSmsCodeBrowserClient = smsCodeBrowserClient;
        }

        public void setContext(WebOTPServiceContext context) {
            mContext = context;
        }

        public WebOTPServiceContext getContext() {
            return mContext;
        }

        public Task<Void> startSmsCodeBrowserRetriever() {
            return mSmsCodeBrowserClient.startSmsCodeRetriever();
        }

        public Task<Void> startSmsUserConsent(String senderAddress) {
            return mSmsRetrieverClient.startSmsUserConsent(senderAddress);
        }
    }

    /**
     * Extends android.content.ContextWrapper to store and retrieve the
     * registered BroadcastReceiver.
     */
    static class WebOTPServiceContext extends ContextWrapper {
        private BroadcastReceiver mVerificationReceiver;
        private BroadcastReceiver mUserConsentReceiver;
        private final SmsProviderGms mSmsProviderGms;

        public WebOTPServiceContext(Context context, SmsProviderGms provider) {
            super(context);
            mSmsProviderGms = provider;
        }

        public SmsVerificationReceiver getRegisteredVerificationReceiver() {
            return (SmsVerificationReceiver) mVerificationReceiver;
        }

        public SmsUserConsentReceiver getRegisteredUserConsentReceiver() {
            return (SmsUserConsentReceiver) mUserConsentReceiver;
        }

        public SmsVerificationReceiver createVerificationReceiverForTesting() {
            return new SmsVerificationReceiver(mSmsProviderGms, this);
        }

        private void onRegisterReceiver(BroadcastReceiver receiver, IntentFilter filter) {
            if (filter.hasAction(SmsCodeRetriever.SMS_CODE_RETRIEVED_ACTION)) {
                mVerificationReceiver = receiver;
            } else {
                mUserConsentReceiver = receiver;
            }
        }

        // ---------------------------------------------------------------------
        // Context overrides:

        @Override
        public Intent registerReceiver(
                BroadcastReceiver receiver,
                IntentFilter filter,
                String permission,
                Handler handler) {
            onRegisterReceiver(receiver, filter);
            return super.registerReceiver(receiver, filter, permission, handler);
        }

        @Override
        public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
            throw new RuntimeException(); // Not implemented.
        }

        @Override
        @RequiresApi(Build.VERSION_CODES.O)
        public Intent registerReceiver(
                BroadcastReceiver receiver,
                IntentFilter filter,
                String permission,
                Handler handler,
                int flags) {
            onRegisterReceiver(receiver, filter);
            return super.registerReceiver(receiver, filter, permission, handler, flags);
        }

        @Override
        @RequiresApi(Build.VERSION_CODES.O)
        public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter, int flags) {
            throw new RuntimeException(); // Not implemented.
        }

        @Override
        public void unregisterReceiver(BroadcastReceiver receiver) {
            if (receiver == mVerificationReceiver) {
                mVerificationReceiver = null;
            } else {
                mUserConsentReceiver = null;
            }

            super.unregisterReceiver(receiver);
        }
    }
}
