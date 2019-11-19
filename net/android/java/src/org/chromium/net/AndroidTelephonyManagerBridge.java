// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.telephony.PhoneStateListener;
import android.telephony.ServiceState;
import android.telephony.TelephonyManager;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.MainDex;

import javax.annotation.CheckForNull;

/**
 * This class subscribes to {@link TelephonyManager} events and stores
 * some useful values.
 */
@AnyThread
@MainDex
public class AndroidTelephonyManagerBridge {
    private static volatile AndroidTelephonyManagerBridge sInstance;

    @CheckForNull
    private volatile String mNetworkCountryIso;
    @CheckForNull
    private volatile String mNetworkOperator;
    @CheckForNull
    private volatile String mSimOperator;

    private Listener mListener;

    private AndroidTelephonyManagerBridge() {}

    /**
     * Returns the ISO country code equivalent of the current MCC.
     */
    public String getNetworkCountryIso() {
        if (mNetworkCountryIso == null) {
            TelephonyManager telephonyManager = getTelephonyManager();
            if (telephonyManager == null) {
                return "";
            }
            mNetworkCountryIso = telephonyManager.getNetworkCountryIso();
        }
        return mNetworkCountryIso;
    }

    /**
     * Returns the MCC+MNC (mobile country code + mobile network code) as
     * the numeric name of the current registered operator.
     */
    public String getNetworkOperator() {
        if (mNetworkOperator == null) {
            TelephonyManager telephonyManager = getTelephonyManager();
            if (telephonyManager == null) {
                return "";
            }
            mNetworkOperator = telephonyManager.getNetworkOperator();
        }
        return mNetworkOperator;
    }

    /**
     * Returns the MCC+MNC (mobile country code + mobile network code) as
     * the numeric name of the current SIM operator.
     */
    public String getSimOperator() {
        if (mSimOperator == null) {
            TelephonyManager telephonyManager = getTelephonyManager();
            if (telephonyManager == null) {
                return "";
            }
            mSimOperator = telephonyManager.getSimOperator();
        }
        return mSimOperator;
    }

    private void update(@CheckForNull TelephonyManager telephonyManager) {
        if (telephonyManager == null) {
            return;
        }
        mNetworkCountryIso = telephonyManager.getNetworkCountryIso();
        mNetworkOperator = telephonyManager.getNetworkOperator();
        mSimOperator = telephonyManager.getSimOperator();
    }

    @MainThread
    private void listenTelephonyServiceState(TelephonyManager telephonyManager) {
        ThreadUtils.assertOnUiThread();

        mListener = new Listener();
        telephonyManager.listen(mListener, PhoneStateListener.LISTEN_SERVICE_STATE);
    }

    @CheckForNull
    private static TelephonyManager getTelephonyManager() {
        return (TelephonyManager) ContextUtils.getApplicationContext().getSystemService(
                Context.TELEPHONY_SERVICE);
    }

    private static AndroidTelephonyManagerBridge create() {
        AndroidTelephonyManagerBridge instance = new AndroidTelephonyManagerBridge();
        ThreadUtils.runOnUiThread(() -> {
            TelephonyManager telephonyManager = getTelephonyManager();
            if (telephonyManager != null) {
                instance.listenTelephonyServiceState(telephonyManager);
            }
        });
        return instance;
    }

    /**
     * Returns {@link AndroidTelephonyManagerBridge} instance.
     */
    public static AndroidTelephonyManagerBridge getInstance() {
        AndroidTelephonyManagerBridge instance = sInstance;
        if (instance == null) {
            synchronized (AndroidTelephonyManagerBridge.class) {
                instance = sInstance;
                if (instance == null) {
                    instance = create();
                    sInstance = instance;
                }
            }
        }
        return instance;
    }

    private class Listener extends PhoneStateListener {
        @CheckForNull
        private ServiceState mServiceState;

        @Override
        public void onServiceStateChanged(ServiceState serviceState) {
            if (mServiceState == null || !mServiceState.equals(serviceState)) {
                mServiceState = serviceState;
                update(getTelephonyManager());
            }
        }
    }
}
