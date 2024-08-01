// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.TrafficStats;
import android.net.TransportInfo;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.security.NetworkSecurityPolicy;
import android.telephony.TelephonyManager;
import android.util.Log;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.CalledByNativeUnchecked;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.Socket;
import java.net.SocketAddress;
import java.net.SocketException;
import java.net.SocketImpl;
import java.net.URLConnection;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.util.Enumeration;
import java.util.List;

/** This class implements net utilities required by the net component. */
class AndroidNetworkLibrary {
    private static final String TAG = "AndroidNetworkLibrary";

    // Cached value indicating if app has ACCESS_NETWORK_STATE permission.
    private static Boolean sHaveAccessNetworkState;
    // Cached value indicating if app has ACCESS_WIFI_STATE permission.
    private static Boolean sHaveAccessWifiState;

    /**
     * @return the mime type (if any) that is associated with the file
     *         extension. Returns null if no corresponding mime type exists.
     */
    @CalledByNative
    public static String getMimeTypeFromExtension(String extension) {
        return URLConnection.guessContentTypeFromName("foo." + extension);
    }

    /**
     * @return true if it can determine that only loopback addresses are
     *         configured. i.e. if only 127.0.0.1 and ::1 are routable. Also
     *         returns false if it cannot determine this.
     */
    @CalledByNative
    public static boolean haveOnlyLoopbackAddresses() {
        Enumeration<NetworkInterface> list = null;
        try {
            list = NetworkInterface.getNetworkInterfaces();
            if (list == null) return false;
        } catch (Exception e) {
            Log.w(TAG, "could not get network interfaces: " + e);
            return false;
        }

        while (list.hasMoreElements()) {
            NetworkInterface netIf = list.nextElement();
            try {
                if (netIf.isUp() && !netIf.isLoopback()) return false;
            } catch (SocketException e) {
                continue;
            }
        }
        return true;
    }

    /**
     * Validate the server's certificate chain is trusted. Note that the caller
     * must still verify the name matches that of the leaf certificate.
     *
     * @param certChain The ASN.1 DER encoded bytes for certificates.
     * @param authType The key exchange algorithm name (e.g. RSA).
     * @param host The hostname of the server.
     * @return Android certificate verification result code.
     */
    @CalledByNative
    public static AndroidCertVerifyResult verifyServerCertificates(
            byte[][] certChain, String authType, String host) {
        try {
            return X509Util.verifyServerCertificates(certChain, authType, host);
        } catch (KeyStoreException e) {
            return new AndroidCertVerifyResult(CertVerifyStatusAndroid.FAILED);
        } catch (NoSuchAlgorithmException e) {
            return new AndroidCertVerifyResult(CertVerifyStatusAndroid.FAILED);
        } catch (IllegalArgumentException e) {
            return new AndroidCertVerifyResult(CertVerifyStatusAndroid.FAILED);
        }
    }

    /**
     * Get the list of user-added roots.
     *
     * @return DER-encoded list of user-added roots.
     */
    @CalledByNative
    public static byte[][] getUserAddedRoots() {
        return X509Util.getUserAddedRoots();
    }

    /**
     * Adds a test root certificate to the local trust store.
     * @param rootCert DER encoded bytes of the certificate.
     */
    @CalledByNativeUnchecked
    public static void addTestRootCertificate(byte[] rootCert)
            throws CertificateException, KeyStoreException, NoSuchAlgorithmException {
        X509Util.addTestRootCertificate(rootCert);
    }

    /**
     * Removes all test root certificates added by |addTestRootCertificate| calls from the local
     * trust store.
     */
    @CalledByNativeUnchecked
    public static void clearTestRootCertificates()
            throws NoSuchAlgorithmException, CertificateException, KeyStoreException {
        X509Util.clearTestRootCertificates();
    }

    /**
     * Returns the MCC+MNC (mobile country code + mobile network code) as
     * the numeric name of the current registered operator. This function
     * potentially blocks the thread, so use with care.
     */
    @CalledByNative
    private static String getNetworkOperator() {
        TelephonyManager telephonyManager =
                (TelephonyManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.TELEPHONY_SERVICE);
        if (telephonyManager == null) return "";
        return telephonyManager.getNetworkOperator();
    }

    /**
     * Indicates whether the device is roaming on the currently active network. When true, it
     * suggests that use of data may incur extra costs.
     */
    @CalledByNative
    private static boolean getIsRoaming() {
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo networkInfo = connectivityManager.getActiveNetworkInfo();
        if (networkInfo == null) return false; // No active network.
        return networkInfo.isRoaming();
    }

    /**
     * Returns true if the system's captive portal probe was blocked for the current default data
     * network. The method will return false if the captive portal probe was not blocked, the login
     * process to the captive portal has been successfully completed, or if the captive portal
     * status can't be determined. Requires ACCESS_NETWORK_STATE permission. Only available on
     * Android Marshmallow and later versions. Returns false on earlier versions.
     */
    @RequiresApi(Build.VERSION_CODES.M)
    @CalledByNative
    private static boolean getIsCaptivePortal() {
        // NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL is only available on Marshmallow and
        // later versions.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connectivityManager == null) return false;

        Network network = connectivityManager.getActiveNetwork();
        if (network == null) return false;

        NetworkCapabilities capabilities = connectivityManager.getNetworkCapabilities(network);
        return capabilities != null
                && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL);
    }

    /**
     * Helper function that gets the WifiInfo of the WiFi network. If we have permission to access
     * to the WiFi state, then we use either {@link NetworkCapabilities} for Android S+ or {@link
     * WifiManager} for earlier versions. Otherwise, we try to get the WifiInfo via broadcast (Note
     * that this approach does not work on Android P and above).
     */
    private static WifiInfo getWifiInfo() {
        if (haveAccessWifiState()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                // On Android S+, need to use NetworkCapabilities to get the WifiInfo.
                ConnectivityManager connectivityManager =
                        (ConnectivityManager)
                                ContextUtils.getApplicationContext()
                                        .getSystemService(Context.CONNECTIVITY_SERVICE);
                Network[] allNetworks = connectivityManager.getAllNetworks();
                // TODO(curranmax): This only gets the WifiInfo of the first WiFi network that is
                // iterated over. On Android S+ there may be up to two WiFi networks.
                // https://crbug.com/1181393
                for (Network network : allNetworks) {
                    NetworkCapabilities networkCapabilities =
                            connectivityManager.getNetworkCapabilities(network);
                    if (networkCapabilities != null
                            && networkCapabilities.hasTransport(
                                    NetworkCapabilities.TRANSPORT_WIFI)) {
                        TransportInfo transportInfo = networkCapabilities.getTransportInfo();
                        if (transportInfo != null && transportInfo instanceof WifiInfo) {
                            return (WifiInfo) transportInfo;
                        }
                    }
                }
                return null;
            } else {
                // Get WifiInfo via WifiManager. This method is deprecated starting with Android S.
                WifiManager wifiManager =
                        (WifiManager)
                                ContextUtils.getApplicationContext()
                                        .getSystemService(Context.WIFI_SERVICE);
                return wifiManager.getConnectionInfo();
            }
        } else {
            // If we do not have permission to access the WiFi state, then try to get the WifiInfo
            // through broadcast. Note that this approach does not work on Android P+.
            final Intent intent =
                    ContextUtils.registerProtectedBroadcastReceiver(
                            ContextUtils.getApplicationContext(),
                            null,
                            new IntentFilter(WifiManager.NETWORK_STATE_CHANGED_ACTION));
            if (intent != null) {
                return intent.getParcelableExtra(WifiManager.EXTRA_WIFI_INFO);
            }
            return null;
        }
    }

    /**
     * Gets the SSID of the currently associated WiFi access point if there is one, and it is
     * available. SSID may not be available if the app does not have permissions to access it. On
     * Android M+, the app accessing SSID needs to have ACCESS_COARSE_LOCATION or
     * ACCESS_FINE_LOCATION. If there is no WiFi access point or its SSID is unavailable, an empty
     * string is returned.
     */
    @CalledByNative
    public static String getWifiSSID() {
        WifiInfo wifiInfo = getWifiInfo();

        if (wifiInfo != null) {
            final String ssid = wifiInfo.getSSID();
            // On Android M+, the platform APIs may return "<unknown ssid>" as the SSID if the
            // app does not have sufficient permissions. In that case, return an empty string.
            if (ssid != null && !ssid.equals("<unknown ssid>")) {
                return ssid;
            }
        }
        return "";
    }

    @CalledByNativeForTesting
    public static void setWifiEnabledForTesting(boolean enabled) {
        WifiManager wifiManager =
                (WifiManager)
                        ContextUtils.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        var oldValue = wifiManager.isWifiEnabled();
        wifiManager.setWifiEnabled(enabled);
        ResettersForTesting.register(() -> wifiManager.setWifiEnabled(oldValue));
    }

    /**
     * Gets the signal strength from the currently associated WiFi access point if there is one, and
     * it is available. Signal strength may not be available if the app does not have permissions to
     * access it.
     * @return -1 if value is unavailable, otherwise, a value between 0 and {@code countBuckets-1}
     *         (both inclusive).
     */
    @CalledByNative
    public static int getWifiSignalLevel(int countBuckets) {
        // Some devices unexpectedly have a null context. See https://crbug.com/1019974.
        if (ContextUtils.getApplicationContext() == null) {
            return -1;
        }
        if (ContextUtils.getApplicationContext().getContentResolver() == null) {
            return -1;
        }

        int rssi;
        // On Android Q and above, the WifiInfo cannot be obtained through broadcast. See
        // https://crbug.com/1026686.
        if (haveAccessWifiState()) {
            WifiInfo wifiInfo = getWifiInfo();
            if (wifiInfo == null) {
                return -1;
            }
            rssi = wifiInfo.getRssi();
        } else {
            Intent intent = null;
            try {
                intent =
                        ContextUtils.registerProtectedBroadcastReceiver(
                                ContextUtils.getApplicationContext(),
                                null,
                                new IntentFilter(WifiManager.RSSI_CHANGED_ACTION));
            } catch (IllegalArgumentException e) {
                // Some devices unexpectedly throw IllegalArgumentException when registering
                // the broadcast receiver. See https://crbug.com/984179.
                return -1;
            }
            if (intent == null) {
                return -1;
            }
            rssi = intent.getIntExtra(WifiManager.EXTRA_NEW_RSSI, Integer.MIN_VALUE);
        }

        if (rssi == Integer.MIN_VALUE) {
            return -1;
        }

        final int signalLevel = WifiManager.calculateSignalLevel(rssi, countBuckets);
        if (signalLevel < 0 || signalLevel >= countBuckets) {
            return -1;
        }

        return signalLevel;
    }

    public static class NetworkSecurityPolicyProxy {
        private static NetworkSecurityPolicyProxy sInstance = new NetworkSecurityPolicyProxy();

        public static NetworkSecurityPolicyProxy getInstance() {
            return sInstance;
        }

        public static void setInstanceForTesting(
                NetworkSecurityPolicyProxy networkSecurityPolicyProxy) {
            var oldValue = sInstance;
            sInstance = networkSecurityPolicyProxy;
            ResettersForTesting.register(() -> sInstance = oldValue);
        }

        @RequiresApi(Build.VERSION_CODES.N)
        public boolean isCleartextTrafficPermitted(String host) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
                // No per-host configuration before N.
                return isCleartextTrafficPermitted();
            }
            return NetworkSecurityPolicy.getInstance().isCleartextTrafficPermitted(host);
        }

        @RequiresApi(Build.VERSION_CODES.M)
        public boolean isCleartextTrafficPermitted() {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
                // Always true before M.
                return true;
            }
            return NetworkSecurityPolicy.getInstance().isCleartextTrafficPermitted();
        }
    }

    /** Returns true if cleartext traffic to |host| is allowed by the current app. */
    @CalledByNative
    @RequiresApi(Build.VERSION_CODES.N)
    private static boolean isCleartextPermitted(String host) {
        try {
            return NetworkSecurityPolicyProxy.getInstance().isCleartextTrafficPermitted(host);
        } catch (IllegalArgumentException e) {
            return NetworkSecurityPolicyProxy.getInstance().isCleartextTrafficPermitted();
        }
    }

    private static boolean haveAccessNetworkState() {
        // This could be racy if called on multiple threads, but races will
        // end in the same result so it's not a problem.
        if (sHaveAccessNetworkState == null) {
            sHaveAccessNetworkState =
                    Boolean.valueOf(
                            ApiCompatibilityUtils.checkPermission(
                                            ContextUtils.getApplicationContext(),
                                            Manifest.permission.ACCESS_NETWORK_STATE,
                                            Process.myPid(),
                                            Process.myUid())
                                    == PackageManager.PERMISSION_GRANTED);
        }
        return sHaveAccessNetworkState;
    }

    private static boolean haveAccessWifiState() {
        // This could be racy if called on multiple threads, but races will
        // end in the same result so it's not a problem.
        if (sHaveAccessWifiState == null) {
            sHaveAccessWifiState =
                    Boolean.valueOf(
                            ApiCompatibilityUtils.checkPermission(
                                            ContextUtils.getApplicationContext(),
                                            Manifest.permission.ACCESS_WIFI_STATE,
                                            Process.myPid(),
                                            Process.myUid())
                                    == PackageManager.PERMISSION_GRANTED);
        }
        return sHaveAccessWifiState;
    }

    /**
     * Returns object representing the DNS configuration for the provided
     * network handle.
     */
    @RequiresApi(Build.VERSION_CODES.P)
    @CalledByNative
    public static DnsStatus getDnsStatusForNetwork(long networkHandle) {
        // In case the network handle is invalid don't crash, instead return an empty DnsStatus and
        // let native code handle that.
        try {
            Network network = Network.fromNetworkHandle(networkHandle);
            return getDnsStatus(network);
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    /**
     * Returns object representing the DNS configuration for the current
     * default network.
     */
    @RequiresApi(Build.VERSION_CODES.M)
    @CalledByNative
    public static DnsStatus getCurrentDnsStatus() {
        return getDnsStatus(null);
    }

    /**
     * Returns object representing the DNS configuration for the provided
     * network. If |network| is null, uses the active network.
     */
    @RequiresApi(Build.VERSION_CODES.M)
    public static DnsStatus getDnsStatus(Network network) {
        if (!haveAccessNetworkState()) {
            return null;
        }
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connectivityManager == null) {
            return null;
        }
        if (network == null) {
            network = connectivityManager.getActiveNetwork();
        }
        if (network == null) {
            return null;
        }
        LinkProperties linkProperties;
        try {
            linkProperties = connectivityManager.getLinkProperties(network);
        } catch (RuntimeException e) {
            return null;
        }
        if (linkProperties == null) {
            return null;
        }
        List<InetAddress> dnsServersList = linkProperties.getDnsServers();
        String searchDomains = linkProperties.getDomains();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return new DnsStatus(
                    dnsServersList,
                    linkProperties.isPrivateDnsActive(),
                    linkProperties.getPrivateDnsServerName(),
                    searchDomains);
        } else {
            return new DnsStatus(dnsServersList, false, "", searchDomains);
        }
    }

    /** Reports a connectivity issue with the device's current default network. */
    @RequiresApi(Build.VERSION_CODES.M)
    @CalledByNative
    private static boolean reportBadDefaultNetwork() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connectivityManager == null) return false;

        connectivityManager.reportNetworkConnectivity(null, false);
        return true;
    }

    /**
     * Class to wrap FileDescriptor.setInt$() which is hidden and so must be accessed via
     * reflection.
     */
    private static class SetFileDescriptor {
        // Reference to FileDescriptor.setInt$(int fd).
        private static final Method sFileDescriptorSetInt;

        // Get reference to FileDescriptor.setInt$(int fd) via reflection.
        static {
            try {
                sFileDescriptorSetInt = FileDescriptor.class.getMethod("setInt$", Integer.TYPE);
            } catch (NoSuchMethodException | SecurityException e) {
                throw new RuntimeException("Unable to get FileDescriptor.setInt$", e);
            }
        }

        /** Creates a FileDescriptor and calls FileDescriptor.setInt$(int fd) on it. */
        public static FileDescriptor createWithFd(int fd) {
            try {
                FileDescriptor fileDescriptor = new FileDescriptor();
                sFileDescriptorSetInt.invoke(fileDescriptor, fd);
                return fileDescriptor;
            } catch (IllegalAccessException e) {
                throw new RuntimeException("FileDescriptor.setInt$() failed", e);
            } catch (InvocationTargetException e) {
                throw new RuntimeException("FileDescriptor.setInt$() failed", e);
            }
        }
    }

    /**
     * This class provides an implementation of {@link java.net.Socket} that serves only as a
     * conduit to pass a file descriptor integer to {@link android.net.TrafficStats#tagSocket}
     * when called by {@link #tagSocket}. This class does not take ownership of the file descriptor,
     * so calling {@link #close} will not actually close the file descriptor.
     */
    private static class SocketFd extends Socket {
        /**
         * This class provides an implementation of {@link java.net.SocketImpl} that serves only as
         * a conduit to pass a file descriptor integer to {@link android.net.TrafficStats#tagSocket}
         * when called by {@link #tagSocket}. This class does not take ownership of the file
         * descriptor, so calling {@link #close} will not actually close the file descriptor.
         */
        private static class SocketImplFd extends SocketImpl {
            /**
             * Create a {@link java.net.SocketImpl} that sets {@code fd} as the underlying file
             * descriptor. Does not take ownership of the file descriptor, so calling {@link #close}
             * will not actually close the file descriptor.
             */
            SocketImplFd(FileDescriptor fd) {
                this.fd = fd;
            }

            @Override
            protected void accept(SocketImpl s) {
                throw new RuntimeException("accept not implemented");
            }

            @Override
            protected int available() {
                throw new RuntimeException("accept not implemented");
            }

            @Override
            protected void bind(InetAddress host, int port) {
                throw new RuntimeException("accept not implemented");
            }

            @Override
            protected void close() {}

            @Override
            protected void connect(InetAddress address, int port) {
                throw new RuntimeException("connect not implemented");
            }

            @Override
            protected void connect(SocketAddress address, int timeout) {
                throw new RuntimeException("connect not implemented");
            }

            @Override
            protected void connect(String host, int port) {
                throw new RuntimeException("connect not implemented");
            }

            @Override
            protected void create(boolean stream) {}

            @Override
            protected InputStream getInputStream() {
                throw new RuntimeException("getInputStream not implemented");
            }

            @Override
            protected OutputStream getOutputStream() {
                throw new RuntimeException("getOutputStream not implemented");
            }

            @Override
            protected void listen(int backlog) {
                throw new RuntimeException("listen not implemented");
            }

            @Override
            protected void sendUrgentData(int data) {
                throw new RuntimeException("sendUrgentData not implemented");
            }

            @Override
            public Object getOption(int optID) {
                throw new RuntimeException("getOption not implemented");
            }

            @Override
            public void setOption(int optID, Object value) {
                throw new RuntimeException("setOption not implemented");
            }
        }

        /**
         * Create a {@link java.net.Socket} that sets {@code fd} as the underlying file
         * descriptor. Does not take ownership of the file descriptor, so calling {@link #close}
         * will not actually close the file descriptor.
         */
        SocketFd(FileDescriptor fd) throws IOException {
            super(new SocketImplFd(fd));
        }
    }

    /**
     * Tag socket referenced by {@code ifd} with {@code tag} for UID {@code uid}.
     *
     * Assumes thread UID tag isn't set upon entry, and ensures thread UID tag isn't set upon exit.
     * Unfortunately there is no TrafficStatis.getThreadStatsUid().
     */
    @CalledByNative
    private static void tagSocket(int ifd, int uid, int tag) throws IOException {
        // Set thread tags.
        int oldTag = TrafficStats.getThreadStatsTag();
        if (tag != oldTag) {
            TrafficStats.setThreadStatsTag(tag);
        }
        if (uid != TrafficStatsUid.UNSET) {
            ThreadStatsUid.set(uid);
        }

        // Apply thread tags to socket.

        // First, convert integer file descriptor (ifd) to FileDescriptor.
        final ParcelFileDescriptor pfd;
        final FileDescriptor fd;
        // The only supported way to generate a FileDescriptor from an integer file
        // descriptor is via ParcelFileDescriptor.adoptFd(). Unfortunately prior to Android
        // Marshmallow ParcelFileDescriptor.detachFd() didn't actually detach from the
        // FileDescriptor, so use reflection to set {@code fd} into the FileDescriptor for
        // versions prior to Marshmallow. Here's the fix that went into Marshmallow:
        // https://android.googlesource.com/platform/frameworks/base/+/b30ad6f
        if (Build.VERSION.SDK_INT < VERSION_CODES.M) {
            pfd = null;
            fd = SetFileDescriptor.createWithFd(ifd);
        } else {
            pfd = ParcelFileDescriptor.adoptFd(ifd);
            fd = pfd.getFileDescriptor();
        }
        // Second, convert FileDescriptor to Socket.
        Socket s = new SocketFd(fd);
        // Third, tag the Socket.
        TrafficStats.tagSocket(s);
        s.close(); // No-op but always good to close() Closeables.
        // Have ParcelFileDescriptor relinquish ownership of the file descriptor.
        if (pfd != null) {
            pfd.detachFd();
        }

        // Restore prior thread tags.
        if (tag != oldTag) {
            TrafficStats.setThreadStatsTag(oldTag);
        }
        if (uid != TrafficStatsUid.UNSET) {
            ThreadStatsUid.clear();
        }
    }
}
