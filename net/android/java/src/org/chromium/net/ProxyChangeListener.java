// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

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

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.UsedByReflection;

import java.util.Locale;

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
@NullMarked
public class ProxyChangeListener {
    private static final String TAG = "ProxyChangeListener";

    private final @Nullable Looper mLooper;
    private final Handler mHandler;

    private long mNativePtr;

    // |mProxyReceiver| handles system proxy change notifications pre-M, and also proxy change
    // notifications triggered via reflection. When its onReceive method is called, either the
    // intent contains the new proxy information as an extra, or it indicates that we should
    // look up the system property values.
    //
    // To avoid triggering as a result of system broadcasts, it is registered with an empty intent
    // filter on M and above.
    private @Nullable ProxyReceiver mProxyReceiver;

    // On M and above we also register |mRealProxyReceiver| with a matching intent filter, to act as
    // a trigger for fetching proxy information via ConnectionManager.
    private @Nullable BroadcastReceiver mRealProxyReceiver;

    private @Nullable Delegate mDelegate;

    private static class ProxyConfig {
        public ProxyConfig(String host, int port, @Nullable String pacUrl, String[] exclusionList) {
            mHost = host;
            mPort = port;
            mPacUrl = pacUrl;
            mExclusionList = exclusionList;
        }

        private static @Nullable ProxyConfig fromProxyInfo(@Nullable ProxyInfo proxyInfo) {
            if (proxyInfo == null) {
                return null;
            }
            final String host = proxyInfo.getHost();
            final Uri pacFileUrl = proxyInfo.getPacFileUrl();
            return new ProxyConfig(
                    host == null ? "" : host,
                    proxyInfo.getPort(),
                    Uri.EMPTY.equals(pacFileUrl) ? null : pacFileUrl.toString(),
                    proxyInfo.getExclusionList());
        }

        @Override
        public String toString() {
            String possiblyRedactedHost =
                    mHost.equals("localhost") || mHost.isEmpty() ? mHost : "<redacted>";
            return String.format(
                    Locale.US,
                    "ProxyConfig [mHost=\"%s\", mPort=%d, mPacUrl=%s]",
                    possiblyRedactedHost,
                    mPort,
                    mPacUrl == null ? "null" : "\"<redacted>\"");
        }

        public final String mHost;
        public final int mPort;
        public final @Nullable String mPacUrl;
        public final String[] mExclusionList;

        public static final ProxyConfig DIRECT = new ProxyConfig("", 0, "", new String[0]);
    }

    /** The delegate for ProxyChangeListener. Use for testing. */
    public interface Delegate {
        void proxySettingsChanged();
    }

    private ProxyChangeListener() {
        Looper myLooper = Looper.myLooper();
        assert myLooper != null;
        mLooper = myLooper;
        mHandler = new Handler(mLooper);
    }

    public void setDelegateForTesting(Delegate delegate) {
        var oldValue = mDelegate;
        mDelegate = delegate;
        ResettersForTesting.register(() -> mDelegate = oldValue);
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
        try (TraceEvent e = TraceEvent.scoped("ProxyChangeListener.start")) {
            assertOnThread();
            assert mNativePtr == 0;
            mNativePtr = nativePtr;
            registerBroadcastReceiver();
        }
    }

    @CalledByNative
    public void stop() {
        assertOnThread();
        mNativePtr = 0;
        unregisterBroadcastReceiver();
    }

    @UsedByReflection("WebView embedders call this to override proxy settings")
    private class ProxyReceiver extends BroadcastReceiver {
        @Override
        @UsedByReflection("WebView embedders call this to override proxy settings")
        public void onReceive(Context context, final Intent intent) {
            try (TraceEvent e = TraceEvent.scoped("ProxyChangeListener.ProxyReceiver#onReceive")) {
                RecordHistogram.recordBooleanHistogram(
                        "Net.ProxyChangeListener.ReflectedCall", false);
                if (Proxy.PROXY_CHANGE_ACTION.equals(intent.getAction())) {
                    runOnThread(() -> proxySettingsChanged(extractNewProxy(intent)));
                }
            }
        }
    }

    // Extract a ProxyConfig object from the supplied Intent's extra data
    // bundle. The android.net.ProxyProperties class is not exported from
    // the Android SDK, so we have to use reflection to get at it and invoke
    // methods on it. If we fail, return an empty proxy config (meaning
    // use system properties).
    @SuppressWarnings({"PrivateApi", "ObsoleteSdkInt"})
    private static @Nullable ProxyConfig extractNewProxy(Intent intent) {
        try (TraceEvent e = TraceEvent.scoped("ProxyChangeListener#extractNewProxy")) {
            Bundle extras = intent.getExtras();
            if (extras == null) {
                return null;
            }

            return ProxyConfig.fromProxyInfo(
                    (ProxyInfo) extras.get("android.intent.extra.PROXY_INFO"));
        }
    }

    private void proxySettingsChanged(@Nullable ProxyConfig cfg) {
        try (TraceEvent e = TraceEvent.scoped("ProxyChangeListener#proxySettingsChanged")) {
            assertOnThread();

            if (mDelegate != null) {
                // proxySettingsChanged is called even if mNativePtr == 0, for testing purposes.
                mDelegate.proxySettingsChanged();
            }
            if (mNativePtr == 0) {
                return;
            }

            if (cfg != null) {
                ProxyChangeListenerJni.get()
                        .proxySettingsChangedTo(
                                mNativePtr, cfg.mHost, cfg.mPort, cfg.mPacUrl, cfg.mExclusionList);
            } else {
                ProxyChangeListenerJni.get().proxySettingsChanged(mNativePtr);
            }
        }
    }

    @RequiresApi(Build.VERSION_CODES.M)
    private @Nullable ProxyConfig getProxyConfig(Intent intent) {
        try (TraceEvent e = TraceEvent.scoped("ProxyChangeListener#getProxyConfig")) {
            ConnectivityManager connectivityManager =
                    (ConnectivityManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.CONNECTIVITY_SERVICE);
            ProxyConfig configFromConnectivityManager =
                    ProxyConfig.fromProxyInfo(connectivityManager.getDefaultProxy());

            if (configFromConnectivityManager == null) {
                return ProxyConfig.DIRECT;
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                    && configFromConnectivityManager.mHost.equals("localhost")
                    && configFromConnectivityManager.mPort == -1) {
                ProxyConfig configFromIntent = extractNewProxy(intent);
                Log.i(
                        TAG,
                        "configFromConnectivityManager = %s, configFromIntent = %s",
                        configFromConnectivityManager,
                        configFromIntent);

                // There's a bug in Android Q+ PAC support. If ConnectivityManager returns
                // localhost:-1
                // then use the intent from the PROXY_CHANGE_ACTION broadcast to extract the
                // ProxyConfig's host and port. See http://crbug.com/993538.
                //
                // -1 is never a reasonable port so just keep this workaround for future versions
                // until
                // we're sure it's fixed on the platform side.
                if (configFromIntent == null) return null;
                String correctHost = configFromIntent.mHost;
                int correctPort = configFromIntent.mPort;
                return new ProxyConfig(
                        correctHost,
                        correctPort,
                        configFromConnectivityManager.mPacUrl,
                        configFromConnectivityManager.mExclusionList);
            }
            return configFromConnectivityManager;
        }
    }

    @RequiresApi(Build.VERSION_CODES.M)
    /* package */ void updateProxyConfigFromConnectivityManager(Intent intent) {
        runOnThread(() -> proxySettingsChanged(getProxyConfig(intent)));
    }

    private void registerBroadcastReceiver() {
        assertOnThread();
        assert mProxyReceiver == null;
        assert mRealProxyReceiver == null;

        IntentFilter filter = new IntentFilter();
        filter.addAction(Proxy.PROXY_CHANGE_ACTION);

        mProxyReceiver = new ProxyReceiver();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            // Proxy change broadcast receiver for Pre-M. Uses reflection to extract proxy
            // information from the intent extra.
            ContextUtils.registerProtectedBroadcastReceiver(
                    ContextUtils.getApplicationContext(), mProxyReceiver, filter);
        } else {
            if (!ContextUtils.isSdkSandboxProcess()) {
                // Register the instance of ProxyReceiver with an empty intent filter, so that it is
                // still found via reflection, but is not called by the system. See:
                // crbug.com/851995
                //
                // Don't do this within an SDK Sandbox, because neither reflection nor registering a
                // broadcast receiver with a blank IntentFilter is allowed.
                ContextUtils.registerNonExportedBroadcastReceiver(
                        ContextUtils.getApplicationContext(), mProxyReceiver, new IntentFilter());
            }

            // Create a BroadcastReceiver that uses M+ APIs to fetch the proxy confuguration from
            // ConnectionManager.
            mRealProxyReceiver = new ProxyBroadcastReceiver(this);
            ContextUtils.registerProtectedBroadcastReceiver(
                    ContextUtils.getApplicationContext(), mRealProxyReceiver, filter);
        }
    }

    private void unregisterBroadcastReceiver() {
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
        if (BuildConfig.ENABLE_ASSERTS && !onThread()) {
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

    /** See net/proxy_resolution/proxy_config_service_android.cc */
    @NativeMethods
    interface Natives {
        @NativeClassQualifiedName("ProxyConfigServiceAndroid::JNIDelegate")
        void proxySettingsChangedTo(
                long nativePtr,
                String host,
                int port,
                @Nullable String pacUrl,
                String[] exclusionList);

        @NativeClassQualifiedName("ProxyConfigServiceAndroid::JNIDelegate")
        void proxySettingsChanged(long nativePtr);
    }
}
