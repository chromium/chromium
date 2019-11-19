// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.Proxy;
import android.net.ProxyInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.BuildConfig;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.annotations.UsedByReflection;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * This class partners with native ProxyConfigServiceAndroid to listen for
 * proxy change notifications from Android.
 *
 * Unfortunately this is called directly via reflection in a number of WebView applications
 * to provide a hacky way to set per-application proxy settings, so it must not be mangled by
 * Proguard.
 */
@UsedByReflection("WebView embedders call this to override proxy settings")
@JNINamespace("net")
public class ProxyChangeListener {
    private static final String TAG = "ProxyChangeListener";
    private static boolean sEnabled = true;

    private final Looper mLooper;
    private final Handler mHandler;

    private long mNativePtr;

    // |mProxyReceiver| handles system proxy change notifications pre-M, and also proxy change
    // notifications triggered via reflection. When its onReceive method is called, either the
    // intent contains the new proxy information as an extra, or it indicates that we should
    // look up the system property values.
    //
    // To avoid triggering as a result of system broadcasts, it is registered with an empty intent
    // filter on M and above.
    private ProxyReceiver mProxyReceiver;

    // On M and above we also register |mRealProxyReceiver| with a matching intent filter, to act as
    // a trigger for fetching proxy information via ConnectionManager.
    private BroadcastReceiver mRealProxyReceiver;

    private Delegate mDelegate;

    private static class ProxyConfig {
        public ProxyConfig(String host, int port, String pacUrl, String[] exclusionList) {
            mHost = host;
            mPort = port;
            mPacUrl = pacUrl;
            mExclusionList = exclusionList;
        }

        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        private static ProxyConfig fromProxyInfo(ProxyInfo proxyInfo) {
            if (proxyInfo == null) {
                return null;
            }
            final Uri pacFileUrl = proxyInfo.getPacFileUrl();
            return new ProxyConfig(proxyInfo.getHost(), proxyInfo.getPort(),
                    Uri.EMPTY.equals(pacFileUrl) ? null : pacFileUrl.toString(),
                    proxyInfo.getExclusionList());
        }

        public final String mHost;
        public final int mPort;
        public final String mPacUrl;
        public final String[] mExclusionList;

        public static final ProxyConfig DIRECT = new ProxyConfig("", 0, "", new String[0]);
    }

    /**
     * The delegate for ProxyChangeListener. Use for testing.
     */
    public interface Delegate { public void proxySettingsChanged(); }

    private ProxyChangeListener() {
        mLooper = Looper.myLooper();
        mHandler = new Handler(mLooper);
    }

    public static void setEnabled(boolean enabled) {
        sEnabled = enabled;
    }

    public void setDelegateForTesting(Delegate delegate) {
        mDelegate = delegate;
    }

    @CalledByNative
    public static ProxyChangeListener create() {
        return new ProxyChangeListener();
    }

    @CalledByNative
    public static String getProperty(String property) {
        return System.getProperty(property);
    }

    @CalledByNative
    public void start(long nativePtr) {
        assertOnThread();
        assert mNativePtr == 0;
        mNativePtr = nativePtr;
        registerReceiver();
    }

    @CalledByNative
    public void stop() {
        assertOnThread();
        mNativePtr = 0;
        unregisterReceiver();
    }

    @UsedByReflection("WebView embedders call this to override proxy settings")
    private class ProxyReceiver extends BroadcastReceiver {
        @Override
        @UsedByReflection("WebView embedders call this to override proxy settings")
        public void onReceive(Context context, final Intent intent) {
            if (intent.getAction().equals(Proxy.PROXY_CHANGE_ACTION)) {
                runOnThread(() -> proxySettingsChanged(extractNewProxy(intent)));
            }
        }
    }

    // Extract a ProxyConfig object from the supplied Intent's extra data
    // bundle. The android.net.ProxyProperties class is not exported from
    // the Android SDK, so we have to use reflection to get at it and invoke
    // methods on it. If we fail, return an empty proxy config (meaning
    // use system properties).
    private static ProxyConfig extractNewProxy(Intent intent) {
        Bundle extras = intent.getExtras();
        if (extras == null) {
            return null;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return ProxyConfig.fromProxyInfo(
                    (ProxyInfo) extras.get("android.intent.extra.PROXY_INFO"));
        }

        try {
            final String getHostName = "getHost";
            final String getPortName = "getPort";
            final String getPacFileUrl = "getPacFileUrl";
            final String getExclusionList = "getExclusionList";
            final String className = "android.net.ProxyProperties";

            Object props = extras.get("proxy");
            if (props == null) {
                return null;
            }

            Class<?> cls = Class.forName(className);
            Method getHostMethod = cls.getDeclaredMethod(getHostName);
            Method getPortMethod = cls.getDeclaredMethod(getPortName);
            Method getExclusionListMethod = cls.getDeclaredMethod(getExclusionList);

            String host = (String) getHostMethod.invoke(props);
            int port = (Integer) getPortMethod.invoke(props);

            String[] exclusionList;
            String s = (String) getExclusionListMethod.invoke(props);
            exclusionList = s.split(",");

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                Method getPacFileUrlMethod = cls.getDeclaredMethod(getPacFileUrl);
                String pacFileUrl = (String) getPacFileUrlMethod.invoke(props);
                if (!TextUtils.isEmpty(pacFileUrl)) {
                    return new ProxyConfig(host, port, pacFileUrl, exclusionList);
                }
            }
            return new ProxyConfig(host, port, null, exclusionList);
        } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException
                | InvocationTargetException | NullPointerException ex) {
            Log.e(TAG, "Using no proxy configuration due to exception:" + ex);
            return null;
        }
    }

    private void proxySettingsChanged(ProxyConfig cfg) {
        assertOnThread();

        if (!sEnabled) {
            return;
        }
        if (mDelegate != null) {
            // proxySettingsChanged is called even if mNativePtr == 0, for testing purposes.
            mDelegate.proxySettingsChanged();
        }
        if (mNativePtr == 0) {
            return;
        }

        if (cfg != null) {
            ProxyChangeListenerJni.get().proxySettingsChangedTo(mNativePtr,
                    ProxyChangeListener.this, cfg.mHost, cfg.mPort, cfg.mPacUrl,
                    cfg.mExclusionList);
        } else {
            ProxyChangeListenerJni.get().proxySettingsChanged(mNativePtr, ProxyChangeListener.this);
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    private ProxyConfig getProxyConfig(Intent intent) {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        ProxyInfo proxyInfo = connectivityManager.getDefaultProxy();
        if (proxyInfo == null) {
            return ProxyConfig.DIRECT;
        }

        // TODO(laisminchillo): replace '29' with 'Build.VERSION_CODES.Q'
        // when Android Q is finalized for release
        if (Build.VERSION.SDK_INT == 29 && proxyInfo.getHost().equals("localhost")
                && proxyInfo.getPort() == -1) {
            // There's a bug in Android Q's PAC support. If ConnectivityManager
            // returns localhost:-1 then use the intent from the PROXY_CHANGE_ACTION
            // broadcast to extract the ProxyConfig. See http://crbug.com/993538.
            return extractNewProxy(intent);
        }
        return ProxyConfig.fromProxyInfo(proxyInfo);
    }

    /* package */ void updateProxyConfigFromConnectivityManager(Intent intent) {
        runOnThread(() -> proxySettingsChanged(getProxyConfig(intent)));
    }

    private void registerReceiver() {
        assertOnThread();
        assert mProxyReceiver == null;
        assert mRealProxyReceiver == null;

        IntentFilter filter = new IntentFilter();
        filter.addAction(Proxy.PROXY_CHANGE_ACTION);

        mProxyReceiver = new ProxyReceiver();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            // Proxy change broadcast receiver for Pre-M. Uses reflection to extract proxy
            // information from the intent extra.
            ContextUtils.getApplicationContext().registerReceiver(mProxyReceiver, filter);
        } else {
            // Register the instance of ProxyReceiver with an empty intent filter, so that it is
            // still found via reflection, but is not called by the system. See: crbug.com/851995
            ContextUtils.getApplicationContext().registerReceiver(
                    mProxyReceiver, new IntentFilter());

            // Create a BroadcastReceiver that uses M+ APIs to fetch the proxy confuguration from
            // ConnectionManager.
            mRealProxyReceiver = new ProxyBroadcastReceiver(this);
            ContextUtils.getApplicationContext().registerReceiver(mRealProxyReceiver, filter);
        }
    }

    private void unregisterReceiver() {
        assertOnThread();
        assert mProxyReceiver != null;

        ContextUtils.getApplicationContext().unregisterReceiver(mProxyReceiver);
        if (mRealProxyReceiver != null) {
            ContextUtils.getApplicationContext().unregisterReceiver(mRealProxyReceiver);
        }
        mProxyReceiver = null;
        mRealProxyReceiver = null;
    }

    private boolean onThread() {
        return mLooper == Looper.myLooper();
    }

    private void assertOnThread() {
        if (BuildConfig.DCHECK_IS_ON && !onThread()) {
            throw new IllegalStateException("Must be called on ProxyChangeListener thread.");
        }
    }

    private void runOnThread(Runnable r) {
        if (onThread()) {
            r.run();
        } else {
            mHandler.post(r);
        }
    }

    /**
     * See net/proxy_resolution/proxy_config_service_android.cc
     */

    @NativeMethods
    interface Natives {
        @NativeClassQualifiedName("ProxyConfigServiceAndroid::JNIDelegate")
        void proxySettingsChangedTo(long nativePtr, ProxyChangeListener caller, String host,
                int port, String pacUrl, String[] exclusionList);

        @NativeClassQualifiedName("ProxyConfigServiceAndroid::JNIDelegate")
        void proxySettingsChanged(long nativePtr, ProxyChangeListener caller);
    }
}
