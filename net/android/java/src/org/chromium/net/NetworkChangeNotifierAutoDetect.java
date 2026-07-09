// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_VPN;
import static android.net.NetworkCapabilities.TRANSPORT_VPN;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.Manifest.permission;
import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;

import java.util.Arrays;

import javax.annotation.concurrent.GuardedBy;

/**
 * Used by the NetworkChangeNotifier to listens to platform changes in connectivity. Note that use
 * of this class requires that the app have the platform ACCESS_NETWORK_STATE permission.
 */
// TODO(crbug.com/40479664): Fix this properly.
@SuppressLint("NewApi")
@NullMarked
public class NetworkChangeNotifierAutoDetect extends BroadcastReceiver {
    /** Queries the WifiManager for SSID of the current Wifi connection. */
    static class WifiManagerDelegate {
        @SuppressWarnings("NullAway.Init") // Due to test-only constructor.
        private final Context mContext;

        // Lock all members below.
        private final Object mLock = new Object();

        // Has mHasWifiPermission been calculated.
        @GuardedBy("mLock")
        private boolean mHasWifiPermissionComputed;

        // Only valid when mHasWifiPermissionComputed is set.
        @GuardedBy("mLock")
        private boolean mHasWifiPermission;

        // Only valid when mHasWifiPermission is set.
        @GuardedBy("mLock")
        private @Nullable WifiManager mWifiManager;

        WifiManagerDelegate(Context context) {
            // Getting SSID requires more permissions in later Android releases.
            assert Build.VERSION.SDK_INT < Build.VERSION_CODES.M;
            mContext = context;
        }

        @VisibleForTesting
        @SuppressWarnings("NullAway")
        WifiManagerDelegate() {
            // Tests must mock all methods.
            mContext = null;
        }

        // Lazily determine if app has ACCESS_WIFI_STATE permission.
        @GuardedBy("mLock")
        @SuppressLint("WifiManagerPotentialLeak")
        @SuppressWarnings("NullAway") // Too hard for it to verify.
        @EnsuresNonNullIf("mWifiManager")
        private boolean hasPermissionLocked() {
            if (mHasWifiPermissionComputed) {
                return mHasWifiPermission;
            }
            mHasWifiPermission =
                    mContext.getPackageManager()
                                    .checkPermission(
                                            permission.ACCESS_WIFI_STATE, mContext.getPackageName())
                            == PackageManager.PERMISSION_GRANTED;
            // TODO(crbug.com/40479664): Fix lint properly.
            mWifiManager =
                    mHasWifiPermission
                            ? (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE)
                            : null;
            mHasWifiPermissionComputed = true;
            return mHasWifiPermission;
        }

        String getWifiSsid() {
            // Synchronized because this method can be called on multiple threads (e.g. mLooper
            // from a private caller, and another thread calling a public API like
            // getCurrentNetworkState) and is otherwise racy.
            synchronized (mLock) {
                // If app has permission it's faster to query WifiManager directly.
                if (hasPermissionLocked()) {
                    WifiInfo wifiInfo = getWifiInfoLocked();
                    if (wifiInfo != null) {
                        return wifiInfo.getSSID();
                    }
                    return "";
                }
            }
            return AndroidNetworkLibrary.getWifiSSID();
        }

        // Fetches WifiInfo and records UMA for NullPointerExceptions.
        @GuardedBy("mLock")
        @RequiresNonNull("mWifiManager")
        private @Nullable WifiInfo getWifiInfoLocked() {
            try {
                return mWifiManager.getConnectionInfo();
            } catch (NullPointerException firstException) {
                // Rarely this unexpectedly throws. Retry or just return {@code null} if it fails.
                try {
                    return mWifiManager.getConnectionInfo();
                } catch (NullPointerException secondException) {
                    return null;
                }
            }
        }
    }

    // NetworkCallback used for listening for changes to the default network.
    private class DefaultNetworkCallback extends NetworkCallback {
        // If registered, notify connectionTypeChanged() to look for changes.
        @Override
        public void onAvailable(@Nullable Network network) {
            if (mRegistered) {
                connectionTypeChanged();
            }
        }

        @Override
        public void onLost(final Network network) {
            onAvailable(null);
        }

        // LinkProperties changes include enabling/disabling DNS-over-TLS.
        @Override
        public void onLinkPropertiesChanged(Network network, LinkProperties linkProperties) {
            onAvailable(null);
        }
    }

    // NetworkCallback used for listening for changes to the default network.
    // This version has two major bug fixes over the above DefaultNetworkCallback:
    // 1. Avoids avoids calling synchronous ConnectivityManager methods which is prohibited inside
    //    NetworkCallbacks see "Do NOT call" here:
    //
    // https://developer.android.com/reference/android/net/ConnectivityManager.NetworkCallback#onAvailable(android.net.Network)
    // 2. Catches onCapabilitiesChanged() which includes cellular connections transitioning to and
    //    from SUSPENDED states.  Failing to catch this could leave the NetworkChangeNotifier in
    //    an incorrect disconnected state, see crbug.com/1120144.
    @RequiresApi(Build.VERSION_CODES.P)
    private class AndroidRDefaultNetworkCallback extends NetworkCallback {
        @Nullable LinkProperties mLinkProperties;
        @Nullable NetworkCapabilitiesWrapper mNetworkCapabilities;

        @Override
        public void onAvailable(Network network) {
            // Clear accumulated state and wait for new state to be received.
            // Android guarantees we receive onLinkPropertiesChanged and
            // onNetworkCapabilities calls after onAvailable:
            // https://developer.android.com/reference/android/net/ConnectivityManager.NetworkCallback#onCapabilitiesChanged(android.net.Network,%20android.net.NetworkCapabilities)
            // so the call to connectionTypeChangedTo() is done when we have received the
            // LinkProperties and NetworkCapabilities.
            mLinkProperties = null;
            mNetworkCapabilities = null;
        }

        @Override
        public void onLost(final Network network) {
            mLinkProperties = null;
            mNetworkCapabilities = null;
            if (mRegistered) {
                connectionTypeChangedTo(
                        new ConnectivityManagerWrapper.NetworkState(
                                false, -1, -1, false, null, false, ""));
            }
        }

        // LinkProperties changes include enabling/disabling DNS-over-TLS.
        @Override
        public void onLinkPropertiesChanged(Network network, LinkProperties linkProperties) {
            mLinkProperties = linkProperties;
            if (mRegistered && mLinkProperties != null && mNetworkCapabilities != null) {
                connectionTypeChangedTo(getNetworkState(network));
            }
        }

        // CapabilitiesChanged includes cellular connections switching in and out of SUSPENDED.
        @Override
        public void onCapabilitiesChanged(
                Network network, NetworkCapabilities networkCapabilities) {
            mNetworkCapabilities = new NetworkCapabilitiesWrapper(networkCapabilities);
            if (mRegistered && mLinkProperties != null && mNetworkCapabilities != null) {
                connectionTypeChangedTo(getNetworkState(network));
            }
        }

        // Calculate current NetworkState.  Unlike getNetworkState(), this method avoids calling
        // synchronous ConnectivityManager methods which is prohibited inside NetworkCallbacks see
        // "Do NOT call" here:
        // https://developer.android.com/reference/android/net/ConnectivityManager.NetworkCallback#onAvailable(android.net.Network)
        @NullUnmarked
        private ConnectivityManagerWrapper.NetworkState getNetworkState(Network network) {
            // Initialize to unknown values then extract more accurate info
            int type = -1;
            int subtype = -1;
            if (mNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
                    || mNetworkCapabilities.hasTransport(
                            NetworkCapabilities.TRANSPORT_WIFI_AWARE)) {
                type = ConnectivityManager.TYPE_WIFI;
            } else if (mNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) {
                type = ConnectivityManager.TYPE_MOBILE;
                // To get the subtype we need to make a synchronous ConnectivityManager call
                // unfortunately.  It's recommended to use TelephonyManager.getDataNetworkType()
                // but that requires an additional permission.  Worst case this might be inaccurate
                // but getting the correct subtype is much much less important than getting the
                // correct type.  Incorrect type could make Chrome behave like it's offline,
                // incorrect subtype will just make cellular bandwidth estimates incorrect.
                NetworkInfo networkInfo = mConnectivityManagerWrapper.getRawNetworkInfo(network);
                if (networkInfo != null) {
                    subtype = networkInfo.getSubtype();
                }
            } else if (mNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) {
                type = ConnectivityManager.TYPE_ETHERNET;
            } else if (mNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_BLUETOOTH)) {
                type = ConnectivityManager.TYPE_BLUETOOTH;
            } else if (mNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) {
                // Use ConnectivityManagerWrapper.getNetworkInfo(network) to find underlying
                // network which has a more useful transport type. crbug.com/1208022
                NetworkInfo networkInfo = mConnectivityManagerWrapper.getNetworkInfo(network);
                type = networkInfo != null ? networkInfo.getType() : ConnectivityManager.TYPE_VPN;
            }
            boolean isMetered =
                    !mNetworkCapabilities.hasCapability(
                            NetworkCapabilities.NET_CAPABILITY_NOT_METERED);
            return new ConnectivityManagerWrapper.NetworkState(
                    true,
                    type,
                    subtype,
                    isMetered,
                    String.valueOf(ConnectivityManagerWrapper.networkToNetId(network)),
                    mLinkProperties.isPrivateDnsActive(),
                    mLinkProperties.getPrivateDnsServerName());
        }
    }

    // This class gets called back by ConnectivityManager whenever networks come
    // and go. It gets called back on a special handler thread
    // ConnectivityManager creates for making the callbacks. The callbacks in
    // turn post to mLooper where mObserver lives.
    private class MyNetworkCallback extends NetworkCallback {
        // If non-null, this indicates a VPN is in place for the current user, and no other
        // networks are accessible.
        private @Nullable Network mVpnInPlace;

        // Initialize mVpnInPlace.
        void initializeVpnInPlace() {
            try (ScopedSysTraceEvent event =
                    ScopedSysTraceEvent.scoped(
                            "NetworkChangeNotifierAutoDetect.initializeVpnInPlace")) {
                final Network[] networks =
                        getAllNetworksFiltered(mConnectivityManagerWrapper, null);
                mVpnInPlace = null;
                // If the filtered list of networks contains just a VPN, then that VPN is in place.
                if (networks.length == 1) {
                    final NetworkCapabilitiesWrapper capabilities =
                            mConnectivityManagerWrapper.getNetworkCapabilities(networks[0]);
                    if (capabilities != null && capabilities.hasTransport(TRANSPORT_VPN)) {
                        mVpnInPlace = networks[0];
                    }
                }
            }
        }

        /**
         * Should changes to network {@code network} be ignored due to a VPN being in place
         * and blocking direct access to {@code network}?
         * @param network Network to possibly consider ignoring changes to.
         */
        private boolean ignoreNetworkDueToVpn(Network network) {
            return mVpnInPlace != null && !mVpnInPlace.equals(network);
        }

        /**
         * Should changes to connected network {@code network} be ignored?
         *
         * @param network Network to possibly consider ignoring changes to.
         * @param capabilities {@code NetworkCapabilitiesWrapper} for {@code network} if known,
         *     otherwise {@code null}.
         * @return {@code true} when either: {@code network} is an inaccessible VPN, or has already
         *     disconnected.
         */
        private boolean ignoreConnectedInaccessibleVpn(
                Network network, @Nullable NetworkCapabilitiesWrapper capabilities) {
            // Ignore inaccessible VPNs as they don't apply to Chrome.
            return capabilities == null
                    || (capabilities.hasTransport(TRANSPORT_VPN)
                            && !mConnectivityManagerWrapper.vpnAccessible(network));
        }

        /**
         * Should changes to connected network {@code network} be ignored?
         *
         * @param network Network to possible consider ignoring changes to.
         * @param capabilities {@code NetworkCapabilitiesWrapper} for {@code network} if known,
         *     otherwise {@code null}.
         */
        private boolean ignoreConnectedNetwork(
                Network network, @Nullable NetworkCapabilitiesWrapper capabilities) {
            return ignoreNetworkDueToVpn(network)
                    || ignoreConnectedInaccessibleVpn(network, capabilities);
        }

        @Override
        public void onAvailable(Network network) {
            try (TraceEvent e = TraceEvent.scoped("NetworkChangeNotifierCallback::onAvailable")) {
                NetworkCapabilitiesWrapper capabilities =
                        mConnectivityManagerWrapper.getNetworkCapabilities(network);
                if (ignoreConnectedNetwork(network, capabilities)) {
                    return;
                }
                capabilities = assumeNonNull(capabilities);
                final boolean makeVpnDefault =
                        capabilities.hasTransport(TRANSPORT_VPN)
                                &&
                                // Only make the VPN the default if it isn't already.
                                (mVpnInPlace == null || !network.equals(mVpnInPlace));
                if (makeVpnDefault) {
                    mVpnInPlace = network;
                }
                final long netId = ConnectivityManagerWrapper.networkToNetId(network);
                @ConnectionType
                final int connectionType = mConnectivityManagerWrapper.getConnectionType(network);
                runOnThread(
                        new Runnable() {
                            @Override
                            public void run() {
                                mObserver.onNetworkConnect(netId, connectionType);
                                if (makeVpnDefault) {
                                    // Make VPN the default network.
                                    mObserver.onConnectionTypeChanged(connectionType);
                                    // Purge all other networks as they're inaccessible to Chrome
                                    // now.
                                    mObserver.purgeActiveNetworkList(new long[] {netId});
                                }
                            }
                        });
            }
        }

        @Override
        public void onCapabilitiesChanged(
                Network network, NetworkCapabilities networkCapabilities) {
            try (TraceEvent e =
                    TraceEvent.scoped("NetworkChangeNotifierCallback::onCapabilitiesChanged")) {
                NetworkCapabilitiesWrapper capabilities =
                        new NetworkCapabilitiesWrapper(networkCapabilities);
                if (ignoreConnectedNetwork(network, capabilities)) {
                    return;
                }
                // A capabilities change may indicate the ConnectionType has changed, so forward
                // the new ConnectionType along to observer.
                final long netId = ConnectivityManagerWrapper.networkToNetId(network);
                @ConnectionType final int connectionType;
                if (ConnectivityManagerWrapper.getDeriveConnectionTypeFromCapabilities()) {
                    connectionType =
                            ConnectivityManagerWrapper.getConnectionTypeFromCapabilities(
                                    capabilities);
                } else {
                    connectionType = mConnectivityManagerWrapper.getConnectionType(network);
                }
                runOnThread(
                        new Runnable() {
                            @Override
                            public void run() {
                                mObserver.onNetworkConnect(netId, connectionType);
                            }
                        });
            }
        }

        @Override
        public void onLosing(Network network, int maxMsToLive) {
            try (TraceEvent e = TraceEvent.scoped("NetworkChangeNotifierCallback::onLosing")) {
                final NetworkCapabilitiesWrapper capabilities =
                        mConnectivityManagerWrapper.getNetworkCapabilities(network);
                if (ignoreConnectedNetwork(network, capabilities)) {
                    return;
                }
                final long netId = ConnectivityManagerWrapper.networkToNetId(network);
                runOnThread(
                        new Runnable() {
                            @Override
                            public void run() {
                                mObserver.onNetworkSoonToDisconnect(netId);
                            }
                        });
            }
        }

        @Override
        public void onLost(final Network network) {
            try (TraceEvent e = TraceEvent.scoped("NetworkChangeNotifierCallback::onLost")) {
                if (ignoreNetworkDueToVpn(network)) {
                    return;
                }
                runOnThread(
                        new Runnable() {
                            @Override
                            public void run() {
                                mObserver.onNetworkDisconnect(
                                        ConnectivityManagerWrapper.networkToNetId(network));
                            }
                        });
                // If the VPN is going away, inform observer that other networks that were
                // previously hidden by ignoreNetworkDueToVpn() are now available for use, now that
                // this user's traffic is not forced into the VPN.
                if (mVpnInPlace != null) {
                    assert network.equals(mVpnInPlace);
                    mVpnInPlace = null;
                    for (Network newNetwork :
                            getAllNetworksFiltered(mConnectivityManagerWrapper, network)) {
                        onAvailable(newNetwork);
                    }
                    updateCurrentNetworkState();
                    @ConnectionType
                    final int newConnectionType = getCurrentNetworkState().getConnectionType();
                    runOnThread(
                            new Runnable() {
                                @Override
                                public void run() {
                                    mObserver.onConnectionTypeChanged(newConnectionType);
                                }
                            });
                }
            }
        }
    }

    /**
     * Abstract class for providing a policy regarding when the NetworkChangeNotifier
     * should listen for network changes.
     */
    public abstract static class RegistrationPolicy {
        private @Nullable NetworkChangeNotifierAutoDetect mNotifier;

        /** Start listening for network changes. */
        protected final void register() {
            assert mNotifier != null;
            mNotifier.register();
        }

        /** Stop listening for network changes. */
        protected final void unregister() {
            assert mNotifier != null;
            mNotifier.unregister();
        }

        /**
         * Initializes the policy with the notifier, overriding subclasses should always
         * call this method.
         */
        protected void init(NetworkChangeNotifierAutoDetect notifier) {
            mNotifier = notifier;
        }

        protected abstract void destroy();
    }

    private static final String TAG = NetworkChangeNotifierAutoDetect.class.getSimpleName();

    // {@link Looper} for the thread this object lives on.
    private final @Nullable Looper mLooper;
    // Used to post to the thread this object lives on.
    private final Handler mHandler;
    // {@link IntentFilter} for incoming global broadcast {@link Intent}s this object listens for.
    private final NetworkConnectivityIntentFilter mIntentFilter;
    // Notifications are sent to this {@link Observer}.
    private final Observer mObserver;
    private final RegistrationPolicy mRegistrationPolicy;
    // Starting with Android Pie, used to detect changes in default network.
    private @Nullable NetworkCallback mDefaultNetworkCallback;

    // mConnectivityManagerWrappers and mWifiManagerDelegate are only non-final for testing.
    private ConnectivityManagerWrapper mConnectivityManagerWrapper;

    private @Nullable WifiManagerDelegate mWifiManagerDelegate;

    // mNetworkCallback and mNetworkRequest are only non-null in Android L and above.
    // mNetworkCallback will be null if ConnectivityManager.registerNetworkCallback() ever fails.
    private @Nullable MyNetworkCallback mNetworkCallback;
    private final NetworkRequest mNetworkRequest;
    private boolean mRegistered;

    private ConnectivityManagerWrapper.NetworkState mNetworkState;

    // When a BroadcastReceiver is registered for a sticky broadcast that has been sent out at
    // least once, onReceive() will immediately be called. mIgnoreNextBroadcast is set to true
    // when this class is registered in such a circumstance, and indicates that the next
    // invokation of onReceive() can be ignored as the state hasn't actually changed. Immediately
    // prior to mIgnoreNextBroadcast being set, all internal state is updated to the current device
    // state so were this initial onReceive() call not ignored, no signals would be passed to
    // observers anyhow as the state hasn't changed. This is simply an optimization to avoid
    // useless work.
    private boolean mIgnoreNextBroadcast;
    // mSignal is set to false when it's not worth calculating if signals to Observers should
    // be sent out because this class is being constructed and the internal state has just
    // been updated to the current device state, so no signals are necessary. This is simply an
    // optimization to avoid useless work.
    private boolean mShouldSignalObserver;
    // Indicates if ConnectivityManager.registerNetworkRequest() ever failed. When true, no
    // network-specific callbacks (e.g. Observer.onNetwork*() ) will be issued.
    private boolean mRegisterNetworkCallbackFailed;

    /** Observer interface by which observer is notified of network changes. */
    public interface Observer {
        /** Called when default network changes. */
        void onConnectionTypeChanged(@ConnectionType int newConnectionType);

        /** Called when connection cost of default network changes. */
        void onConnectionCostChanged(int newConnectionCost);

        /** Called when connection subtype of default network changes. */
        void onConnectionSubtypeChanged(int newConnectionSubtype);

        /**
         * Called when device connects to network with NetID netId. For example device associates
         * with a WiFi access point. connectionType is the type of the network; a member of
         * ConnectionType. Only called on Android L and above.
         */
        void onNetworkConnect(long netId, int connectionType);

        /**
         * Called when device determines the connection to the network with NetID netId is no longer
         * preferred, for example when a device transitions from cellular to WiFi it might deem the
         * cellular connection no longer preferred. The device will disconnect from the network in
         * 30s allowing network communications on that network to wrap up. Only called on Android L
         * and above.
         */
        void onNetworkSoonToDisconnect(long netId);

        /**
         * Called when device disconnects from network with NetID netId. Only called on Android L
         * and above.
         */
        void onNetworkDisconnect(long netId);

        /**
         * Called to cause a purge of cached lists of active networks, of any networks not in the
         * accompanying list of active networks. This is issued if a period elapsed where
         * disconnected notifications may have been missed, and acts to keep cached lists of active
         * networks accurate. Only called on Android L and above.
         */
        void purgeActiveNetworkList(long[] activeNetIds);
    }

    /**
     * Constructs a NetworkChangeNotifierAutoDetect.  Lives on calling thread, receives broadcast
     * notifications on the UI thread and forwards the notifications to be processed on the calling
     * thread.
     * @param policy The RegistrationPolicy which determines when this class should watch
     *     for network changes (e.g. see (@link RegistrationPolicyAlwaysRegister} and
     *     {@link RegistrationPolicyApplicationStatus}).
     */
    public NetworkChangeNotifierAutoDetect(Observer observer, RegistrationPolicy policy) {
        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped("NetworkChangeNotifierAutoDetect.constructor")) {
            Looper myLooper = Looper.myLooper();
            assert myLooper != null;
            mLooper = myLooper;
            mHandler = new Handler(myLooper);
            mObserver = observer;
            mConnectivityManagerWrapper =
                    new ConnectivityManagerWrapper(ContextUtils.getApplicationContext());
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
                mWifiManagerDelegate =
                        new WifiManagerDelegate(ContextUtils.getApplicationContext());
            }
            mNetworkCallback = new MyNetworkCallback();
            mNetworkRequest =
                    new NetworkRequest.Builder()
                            .addCapability(NET_CAPABILITY_INTERNET)
                            // Need to hear about VPNs too.
                            .removeCapability(NET_CAPABILITY_NOT_VPN)
                            .build();

            // Use AndroidRDefaultNetworkCallback to fix Android R issue crbug.com/1120144.
            // This NetworkCallback could be used on O+ (where onCapabilitiesChanged and
            // onLinkProperties callbacks are guaranteed to be called after onAvailable)
            // but is only necessary on Android R+.  For now it's only used on R+ to reduce
            // churn.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                mDefaultNetworkCallback = new AndroidRDefaultNetworkCallback();
            } else {
                mDefaultNetworkCallback =
                        Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                                ? new DefaultNetworkCallback()
                                : null;
            }
            updateCurrentNetworkState();
            mIntentFilter = new NetworkConnectivityIntentFilter();
            mIgnoreNextBroadcast = false;
            mShouldSignalObserver = false;
            mRegistrationPolicy = policy;
            mRegistrationPolicy.init(this);
            mShouldSignalObserver = true;
        }
    }

    private boolean onThread() {
        return mLooper == Looper.myLooper();
    }

    private void assertOnThread() {
        if (BuildConfig.ENABLE_ASSERTS && !onThread()) {
            throw new IllegalStateException(
                    "Must be called on NetworkChangeNotifierAutoDetect thread.");
        }
    }

    private void runOnThread(Runnable r) {
        if (onThread()) {
            r.run();
        } else {
            // Once execution begins on the correct thread, make sure unregister() hasn't
            // been called in the mean time.
            mHandler.post(
                    () -> {
                        if (mRegistered) r.run();
                    });
        }
    }

    /** Allows overriding the ConnectivityManagerWrapper for tests. */
    void setConnectivityManagerWrapperForTests(ConnectivityManagerWrapper wrapper) {
        var oldValue = mConnectivityManagerWrapper;
        mConnectivityManagerWrapper = wrapper;
        ResettersForTesting.register(() -> mConnectivityManagerWrapper = oldValue);
    }

    /** Allows overriding the WifiManagerDelegate for tests. */
    void setWifiManagerDelegateForTests(WifiManagerDelegate delegate) {
        var oldValue = mWifiManagerDelegate;
        mWifiManagerDelegate = delegate;
        ResettersForTesting.register(() -> mWifiManagerDelegate = oldValue);
    }

    @VisibleForTesting
    RegistrationPolicy getRegistrationPolicy() {
        return mRegistrationPolicy;
    }

    /** Returns whether the object has registered to receive network connectivity intents. */
    boolean isReceiverRegisteredForTesting() {
        return mRegistered;
    }

    public void destroy() {
        assertOnThread();
        mRegistrationPolicy.destroy();
        unregister();
    }

    /** Registers a BroadcastReceiver in the given context. */
    public void register() {
        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped("NetworkChangeNotifierAutoDetect.register")) {
            assertOnThread();
            if (mRegistered) {
                // Even when registered previously, Android may not send callbacks about change of
                // network state when the device screen is turned on from off. Get the most
                // up-to-date
                // network state. See https://crbug.com/1007998 for more details.
                connectionTypeChanged();
                return;
            }

            if (mShouldSignalObserver) {
                connectionTypeChanged();
            }
            if (mDefaultNetworkCallback != null) {
                try {
                    mConnectivityManagerWrapper.registerDefaultNetworkCallback(
                            mDefaultNetworkCallback, mHandler);
                } catch (RuntimeException e) {
                    // If registering a default network callback failed, fallback to
                    // listening for CONNECTIVITY_ACTION broadcast.
                    mDefaultNetworkCallback = null;
                }
            }
            if (mDefaultNetworkCallback == null) {
                // When registering for a sticky broadcast, like CONNECTIVITY_ACTION, if
                // registerReceiver returns non-null, it means the broadcast was previously issued
                // and
                // onReceive() will be immediately called with this previous Intent. Since this
                // initial
                // callback doesn't actually indicate a network change, we can ignore it by setting
                // mIgnoreNextBroadcast.
                mIgnoreNextBroadcast =
                        ContextUtils.registerProtectedBroadcastReceiver(
                                        ContextUtils.getApplicationContext(), this, mIntentFilter)
                                != null;
            }
            mRegistered = true;

            if (mNetworkCallback != null) {
                mNetworkCallback.initializeVpnInPlace();
                try {
                    mConnectivityManagerWrapper.registerNetworkCallback(
                            mNetworkRequest, mNetworkCallback, mHandler);
                } catch (RuntimeException e) {
                    mRegisterNetworkCallbackFailed = true;
                    // If Android thinks this app has used up all available NetworkRequests, don't
                    // bother trying to register any more callbacks as Android will still think
                    // all available NetworkRequests are used up and fail again needlessly.
                    // Also don't bother unregistering as this call didn't actually register.
                    // See crbug.com/791025 for more info.
                    mNetworkCallback = null;
                }
                if (!mRegisterNetworkCallbackFailed && mShouldSignalObserver) {
                    // registerNetworkCallback() will rematch the NetworkRequest
                    // against active networks, so a cached list of active networks
                    // will be repopulated immediately after this. However we need to
                    // purge any cached networks as they may have been disconnected
                    // while mNetworkCallback was unregistered.
                    final Network[] networks =
                            getAllNetworksFiltered(mConnectivityManagerWrapper, null);
                    // Convert Networks to NetIDs.
                    final long[] netIds = new long[networks.length];
                    for (int i = 0; i < networks.length; i++) {
                        netIds[i] = ConnectivityManagerWrapper.networkToNetId(networks[i]);
                    }
                    mObserver.purgeActiveNetworkList(netIds);
                }
            }
        }
    }

    /** Unregisters a BroadcastReceiver in the given context. */
    public void unregister() {
        assertOnThread();
        if (!mRegistered) return;
        mRegistered = false;
        if (mNetworkCallback != null) {
            mConnectivityManagerWrapper.unregisterNetworkCallback(mNetworkCallback);
        }
        if (mDefaultNetworkCallback != null) {
            mConnectivityManagerWrapper.unregisterNetworkCallback(mDefaultNetworkCallback);
        } else {
            ContextUtils.getApplicationContext().unregisterReceiver(this);
        }
    }

    /**
     * Updates internally stored network state by querying the current state from the system.
     * TODO(crbug.com/40936429): migrate external callers to getCurrentNetworkState() and make this
     * method private (to be called only when updates are received from the system.)
     */
    @EnsuresNonNull("mNetworkState")
    public void updateCurrentNetworkState() {
        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped(
                        "NetworkChangeNotifierAutoDetect.updateCurrentNetworkState")) {
            mNetworkState = mConnectivityManagerWrapper.getNetworkState(mWifiManagerDelegate);
        }
    }

    public ConnectivityManagerWrapper.NetworkState getCurrentNetworkState() {
        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped(
                        "NetworkChangeNotifierAutoDetect.getCurrentNetworkState")) {
            return mNetworkState;
        }
    }

    /**
     * Returns all connected networks that are useful and accessible to Chrome.
     *
     * @param ignoreNetwork ignore this network as if it is not connected.
     */
    static Network[] getAllNetworksFiltered(
            ConnectivityManagerWrapper connectivityManagerWrapper,
            @Nullable Network ignoreNetwork) {
        Network[] networks = connectivityManagerWrapper.getAllNetworksUnfiltered();
        // Whittle down |networks| into just the list of networks useful to us.
        int filteredIndex = 0;
        for (Network network : networks) {
            if (network.equals(ignoreNetwork)) {
                continue;
            }
            final NetworkCapabilitiesWrapper capabilities =
                    connectivityManagerWrapper.getNetworkCapabilities(network);
            if (capabilities == null || !capabilities.hasCapability(NET_CAPABILITY_INTERNET)) {
                continue;
            }
            if (capabilities.hasTransport(TRANSPORT_VPN)) {
                // If we can access the VPN then...
                if (connectivityManagerWrapper.vpnAccessible(network)) {
                    // ...we cannot access any other network, so return just the VPN.
                    return new Network[] {network};
                } else {
                    // ...otherwise ignore it as we cannot use it.
                    continue;
                }
            }
            networks[filteredIndex++] = network;
        }
        return Arrays.copyOf(networks, filteredIndex);
    }

    /** Returns all connected networks that are useful and accessible to Chrome. */
    public Network[] getNetworksForTesting() {
        return getAllNetworksFiltered(mConnectivityManagerWrapper, null);
    }

    /**
     * Returns an array of all of the device's currently connected
     * networks and ConnectionTypes, including only those that are useful and accessible to Chrome.
     * Array elements are a repeated sequence of:
     *   NetID of network
     *   ConnectionType of network
     * Only available when auto-detection has been enabled.
     */
    public long[] getNetworksAndTypes() {
        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped("NetworkChangeNotifierAutoDetect.getNetworksAndTypes")) {
            final Network[] networks = getAllNetworksFiltered(mConnectivityManagerWrapper, null);
            final long[] networksAndTypes = new long[networks.length * 2];
            int index = 0;
            for (Network network : networks) {
                networksAndTypes[index++] = ConnectivityManagerWrapper.networkToNetId(network);
                networksAndTypes[index++] = mConnectivityManagerWrapper.getConnectionType(network);
            }
            return networksAndTypes;
        }
    }

    /**
     * Returns the device's current default connected network used for
     * communication.
     * Returns null when not implemented.
     */
    public @Nullable Network getDefaultNetwork() {
        return mConnectivityManagerWrapper.getDefaultNetwork();
    }

    /**
     * Returns NetID of device's current default connected network used for
     * communication.
     * Returns NetId.INVALID when not implemented.
     */
    public long getDefaultNetId() {
        Network network = getDefaultNetwork();
        return network == null ? NetId.INVALID : ConnectivityManagerWrapper.networkToNetId(network);
    }

    /**
     * Returns {@code true} if NetworkCallback failed to register, indicating that network-specific
     * callbacks will not be issued.
     */
    public boolean registerNetworkCallbackFailed() {
        return mRegisterNetworkCallbackFailed;
    }

    // BroadcastReceiver
    @Override
    public void onReceive(Context context, Intent intent) {
        runOnThread(
                new Runnable() {
                    @Override
                    public void run() {
                        if (mIgnoreNextBroadcast) {
                            mIgnoreNextBroadcast = false;
                            return;
                        }
                        connectionTypeChanged();
                    }
                });
    }

    private void connectionTypeChanged() {
        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped(
                        "NetworkChangeNotifierAutoDetect.connectionTypeChanged")) {
            connectionTypeChangedTo(
                    mConnectivityManagerWrapper.getNetworkState(mWifiManagerDelegate));
        }
    }

    private void connectionTypeChangedTo(ConnectivityManagerWrapper.NetworkState networkState) {
        if (networkState.getConnectionType() != mNetworkState.getConnectionType()
                || !networkState.getNetworkIdentifier().equals(mNetworkState.getNetworkIdentifier())
                || networkState.isPrivateDnsActive() != mNetworkState.isPrivateDnsActive()
                || !networkState
                        .getPrivateDnsServerName()
                        .equals(mNetworkState.getPrivateDnsServerName())) {
            mObserver.onConnectionTypeChanged(networkState.getConnectionType());
        }
        if (networkState.getConnectionType() != mNetworkState.getConnectionType()
                || networkState.getConnectionSubtype() != mNetworkState.getConnectionSubtype()) {
            mObserver.onConnectionSubtypeChanged(networkState.getConnectionSubtype());
        }
        if (networkState.getConnectionCost() != mNetworkState.getConnectionCost()) {
            mObserver.onConnectionCostChanged(networkState.getConnectionCost());
        }
        mNetworkState = networkState;
    }

    private static class NetworkConnectivityIntentFilter extends IntentFilter {
        NetworkConnectivityIntentFilter() {
            addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        }
    }
}
