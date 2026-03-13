// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.net.ConnectivityManager.TYPE_VPN;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.os.Build;
import android.os.Handler;
import android.telephony.TelephonyManager;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.net.Socket;

/**
 * Adds convenience methods for interacting with ConnectivityManager in production code and in
 * testing.
 */
@NullMarked
public class ConnectivityManagerWrapper {
    private static final String TAG = "ConnectivityManagerW";

    @SuppressWarnings("NullAway.Init") // Due to test-only constructor.
    private final ConnectivityManager mConnectivityManager;

    ConnectivityManagerWrapper(Context context) {
        mConnectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    @VisibleForTesting
    @SuppressWarnings("NullAway")
    public ConnectivityManagerWrapper() {
        // Tests must mock all methods.
        mConnectivityManager = null;
    }

    /**
     * @param networkInfo The NetworkInfo for the active network.
     * @return the info of the network that is available to this app.
     */
    private @Nullable NetworkInfo processActiveNetworkInfo(@Nullable NetworkInfo networkInfo) {
        if (networkInfo == null) {
            return null;
        }

        if (networkInfo.isConnected()) {
            return networkInfo;
        }

        // If |networkInfo| is BLOCKED, but the app is in the foreground, then it's likely that
        // Android hasn't finished updating the network access permissions as BLOCKED is only
        // meant for apps in the background.  See https://crbug.com/677365 for more details.

        if (networkInfo.getDetailedState() != NetworkInfo.DetailedState.BLOCKED) {
            // Network state is not blocked which implies that network access is
            // unavailable (not just blocked to this app).
            return null;
        }

        if (ApplicationStatus.getStateForApplication() != ApplicationState.HAS_RUNNING_ACTIVITIES) {
            // The app is not in the foreground.
            return null;
        }
        return networkInfo;
    }

    /** Returns connection type and status information about the current default network. */
    NetworkState getNetworkState(
            NetworkChangeNotifierAutoDetect.@Nullable WifiManagerDelegate wifiManagerDelegate) {
        Network network = null;
        NetworkInfo networkInfo;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            network = getDefaultNetwork();
            networkInfo = getNetworkInfo(network);
        } else {
            networkInfo = mConnectivityManager.getActiveNetworkInfo();
        }
        networkInfo = processActiveNetworkInfo(networkInfo);
        if (networkInfo == null) {
            return new NetworkState(false, -1, -1, false, null, false, "");
        }

        if (network != null) {
            final NetworkCapabilitiesWrapper capabilities = getNetworkCapabilities(network);
            boolean isMetered =
                    (capabilities != null
                            && !capabilities.hasCapability(
                                    NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
            DnsStatus dnsStatus = AndroidNetworkLibrary.getDnsStatus(network);
            if (dnsStatus == null) {
                return new NetworkState(
                        true,
                        networkInfo.getType(),
                        networkInfo.getSubtype(),
                        isMetered,
                        String.valueOf(networkToNetId(network)),
                        false,
                        "");
            } else {
                return new NetworkState(
                        true,
                        networkInfo.getType(),
                        networkInfo.getSubtype(),
                        isMetered,
                        String.valueOf(networkToNetId(network)),
                        dnsStatus.getPrivateDnsActive(),
                        dnsStatus.getPrivateDnsServerName());
            }
        }
        assert Build.VERSION.SDK_INT < Build.VERSION_CODES.M;
        // If Wifi, then fetch SSID also
        if (networkInfo.getType() == ConnectivityManager.TYPE_WIFI) {
            // Since Android 4.2 the SSID can be retrieved from NetworkInfo.getExtraInfo().
            if (networkInfo.getExtraInfo() != null && !"".equals(networkInfo.getExtraInfo())) {
                return new NetworkState(
                        true,
                        networkInfo.getType(),
                        networkInfo.getSubtype(),
                        false,
                        networkInfo.getExtraInfo(),
                        false,
                        "");
            }
            // Fetch WiFi SSID directly from WifiManagerDelegate if not in NetworkInfo.
            return new NetworkState(
                    true,
                    networkInfo.getType(),
                    networkInfo.getSubtype(),
                    false,
                    assumeNonNull(wifiManagerDelegate).getWifiSsid(),
                    false,
                    "");
        }
        return new NetworkState(
                true, networkInfo.getType(), networkInfo.getSubtype(), false, null, false, "");
    }

    /**
     * Fetches NetworkInfo for |network|. Does not account for underlying VPNs; see
     * getNetworkInfo(Network) for a method that does.
     */
    @Nullable NetworkInfo getRawNetworkInfo(@Nullable Network network) {
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

    /** Fetches NetworkInfo for |network|. */
    @Nullable NetworkInfo getNetworkInfo(@Nullable Network network) {
        NetworkInfo networkInfo = getRawNetworkInfo(network);
        if (networkInfo != null && networkInfo.getType() == TYPE_VPN) {
            // When a VPN is in place the underlying network type can be queried via
            // getActiveNetworkInfo() thanks to
            // https://android.googlesource.com/platform/frameworks/base/+/d6a7980d
            networkInfo = mConnectivityManager.getActiveNetworkInfo();
        }
        return networkInfo;
    }

    /** Returns connection type for |network|. */
    @ConnectionType
    int getConnectionType(Network network) {
        NetworkInfo networkInfo = getNetworkInfo(network);
        if (networkInfo != null && networkInfo.isConnected()) {
            return convertToConnectionType(networkInfo.getType(), networkInfo.getSubtype());
        }
        return ConnectionType.CONNECTION_NONE;
    }

    /**
     * Returns all connected networks. This may include networks that aren't useful to Chrome (e.g.
     * MMS, IMS, FOTA etc) or aren't accessible to Chrome (e.g. a VPN for another user); use {@link
     * getAllNetworks} for a filtered list.
     */
    @VisibleForTesting
    protected Network[] getAllNetworksUnfiltered() {
        Network[] networks = mConnectivityManager.getAllNetworks();
        // Very rarely this API inexplicably returns {@code null}, crbug.com/721116.
        return networks == null ? new Network[0] : networks;
    }

    /**
     * Returns {@code true} if {@code network} applies to (and hence is accessible) to the current
     * user.
     */
    @VisibleForTesting
    protected boolean vpnAccessible(Network network) {
        // Determine if the VPN applies to the current user by seeing if a socket can be bound
        // to the VPN.
        Socket s = new Socket();
        // Disable detectUntaggedSockets StrictMode policy to avoid false positives, as |s|
        // isn't used to send or receive traffic. https://crbug.com/946531
        try (StrictModeContext ignored = StrictModeContext.allowAllVmPolicies()) {
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
     * Return the NetworkCapabilities for {@code network}, or {@code null} if they cannot be
     * retrieved (e.g. {@code network} has disconnected).
     */
    @VisibleForTesting
    protected @Nullable NetworkCapabilitiesWrapper getNetworkCapabilities(Network network) {
        final int retryCount = 2;
        for (int i = 0; i < retryCount; ++i) {
            // This try-catch is a workaround for https://crbug.com/1218536. We ignore
            // the exception intentionally.
            try {
                return new NetworkCapabilitiesWrapper(
                        mConnectivityManager.getNetworkCapabilities(network));
            } catch (SecurityException e) {
                // Do nothing.
            }
        }
        return null;
    }

    /**
     * Registers networkCallback to receive notifications about networks that satisfy
     * networkRequest.
     */
    void registerNetworkCallback(
            NetworkRequest networkRequest, NetworkCallback networkCallback, Handler handler) {
        // Starting with Oreo specifying a Handler is allowed.  Use this to avoid thread-hops.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                // Samsung Android O devices aggressively trigger StrictMode violations.
                // See https://crbug.com/1450175 for detail.
                mConnectivityManager.registerNetworkCallback(
                        networkRequest, networkCallback, handler);
            }
        } else {
            mConnectivityManager.registerNetworkCallback(networkRequest, networkCallback);
        }
    }

    /**
     * Registers networkCallback to receive notifications about default network. Only callable on P
     * and newer releases.
     */
    @RequiresApi(Build.VERSION_CODES.P)
    void registerDefaultNetworkCallback(NetworkCallback networkCallback, Handler handler) {
        mConnectivityManager.registerDefaultNetworkCallback(networkCallback, handler);
    }

    /** Unregisters networkCallback from receiving notifications. */
    void unregisterNetworkCallback(NetworkCallback networkCallback) {
        mConnectivityManager.unregisterNetworkCallback(networkCallback);
    }

    /** Returns the current default {@link Network}, or {@code null} if disconnected. */
    @Nullable Network getDefaultNetwork() {
        Network defaultNetwork = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            defaultNetwork = mConnectivityManager.getActiveNetwork();
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
        final Network[] networks =
                NetworkChangeNotifierAutoDetect.getAllNetworksFiltered(this, null);
        for (Network network : networks) {
            final NetworkInfo networkInfo = getRawNetworkInfo(network);
            if (networkInfo != null
                    && (networkInfo.getType() == defaultNetworkInfo.getType()
                            // getActiveNetworkInfo() will not return TYPE_VPN types due to
                            // https://android.googlesource.com/platform/frameworks/base/+/d6a7980d
                            // so networkInfo.getType() can't be matched against
                            // defaultNetworkInfo.getType() but networkInfo.getType() should
                            // be TYPE_VPN. In the case of a VPN, getAllNetworks() will have
                            // returned just this VPN if it applies.
                            || networkInfo.getType() == TYPE_VPN)) {
                // Android 10+ devices occasionally return multiple networks
                // of the same type that are stuck in the CONNECTING state.
                // Now that Java asserts are enabled, ignore these zombie
                // networks here to avoid hitting the assert below. crbug.com/1361170
                if (defaultNetwork != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    // If `network` is CONNECTING, ignore it.
                    if (networkInfo.getDetailedState() == NetworkInfo.DetailedState.CONNECTING) {
                        continue;
                    }
                    // If `defaultNetwork` is CONNECTING, ignore it.
                    NetworkInfo prevDefaultNetworkInfo = getRawNetworkInfo(defaultNetwork);
                    if (prevDefaultNetworkInfo != null
                            && prevDefaultNetworkInfo.getDetailedState()
                                    == NetworkInfo.DetailedState.CONNECTING) {
                        defaultNetwork = null;
                    }
                }
                if (defaultNetwork != null) {
                    // TODO(crbug.com/40060873): Investigate why there are multiple
                    // connected networks.
                    Log.e(
                            TAG,
                            "There should not be multiple connected "
                                    + "networks of the same type. At least as of Android "
                                    + "Marshmallow this is not supported. If this becomes "
                                    + "supported this assertion may trigger.");
                }
                defaultNetwork = network;
            }
        }
        return defaultNetwork;
    }

    /** Immutable class representing the state of a device's network. */
    public static class NetworkState {
        private final boolean mConnected;
        private final int mType;
        private final int mSubtype;
        private final boolean mIsMetered;
        // WIFI SSID of the connection on pre-Marshmallow, NetID starting with Marshmallow. Always
        // non-null (i.e. instead of null it'll be an empty string) to facilitate .equals().
        private final String mNetworkIdentifier;
        // Indicates if this network is using DNS-over-TLS.
        private final boolean mIsPrivateDnsActive;
        // Indicates the DNS-over-TLS server in use, if specified.
        private final String mPrivateDnsServerName;

        public NetworkState(
                boolean connected,
                int type,
                int subtype,
                boolean isMetered,
                @Nullable String networkIdentifier,
                boolean isPrivateDnsActive,
                @Nullable String privateDnsServerName) {
            mConnected = connected;
            mType = type;
            mSubtype = subtype;
            mIsMetered = isMetered;
            mNetworkIdentifier = networkIdentifier == null ? "" : networkIdentifier;
            mIsPrivateDnsActive = isPrivateDnsActive;
            mPrivateDnsServerName = privateDnsServerName == null ? "" : privateDnsServerName;
        }

        public boolean isConnected() {
            return mConnected;
        }

        public int getNetworkType() {
            return mType;
        }

        public boolean isMetered() {
            return mIsMetered;
        }

        public int getNetworkSubType() {
            return mSubtype;
        }

        // Always non-null to facilitate .equals().
        public String getNetworkIdentifier() {
            return mNetworkIdentifier;
        }

        /** Returns the connection type for the given NetworkState. */
        @ConnectionType
        public int getConnectionType() {
            if (!isConnected()) {
                return ConnectionType.CONNECTION_NONE;
            }
            return convertToConnectionType(getNetworkType(), getNetworkSubType());
        }

        /** Returns the connection cost for the given NetworkState. */
        @ConnectionCost
        public int getConnectionCost() {
            if (isMetered()) {
                return ConnectionCost.METERED;
            }
            return ConnectionCost.UNMETERED;
        }

        /** Returns the connection subtype for the given NetworkState. */
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
                case ConnectivityManager.TYPE_MOBILE_DUN:
                case ConnectivityManager.TYPE_MOBILE_HIPRI:
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

        /** Returns boolean indicating if this network uses DNS-over-TLS. */
        public boolean isPrivateDnsActive() {
            return mIsPrivateDnsActive;
        }

        /** Returns the DNS-over-TLS server in use, if specified. */
        public String getPrivateDnsServerName() {
            return mPrivateDnsServerName;
        }
    }

    /** Returns the connection type for the given ConnectivityManager type and subtype. */
    @ConnectionType
    static int convertToConnectionType(int type, int subtype) {
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
            case ConnectivityManager.TYPE_MOBILE_DUN:
            case ConnectivityManager.TYPE_MOBILE_HIPRI:
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
                    case TelephonyManager.NETWORK_TYPE_NR:
                        return ConnectionType.CONNECTION_5G;
                    default:
                        return ConnectionType.CONNECTION_UNKNOWN;
                }
            default:
                return ConnectionType.CONNECTION_UNKNOWN;
        }
    }

    /**
     * Extracts NetID of Network on Lollipop and NetworkHandle (which is munged NetID) on
     * Marshmallow and newer releases. TODO(crbug.com/40283930): Rename networkToNetId to something
     * meaningful and update javadoc comment.
     */
    static long networkToNetId(Network network) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return network.getNetworkHandle();
        } else {
            // NOTE(pauljensen): This depends on Android framework implementation details. These
            // details cannot change because Lollipop is long since released.
            // NetIDs are only 16-bit so use parseInt. This function returns a long because
            // getNetworkHandle() returns a long.
            return Integer.parseInt(network.toString());
        }
    }
}
