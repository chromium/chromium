// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;

import com.google.android.gms.auth.api.phone.SmsRetrieverClient;
import com.google.android.gms.tasks.Task;

class Wrappers {
    // Prevent instantiation.
    private Wrappers() {}

    /**
     * Wraps com.google.android.gms.auth.api.phone.SmsRetrieverClient.
     */
    static class SmsRetrieverClientWrapper {
        private final SmsRetrieverClient mSmsRetrieverClient;
        private SmsReceiverContext mContext;

        public SmsRetrieverClientWrapper(SmsRetrieverClient smsRetrieverClient) {
            mSmsRetrieverClient = smsRetrieverClient;
        }

        public void setContext(SmsReceiverContext context) {
            mContext = context;
        }

        public SmsReceiverContext getContext() {
            return mContext;
        }

        public Task<Void> startSmsRetriever() {
            return mSmsRetrieverClient.startSmsRetriever();
        }
    }

    /**
     * Extends android.content.ContextWrapper to store and retrieve the
     * registered BroadcastReceiver.
     */
    static class SmsReceiverContext extends ContextWrapper {
        private BroadcastReceiver mReceiver;

        public SmsReceiverContext(Context context) {
            super(context);
        }

        public BroadcastReceiver getRegisteredReceiver() {
            return mReceiver;
        }

        // ---------------------------------------------------------------------
        // Context overrides:

        @Override
        public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
            mReceiver = receiver;
            return super.registerReceiver(receiver, filter);
        }

        @Override
        public void unregisterReceiver(BroadcastReceiver receiver) {
            mReceiver = null;
            super.unregisterReceiver(receiver);
        }
    }
}
