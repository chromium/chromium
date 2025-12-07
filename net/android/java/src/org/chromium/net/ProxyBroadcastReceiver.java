// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Proxy;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;

@NullMarked
final class ProxyBroadcastReceiver extends BroadcastReceiver {
    private final ProxyChangeListener mListener;

    /* package */ ProxyBroadcastReceiver(ProxyChangeListener listener) {
        mListener = listener;
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.M)
    public void onReceive(Context context, final Intent intent) {
        try (TraceEvent e = TraceEvent.scoped("ProxyBroadcastReceiver#onReceive")) {
            if (Proxy.PROXY_CHANGE_ACTION.equals(intent.getAction())) {
                mListener.updateProxyConfigFromConnectivityManager(intent);
            }
        }
    }
}
