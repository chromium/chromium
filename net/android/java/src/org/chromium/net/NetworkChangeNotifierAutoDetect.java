// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.net.ConnectivityManager.TYPE_VPN;
import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_VPN;
import static android.net.NetworkCapabilities.TRANSPORT_VPN;

import android.Manifest.permission;
import android.annotation.SuppressLint;
import android.annotation.TargetApi;
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
import android.telephony.TelephonyManager;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildConfig;
import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.compat.ApiHelperForM;

import java.io.IOException;
import java.net.Socket;
import java.util.Arrays;

import javax.annotation.concurrent.GuardedBy;

/**
 * Used by the NetworkChangeNotifier to listens to platform changes in connectivity.
 * Note that use of this class requires that the app have the platform
 * ACCESS_NETWORK_STATE permission.
 */
// TODO(crbug.com/635567): Fix this properly.
@SuppressLint("NewApi")
public class NetworkChangeNotifierAutoDetect extends BroadcastReceiver {
    /**
     * Immutable class representing the state of a device's network.
     */
    public static class NetworkState {
        private final boolean mConnected;
        private final int mType;
        private final int mSubtype;
        // WIFI SSID of the connection on pre-Marshmallow, NetID starting with Marshmallow. Always
        // non-null (i.e. instead of null it'll be an empty string) to facilitate .equals().
        private final String mNetworkIdentifier;
        // Indicates if this network is using DNS-over-TLS.
        private final boolean mIsPrivateDnsActive;

        public NetworkState(boolean connected, int type, int subtype, String networkIdentifier,
                boolean isPrivateDnsActive) {
            mConnected = connected;
            mType = type;
            mSubtype = subtype;
            mNetworkIdentifier = networkIdentifier == null ? "" : networkIdentifier;
            mIsPrivateDnsActive = isPrivateDnsActive;
        }

        public boolean isConnected() {
            return mConnected;
        }

        public int getNetworkType() {
            return mType;
        }

        public int getNetworkSubType() {
            return mSubtype;
        }

        // Always non-null to facilitate .equals().
        public String getNetworkIdentifier() {
            return mNetworkIdentifier;
        }

        /**
         * Returns the connection type for the given NetworkState.
         */
        @ConnectionType
        public int getConnectionType() {
            if (!isConnected()) {
                return ConnectionType.CONNECTION_NONE;
            }
            return convertToConnectionType(getNetworkType(), getNetworkSubType());
        }

        /**
         * Returns the connection subtype for the given NetworkState.
         */
        public int getConnectionSubtype() {
            if (!isConnected()) {
                return ConnectionSubtype.SUBTYPE_NONE;
            }

            switch (getNetworkType()) {
                case ConnectivityManager.TYPE_ETHERNET:
                case ConnectivityManager.TYPE_WIFI:
                case ConnectivityManager.TYPE_WIMAX:
                case ConnectivityManager.TYPE_BLUETOOTH:
                    return ConnectionSubtype.SUBTYPE_UNKNOWN;
                case ConnectivityManager.TYPE_MOBILE:
                    // Use information from TelephonyManager to classify the connection.
                    switch (getNetworkSubType()) {
                        case TelephonyManager.NETWORK_TYPE_GPRS:
                            return ConnectionSubtype.SUBTYPE_GPRS;
                        case TelephonyManager.NETWORK_TYPE_EDGE:
                            return ConnectionSubtype.SUBTYPE_EDGE;
                        case TelephonyManager.NETWORK_TYPE_CDMA:
                            return ConnectionSubtype.SUBTYPE_CDMA;
                        case TelephonyManager.NETWORK_TYPE_1xRTT:
                            return ConnectionSubtype.SUBTYPE_1XRTT;
                        case TelephonyManager.NETWORK_TYPE_IDEN:
                            return ConnectionSubtype.SUBTYPE_IDEN;
                        case TelephonyManager.NETWORK_TYPE_UMTS:
                            return ConnectionSubtype.SUBTYPE_UMTS;
                        case TelephonyManager.NETWORK_TYPE_EVDO_0:
                            return ConnectionSubtype.SUBTYPE_EVDO_REV_0;
                        case TelephonyManager.NETWORK_TYPE_EVDO_A:
                            return ConnectionSubtype.SUBTYPE_EVDO_REV_A;
                        case TelephonyManager.NETWORK_TYPE_HSDPA:
                            return ConnectionSubtype.SUBTYPE_HSDPA;
                        case TelephonyManager.NETWORK_TYPE_HSUPA:
                            return ConnectionSubtype.SUBTYPE_HSUPA;
                        case TelephonyManager.NETWORK_TYPE_HSPA:
                            return ConnectionSubtype.SUBTYPE_HSPA;
                        case TelephonyManager.NETWORK_TYPE_EVDO_B:
                            return ConnectionSubtype.SUBTYPE_EVDO_REV_B;
                        case TelephonyManager.NETWORK_TYPE_EHRPD:
                            return ConnectionSubtype.SUBTYPE_EHRPD;
                        case TelephonyManager.NETWORK_TYPE_HSPAP:
                            return ConnectionSubtype.SUBTYPE_HSPAP;
                        case TelephonyManager.NETWORK_TYPE_LTE:
                            return ConnectionSubtype.SUBTYPE_LTE;
                        default:
                            return ConnectionSubtype.SUBTYPE_UNKNOWN;
                    }
                default:
                    return ConnectionSubtype.SUBTYPE_UNKNOWN;
            }
        }

        /**
         * Returns boolean indicating if this network uses DNS-over-TLS.
         */
        public boolean isPrivateDnsActive() {
            return mIsPrivateDnsActive;
        }
    }

    /** Queries the ConnectivityManager for information about the current connection. */
    static class ConnectivityManagerDelegate {
        private final ConnectivityManager mConnectivityManager;

        ConnectivityManagerDelegate(Context context) {
            mConnectivityManager =
                    (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        }

        // For testing.
        ConnectivityManagerDelegate() {
            // All the methods below should be overridden.
            mConnectivityManager = null;
        }

        /**
         * @param networkInfo The NetworkInfo for the active network.
         * @return the info of the network that is available to this app.
         */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        private NetworkInfo processActiveNetworkInfo(NetworkInfo networkInfo) {
            if (networkInfo == null) {
                return null;
            }

            if (networkInfo.isConnected()) {
                return networkInfo;
            }

            // If |networkInfo| is BLOCKED, but the app is in the foreground, then it's likely that
            // Android hasn't finished updating the network access permissions as BLOCKED is only
            // meant for apps in the background.  See https://crbug.com/677365 for more details.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                // https://crbug.com/677365 primarily affects only Lollipop and higher versions.
                return null;
            }

            if (networkInfo.getDetailedState() != NetworkInfo.DetailedState.BLOCKED) {
                // Network state is not blocked which implies that network access is
                // unavailable (not just blocked to this app).
                return null;
            }

            if (ApplicationStatus.getStateForApplication()
                    != ApplicationState.HAS_RUNNING_ACTIVITIES) {
                // The app is not in the foreground.
                return null;
            }
            return networkInfo;
        }

        /**
         * Returns connection type and status information about the current
         * default network.
         */
        NetworkState getNetworkState(WifiManagerDelegate wifiManagerDelegate) {
            Network network = null;
            NetworkInfo networkInfo;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                network = getDefaultNetwork();
                networkInfo = ApiHelperForM.getNetworkInfo(mConnectivityManager, network);
            } else {
                networkInfo = mConnectivityManager.getActiveNetworkInfo();
            }
            networkInfo = processActiveNetworkInfo(networkInfo);
            if (networkInfo == null) {
                return new NetworkState(false, -1, -1, null, false);
            }
            if (network != null) {
                return new NetworkState(true, networkInfo.getType(), networkInfo.getSubtype(),
                        String.valueOf(networkToNetId(network)),
                        Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                                && AndroidNetworkLibrary.isPrivateDnsActive(
                                           mConnectivityManager.getLinkProperties(network)));
            }
            assert Build.VERSION.SDK_INT < Build.VERSION_CODES.M;
            // If Wifi, then fetch SSID also
            if (networkInfo.getType() == ConnectivityManager.TYPE_WIFI) {
                // Since Android 4.2 the SSID can be retrieved from NetworkInfo.getExtraInfo().
                if (networkInfo.getExtraInfo() != null && !"".equals(networkInfo.getExtraInfo())) {
                    return new NetworkState(true, networkInfo.getType(), networkInfo.getSubtype(),
                            networkInfo.getExtraInfo(), false);
                }
                // Fetch WiFi SSID directly from WifiManagerDelegate if not in NetworkInfo.
                return new NetworkState(true, networkInfo.getType(), networkInfo.getSubtype(),
                        wifiManagerDelegate.getWifiSsid(), false);
            }
            return new NetworkState(
                    true, networkInfo.getType(), networkInfo.getSubtype(), null, false);
        }

        // Fetches NetworkInfo and records UMA for NullPointerExceptions.
        private NetworkInfo getNetworkInfo(Network network) {
            try {
                return mConnectivityManager.getNetworkInfo(network);
            } catch (NullPointerException firstException) {
                // Rarely this unexpectedly throws. Retry or just return {@code null} if it fails.
                try {
                    return mConnectivityManager.getNetworkInfo(network);
                } catch (NullPointerException secondException) {
                    return null;
                }
            }
        }

        /**
         * Returns connection type for |network|.
         * Only callable on Lollipop and newer releases.
         */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        @ConnectionType
        int getConnectionType(Network network) {
            NetworkInfo networkInfo = getNetworkInfo(network);
            if (networkInfo != null && networkInfo.getType() == TYPE_VPN) {
                // When a VPN is in place the underlying network type can be queried via
                // getActiveNeworkInfo() thanks to
                // https://android.googlesource.com/platform/frameworks/base/+/d6a7980d
                networkInfo = mConnectivityManager.getActiveNetworkInfo();
            }
            if (networkInfo != null && networkInfo.isConnected()) {
                return convertToConnectionType(networkInfo.getType(), networkInfo.getSubtype());
            }
            return ConnectionType.CONNECTION_NONE;
        }

        /**
         * Returns all connected networks. This may include networks that aren't useful
         * to Chrome (e.g. MMS, IMS, FOTA etc) or aren't accessible to Chrome (e.g. a VPN for
         * another user); use {@link getAllNetworks} for a filtered list.
         * Only callable on Lollipop and newer releases.
         */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        @VisibleForTesting
        protected Network[] getAllNetworksUnfiltered() {
            Network[] networks = mConnectivityManager.getAllNetworks();
            // Very rarely this API inexplicably returns {@code null}, crbug.com/721116.
            return networks == null ? new Network[0] : networks;
        }

        /**
         * Returns {@code true} if {@code network} applies to (and hence is accessible) to the
         * current user.
         */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        @VisibleForTesting
        protected boolean vpnAccessible(Network network) {
            // Determine if the VPN applies to the current user by seeing if a socket can be bound
            // to the VPN.
            Socket s = new Socket();
            try {
                // Avoid using network.getSocketFactory().createSocket() because it leaks.
                // https://crbug.com/805424
                network.bindSocket(s);
            } catch (IOException e) {
                // Failed to bind so this VPN isn't for the current user to use.
                return false;
            } finally {
                try {
                    s.close();
                } catch (IOException e) {
                    // Not worth taking action on a failed close.
                }
            }
            return true;
        }

        /**
         * Return the NetworkCapabilities for {@code network}, or {@code null} if they cannot
         * be retrieved (e.g. {@code network} has disconnected).
         */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        @VisibleForTesting
        protected NetworkCapabilities getNetworkCapabilities(Network network) {
            return mConnectivityManager.getNetworkCapabilities(network);
        }

        /**
         * Registers networkCallback to receive notifications about networks
         * that satisfy networkRequest.
         * Only callable on Lollipop and newer releases.
         */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        void registerNetworkCallback(
                NetworkRequest networkRequest, NetworkCallback networkCallback, Handler handler) {
            // Starting with Oreo specifying a Handler is allowed.  Use this to avoid thread-hops.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                mConnectivityManager.registerNetworkCallback(
                        networkRequest, networkCallback, handler);
            } else {
                mConnectivityManager.registerNetworkCallback(networkRequest, networkCallback);
            }
        }

        /**
         * Registers networkCallback to receive notifications about default network.
         * Only callable on P and newer releases.
         */
        @TargetApi(Build.VERSION_CODES.P)
        void registerDefaultNetworkCallback(NetworkCallback networkCallback, Handler handler) {
            mConnectivityManager.registerDefaultNetworkCallback(networkCallback, handler);
        }

        /**
         * Unregisters networkCallback from receiving notifications.
         * Only callable on Lollipop and newer releases.
         */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        void unregisterNetworkCallback(NetworkCallback networkCallback) {
            mConnectivityManager.unregisterNetworkCallback(networkCallback);
        }

        /**
         * Returns the current default {@link Network}, or {@code null} if disconnected.
         * Only callable on Lollipop and newer releases.
         */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        Network getDefaultNetwork() {
            Network defaultNetwork = null;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                defaultNetwork = ApiHelperForM.getActiveNetwork(mConnectivityManager);
                // getActiveNetwork() returning null cannot be trusted to indicate disconnected
                // as it suffers from https://crbug.com/677365.
                if (defaultNetwork != null) {
                    return defaultNetwork;
                }
            }
            // Android Lollipop had no API to get the default network; only an
            // API to return the NetworkInfo for the default network. To
            // determine the default network one can find the network with
            // type matching that of the default network.
            final NetworkInfo defaultNetworkInfo = mConnectivityManager.getActiveNetworkInfo();
            if (defaultNetworkInfo == null) {
                return null;
            }
            final Network[] networks = getAllNetworksFiltered(this, null);
            for (Network network : networks) {
                final NetworkInfo networkInfo = getNetworkInfo(network);
                if (networkInfo != null
                        && (networkInfo.getType() == defaultNetworkInfo.getType()
                                   // getActiveNetworkInfo() will not return TYPE_VPN types due to
                                   // https://android.googlesource.com/platform/frameworks/base/+/d6a7980d
                                   // so networkInfo.getType() can't be matched against
                                   // defaultNetworkInfo.getType() but networkInfo.getType() should
                                   // be TYPE_VPN. In the case of a VPN, getAllNetworks() will have
                                   // returned just this VPN if it applies.
                                   || networkInfo.getType() == TYPE_VPN)) {
                    // There should not be multiple connected networks of the
                    // same type. At least as of Android Marshmallow this is
                    // not supported. If this becomes supported this assertion
                    // may trigger.
                    assert defaultNetwork == null;
                    defaultNetwork = network;
                }
            }
            return defaultNetwork;
        }
    }

    /** Queries the WifiManager for SSID of the current Wifi connection. */
    static class WifiManagerDelegate {
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
        private WifiManager mWifiManager;

        WifiManagerDelegate(Context context) {
            // Getting SSID requires more permissions in later Android releases.
            assert Build.VERSION.SDK_INT < Build.VERSION_CODES.M;
            mContext = context;
        }

        // For testing.
        WifiManagerDelegate() {
            // All the methods below should be overridden.
            mContext = null;
        }

        // Lazily determine if app has ACCESS_WIFI_STATE permission.
        @GuardedBy("mLock")
        @SuppressLint("WifiManagerPotentialLeak")
        private boolean hasPermissionLocked() {
            if (mHasWifiPermissionComputed) {
                return mHasWifiPermission;
            }
            mHasWifiPermission = mContext.getPackageManager().checkPermission(
                                         permission.ACCESS_WIFI_STATE, mContext.getPackageName())
                    == PackageManager.PERMISSION_GRANTED;
            // TODO(crbug.com/635567): Fix lint properly.
            mWifiManager = mHasWifiPermission
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
        private WifiInfo getWifiInfoLocked() {
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
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private class DefaultNetworkCallback extends NetworkCallback {
        // If registered, notify connectionTypeChanged() to look for changes.
        @Override
        public void onAvailable(Network network) {
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

    // This class gets called back by ConnectivityManager whenever networks come
    // and go. It gets called back on a special handler thread
    // ConnectivityManager creates for making the callbacks. The callbacks in
    // turn post to mLooper where mObserver lives.
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private class MyNetworkCallback extends NetworkCallback {
        // If non-null, this indicates a VPN is in place for the current user, and no other
        // networks are accessible.
        private Network mVpnInPlace;

        // Initialize mVpnInPlace.
        void initializeVpnInPlace() {
            final Network[] networks = getAllNetworksFiltered(mConnectivityManagerDelegate, null);
            mVpnInPlace = null;
            // If the filtered list of networks contains just a VPN, then that VPN is in place.
            if (networks.length == 1) {
                final NetworkCapabilities capabilities =
                        mConnectivityManagerDelegate.getNetworkCapabilities(networks[0]);
                if (capabilities != null && capabilities.hasTransport(TRANSPORT_VPN)) {
                    mVpnInPlace = networks[0];
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
         * @param network Network to possibly consider ignoring changes to.
         * @param capabilities {@code NetworkCapabilities} for {@code network} if known, otherwise
         *         {@code null}.
         * @return {@code true} when either: {@code network} is an inaccessible VPN, or has already
         *         disconnected.
         */
        private boolean ignoreConnectedInaccessibleVpn(
                Network network, NetworkCapabilities capabilities) {
            // Fetch capabilities if not provided.
            if (capabilities == null) {
                capabilities = mConnectivityManagerDelegate.getNetworkCapabilities(network);
            }
            // Ignore inaccessible VPNs as they don't apply to Chrome.
            return capabilities == null
                    || capabilities.hasTransport(TRANSPORT_VPN)
                    && !mConnectivityManagerDelegate.vpnAccessible(network);
        }

        /**
         * Should changes to connected network {@code network} be ignored?
         * @param network Network to possible consider ignoring changes to.
         * @param capabilities {@code NetworkCapabilities} for {@code network} if known, otherwise
         *         {@code null}.
         */
        private boolean ignoreConnectedNetwork(Network network, NetworkCapabilities capabilities) {
            return ignoreNetworkDueToVpn(network)
                    || ignoreConnectedInaccessibleVpn(network, capabilities);
        }

        @Override
        public void onAvailable(Network network) {
            final NetworkCapabilities capabilities =
                    mConnectivityManagerDelegate.getNetworkCapabilities(network);
            if (ignoreConnectedNetwork(network, capabilities)) {
                return;
            }
            final boolean makeVpnDefault = capabilities.hasTransport(TRANSPORT_VPN);
            if (makeVpnDefault) {
                mVpnInPlace = network;
            }
            final long netId = networkToNetId(network);
            @ConnectionType
            final int connectionType = mConnectivityManagerDelegate.getConnectionType(network);
            runOnThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onNetworkConnect(netId, connectionType);
                    if (makeVpnDefault) {
                        // Make VPN the default network.
                        mObserver.onConnectionTypeChanged(connectionType);
                        // Purge all other networks as they're inaccessible to Chrome now.
                        mObserver.purgeActiveNetworkList(new long[] {netId});
                    }
                }
            });
        }

        @Override
        public void onCapabilitiesChanged(
                Network network, NetworkCapabilities networkCapabilities) {
            if (ignoreConnectedNetwork(network, networkCapabilities)) {
                return;
            }
            // A capabilities change may indicate the ConnectionType has changed,
            // so forward the new ConnectionType along to observer.
            final long netId = networkToNetId(network);
            final int connectionType = mConnectivityManagerDelegate.getConnectionType(network);
            runOnThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onNetworkConnect(netId, connectionType);
                }
            });
        }

        @Override
        public void onLosing(Network network, int maxMsToLive) {
            if (ignoreConnectedNetwork(network, null)) {
                return;
            }
            final long netId = networkToNetId(network);
            runOnThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onNetworkSoonToDisconnect(netId);
                }
            });
        }

        @Override
        public void onLost(final Network network) {
            if (ignoreNetworkDueToVpn(network)) {
                return;
            }
            runOnThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onNetworkDisconnect(networkToNetId(network));
                }
            });
            // If the VPN is going away, inform observer that other networks that were previously
            // hidden by ignoreNetworkDueToVpn() are now available for use, now that this user's
            // traffic is not forced into the VPN.
            if (mVpnInPlace != null) {
                assert network.equals(mVpnInPlace);
                mVpnInPlace = null;
                for (Network newNetwork :
                        getAllNetworksFiltered(mConnectivityManagerDelegate, network)) {
                    onAvailable(newNetwork);
                }
                @ConnectionType
                final int newConnectionType = getCurrentNetworkState().getConnectionType();
                runOnThread(new Runnable() {
                    @Override
                    public void run() {
                        mObserver.onConnectionTypeChanged(newConnectionType);
                    }
                });
            }
        }
    }

    /**
     * Abstract class for providing a policy regarding when the NetworkChangeNotifier
     * should listen for network changes.
     */
    public abstract static class RegistrationPolicy {
        private NetworkChangeNotifierAutoDetect mNotifier;

        /**
         * Start listening for network changes.
         */
        protected final void register() {
            assert mNotifier != null;
            mNotifier.register();
        }

        /**
         * Stop listening for network changes.
         */
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
    private static final int UNKNOWN_LINK_SPEED = -1;

    // {@link Looper} for the thread this object lives on.
    private final Looper mLooper;
    // Used to post to the thread this object lives on.
    private final Handler mHandler;
    // {@link IntentFilter} for incoming global broadcast {@link Intent}s this object listens for.
    private final NetworkConnectivityIntentFilter mIntentFilter;
    // Notifications are sent to this {@link Observer}.
    private final Observer mObserver;
    private final RegistrationPolicy mRegistrationPolicy;
    // Starting with Android Pie, used to detect changes in default network.
    private final DefaultNetworkCallback mDefaultNetworkCallback;

    // mConnectivityManagerDelegates and mWifiManagerDelegate are only non-final for testing.
    private ConnectivityManagerDelegate mConnectivityManagerDelegate;
    private WifiManagerDelegate mWifiManagerDelegate;
    // mNetworkCallback and mNetworkRequest are only non-null in Android L and above.
    // mNetworkCallback will be null if ConnectivityManager.registerNetworkCallback() ever fails.
    private MyNetworkCallback mNetworkCallback;
    private NetworkRequest mNetworkRequest;
    private boolean mRegistered;
    private NetworkState mNetworkState;
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

    /**
     * Observer interface by which observer is notified of network changes.
     */
    public static interface Observer {
        /**
         * Called when default network changes.
         */
        public void onConnectionTypeChanged(@ConnectionType int newConnectionType);
        /**
         * Called when connection subtype of default network changes.
         */
        public void onConnectionSubtypeChanged(int newConnectionSubtype);
        /**
         * Called when device connects to network with NetID netId. For
         * example device associates with a WiFi access point.
         * connectionType is the type of the network; a member of
         * ConnectionType. Only called on Android L and above.
         */
        public void onNetworkConnect(long netId, int connectionType);
        /**
         * Called when device determines the connection to the network with
         * NetID netId is no longer preferred, for example when a device
         * transitions from cellular to WiFi it might deem the cellular
         * connection no longer preferred. The device will disconnect from
         * the network in 30s allowing network communications on that network
         * to wrap up. Only called on Android L and above.
         */
        public void onNetworkSoonToDisconnect(long netId);
        /**
         * Called when device disconnects from network with NetID netId.
         * Only called on Android L and above.
         */
        public void onNetworkDisconnect(long netId);
        /**
         * Called to cause a purge of cached lists of active networks, of any
         * networks not in the accompanying list of active networks. This is
         * issued if a period elapsed where disconnected notifications may have
         * been missed, and acts to keep cached lists of active networks
         * accurate. Only called on Android L and above.
         */
        public void purgeActiveNetworkList(long[] activeNetIds);
    }

    /**
     * Constructs a NetworkChangeNotifierAutoDetect.  Lives on calling thread, receives broadcast
     * notifications on the UI thread and forwards the notifications to be processed on the calling
     * thread.
     * @param policy The RegistrationPolicy which determines when this class should watch
     *     for network changes (e.g. see (@link RegistrationPolicyAlwaysRegister} and
     *     {@link RegistrationPolicyApplicationStatus}).
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public NetworkChangeNotifierAutoDetect(Observer observer, RegistrationPolicy policy) {
        mLooper = Looper.myLooper();
        mHandler = new Handler(mLooper);
        mObserver = observer;
        mConnectivityManagerDelegate =
                new ConnectivityManagerDelegate(ContextUtils.getApplicationContext());
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            mWifiManagerDelegate = new WifiManagerDelegate(ContextUtils.getApplicationContext());
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            mNetworkCallback = new MyNetworkCallback();
            mNetworkRequest = new NetworkRequest.Builder()
                                      .addCapability(NET_CAPABILITY_INTERNET)
                                      // Need to hear about VPNs too.
                                      .removeCapability(NET_CAPABILITY_NOT_VPN)
                                      .build();
        } else {
            mNetworkCallback = null;
            mNetworkRequest = null;
        }
        mDefaultNetworkCallback = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                ? new DefaultNetworkCallback()
                : null;
        mNetworkState = getCurrentNetworkState();
        mIntentFilter = new NetworkConnectivityIntentFilter();
        mIgnoreNextBroadcast = false;
        mShouldSignalObserver = false;
        mRegistrationPolicy = policy;
        mRegistrationPolicy.init(this);
        mShouldSignalObserver = true;
    }

    private boolean onThread() {
        return mLooper == Looper.myLooper();
    }

    private void assertOnThread() {
        if (BuildConfig.DCHECK_IS_ON && !onThread()) {
            throw new IllegalStateException(
                    "Must be called on NetworkChangeNotifierAutoDetect thread.");
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
     * Allows overriding the ConnectivityManagerDelegate for tests.
     */
    void setConnectivityManagerDelegateForTests(ConnectivityManagerDelegate delegate) {
        mConnectivityManagerDelegate = delegate;
    }

    /**
     * Allows overriding the WifiManagerDelegate for tests.
     */
    void setWifiManagerDelegateForTests(WifiManagerDelegate delegate) {
        mWifiManagerDelegate = delegate;
    }

    @VisibleForTesting
    RegistrationPolicy getRegistrationPolicy() {
        return mRegistrationPolicy;
    }

    /**
     * Returns whether the object has registered to receive network connectivity intents.
     */
    @VisibleForTesting
    boolean isReceiverRegisteredForTesting() {
        return mRegistered;
    }

    public void destroy() {
        assertOnThread();
        mRegistrationPolicy.destroy();
        unregister();
    }

    /**
     * Registers a BroadcastReceiver in the given context.
     */
    public void register() {
        assertOnThread();
        if (mRegistered) return;

        if (mShouldSignalObserver) {
            connectionTypeChanged();
        }
        if (mDefaultNetworkCallback != null) {
            mConnectivityManagerDelegate.registerDefaultNetworkCallback(
                    mDefaultNetworkCallback, mHandler);
        } else {
            // When registering for a sticky broadcast, like CONNECTIVITY_ACTION, if
            // registerReceiver returns non-null, it means the broadcast was previously issued and
            // onReceive() will be immediately called with this previous Intent. Since this initial
            // callback doesn't actually indicate a network change, we can ignore it by setting
            // mIgnoreNextBroadcast.
            mIgnoreNextBroadcast =
                    ContextUtils.getApplicationContext().registerReceiver(this, mIntentFilter)
                    != null;
        }
        mRegistered = true;

        if (mNetworkCallback != null) {
            mNetworkCallback.initializeVpnInPlace();
            try {
                mConnectivityManagerDelegate.registerNetworkCallback(
                        mNetworkRequest, mNetworkCallback, mHandler);
            } catch (IllegalArgumentException e) {
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
                // will be repopulated immediatly after this. However we need to
                // purge any cached networks as they may have been disconnected
                // while mNetworkCallback was unregistered.
                final Network[] networks =
                        getAllNetworksFiltered(mConnectivityManagerDelegate, null);
                // Convert Networks to NetIDs.
                final long[] netIds = new long[networks.length];
                for (int i = 0; i < networks.length; i++) {
                    netIds[i] = networkToNetId(networks[i]);
                }
                mObserver.purgeActiveNetworkList(netIds);
            }
        }
    }

    /**
     * Unregisters a BroadcastReceiver in the given context.
     */
    public void unregister() {
        assertOnThread();
        if (!mRegistered) return;
        mRegistered = false;
        if (mNetworkCallback != null) {
            mConnectivityManagerDelegate.unregisterNetworkCallback(mNetworkCallback);
        }
        if (mDefaultNetworkCallback != null) {
            mConnectivityManagerDelegate.unregisterNetworkCallback(mDefaultNetworkCallback);
        } else {
            ContextUtils.getApplicationContext().unregisterReceiver(this);
        }
    }

    public NetworkState getCurrentNetworkState() {
        return mConnectivityManagerDelegate.getNetworkState(mWifiManagerDelegate);
    }

    /**
     * Returns all connected networks that are useful and accessible to Chrome.
     * Only callable on Lollipop and newer releases.
     * @param ignoreNetwork ignore this network as if it is not connected.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static Network[] getAllNetworksFiltered(
            ConnectivityManagerDelegate connectivityManagerDelegate, Network ignoreNetwork) {
        Network[] networks = connectivityManagerDelegate.getAllNetworksUnfiltered();
        // Whittle down |networks| into just the list of networks useful to us.
        int filteredIndex = 0;
        for (Network network : networks) {
            if (network.equals(ignoreNetwork)) {
                continue;
            }
            final NetworkCapabilities capabilities =
                    connectivityManagerDelegate.getNetworkCapabilities(network);
            if (capabilities == null || !capabilities.hasCapability(NET_CAPABILITY_INTERNET)) {
                continue;
            }
            if (capabilities.hasTransport(TRANSPORT_VPN)) {
                // If we can access the VPN then...
                if (connectivityManagerDelegate.vpnAccessible(network)) {
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

    /**
     * Returns an array of all of the device's currently connected
     * networks and ConnectionTypes, including only those that are useful and accessible to Chrome.
     * Array elements are a repeated sequence of:
     *   NetID of network
     *   ConnectionType of network
     * Only available on Lollipop and newer releases and when auto-detection has
     * been enabled.
     */
    public long[] getNetworksAndTypes() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return new long[0];
        }
        final Network networks[] = getAllNetworksFiltered(mConnectivityManagerDelegate, null);
        final long networksAndTypes[] = new long[networks.length * 2];
        int index = 0;
        for (Network network : networks) {
            networksAndTypes[index++] = networkToNetId(network);
            networksAndTypes[index++] = mConnectivityManagerDelegate.getConnectionType(network);
        }
        return networksAndTypes;
    }

    /**
     * Returns NetID of device's current default connected network used for
     * communication.
     * Only implemented on Lollipop and newer releases, returns NetId.INVALID
     * when not implemented.
     */
    public long getDefaultNetId() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return NetId.INVALID;
        }
        Network network = mConnectivityManagerDelegate.getDefaultNetwork();
        return network == null ? NetId.INVALID : networkToNetId(network);
    }

    /**
     * Returns {@code true} if NetworkCallback failed to register, indicating that network-specific
     * callbacks will not be issued.
     */
    public boolean registerNetworkCallbackFailed() {
        return mRegisterNetworkCallbackFailed;
    }

    /**
     * Returns the connection type for the given ConnectivityManager type and subtype.
     */
    @ConnectionType
    private static int convertToConnectionType(int type, int subtype) {
        switch (type) {
            case ConnectivityManager.TYPE_ETHERNET:
                return ConnectionType.CONNECTION_ETHERNET;
            case ConnectivityManager.TYPE_WIFI:
                return ConnectionType.CONNECTION_WIFI;
            case ConnectivityManager.TYPE_WIMAX:
                return ConnectionType.CONNECTION_4G;
            case ConnectivityManager.TYPE_BLUETOOTH:
                return ConnectionType.CONNECTION_BLUETOOTH;
            case ConnectivityManager.TYPE_MOBILE:
                // Use information from TelephonyManager to classify the connection.
                switch (subtype) {
                    case TelephonyManager.NETWORK_TYPE_GPRS:
                    case TelephonyManager.NETWORK_TYPE_EDGE:
                    case TelephonyManager.NETWORK_TYPE_CDMA:
                    case TelephonyManager.NETWORK_TYPE_1xRTT:
                    case TelephonyManager.NETWORK_TYPE_IDEN:
                        return ConnectionType.CONNECTION_2G;
                    case TelephonyManager.NETWORK_TYPE_UMTS:
                    case TelephonyManager.NETWORK_TYPE_EVDO_0:
                    case TelephonyManager.NETWORK_TYPE_EVDO_A:
                    case TelephonyManager.NETWORK_TYPE_HSDPA:
                    case TelephonyManager.NETWORK_TYPE_HSUPA:
                    case TelephonyManager.NETWORK_TYPE_HSPA:
                    case TelephonyManager.NETWORK_TYPE_EVDO_B:
                    case TelephonyManager.NETWORK_TYPE_EHRPD:
                    case TelephonyManager.NETWORK_TYPE_HSPAP:
                        return ConnectionType.CONNECTION_3G;
                    case TelephonyManager.NETWORK_TYPE_LTE:
                        return ConnectionType.CONNECTION_4G;
                    default:
                        return ConnectionType.CONNECTION_UNKNOWN;
                }
            default:
                return ConnectionType.CONNECTION_UNKNOWN;
        }
    }

    // BroadcastReceiver
    @Override
    public void onReceive(Context context, Intent intent) {
        runOnThread(new Runnable() {
            @Override
            public void run() {
                // Once execution begins on the correct thread, make sure unregister() hasn't
                // been called in the mean time. Ignore the broadcast if unregister() was called.
                if (!mRegistered) {
                    return;
                }
                if (mIgnoreNextBroadcast) {
                    mIgnoreNextBroadcast = false;
                    return;
                }
                connectionTypeChanged();
            }
        });
    }

    private void connectionTypeChanged() {
        NetworkState networkState = getCurrentNetworkState();
        if (networkState.getConnectionType() != mNetworkState.getConnectionType()
                || !networkState.getNetworkIdentifier().equals(mNetworkState.getNetworkIdentifier())
                || networkState.isPrivateDnsActive() != mNetworkState.isPrivateDnsActive()) {
            mObserver.onConnectionTypeChanged(networkState.getConnectionType());
        }
        if (networkState.getConnectionType() != mNetworkState.getConnectionType()
                || networkState.getConnectionSubtype() != mNetworkState.getConnectionSubtype()) {
            mObserver.onConnectionSubtypeChanged(networkState.getConnectionSubtype());
        }
        mNetworkState = networkState;
    }

    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint({"NewApi", "ParcelCreator"})
    private static class NetworkConnectivityIntentFilter extends IntentFilter {
        NetworkConnectivityIntentFilter() {
            addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        }
    }

    /**
     * Extracts NetID of Network on Lollipop and NetworkHandle (which is munged NetID) on
     * Marshmallow and newer releases. Only available on Lollipop and newer releases.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @VisibleForTesting
    static long networkToNetId(Network network) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return ApiHelperForM.getNetworkHandle(network);
        } else {
            // NOTE(pauljensen): This depends on Android framework implementation details. These
            // details cannot change because Lollipop is long since released.
            // NetIDs are only 16-bit so use parseInt. This function returns a long because
            // getNetworkHandle() returns a long.
            return Integer.parseInt(network.toString());
        }
    }
}
