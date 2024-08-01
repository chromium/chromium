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

final class ProxyBroadcastReceiver extends BroadcastReceiver {
    private final ProxyChangeListener mListener;

    /* package */ ProxyBroadcastReceiver(ProxyChangeListener listener) {
        mListener = listener;
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.M)
    public void onReceive(Context context, final Intent intent) {
        if (intent.getAction().equals(Proxy.PROXY_CHANGE_ACTION)) {
            mListener.updateProxyConfigFromConnectivityManager(intent);
        }
    }
}
