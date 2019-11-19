// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;
import static android.net.NetworkCapabilities.TRANSPORT_VPN;
import static android.net.NetworkCapabilities.TRANSPORT_WIFI;

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.StrictMode;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;
import android.telephony.TelephonyManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationState;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.net.NetworkChangeNotifierAutoDetect.ConnectivityManagerDelegate;
import org.chromium.net.NetworkChangeNotifierAutoDetect.NetworkState;
import org.chromium.net.NetworkChangeNotifierAutoDetect.WifiManagerDelegate;
import org.chromium.net.test.util.NetworkChangeNotifierTestUtil;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;

/**
 * Tests for org.chromium.net.NetworkChangeNotifier.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@SuppressLint("NewApi")
public class NetworkChangeNotifierTest {
    @Rule
    public UiThreadTestRule mUiThreadRule = new UiThreadTestRule();

    /**
     * Listens for alerts fired by the NetworkChangeNotifier when network status changes.
     */
    private static class NetworkChangeNotifierTestObserver
            implements NetworkChangeNotifier.ConnectionTypeObserver {
        private boolean mReceivedNotification;

        @Override
        public void onConnectionTypeChanged(int connectionType) {
            mReceivedNotification = true;
        }

        public boolean hasReceivedNotification() {
            return mReceivedNotification;
        }

        public void resetHasReceivedNotification() {
            mReceivedNotification = false;
        }
    }

    /**
      * Listens for native notifications of max bandwidth change.
      */
    private static class TestNetworkChangeNotifier extends NetworkChangeNotifier {
        @Override
        void notifyObserversOfConnectionSubtypeChange(int newConnectionSubtype) {
            mReceivedConnectionSubtypeNotification = true;
        }

        public boolean hasReceivedConnectionSubtypeNotification() {
            return mReceivedConnectionSubtypeNotification;
        }

        public void resetHasReceivedConnectionSubtypeNotification() {
            mReceivedConnectionSubtypeNotification = false;
        }

        private boolean mReceivedConnectionSubtypeNotification;
    }

    private static class Helper {
        private static final Constructor<Network> sNetworkConstructor;

        static {
            try {
                sNetworkConstructor = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
                        ? Network.class.getConstructor(Integer.TYPE)
                        : null;
            } catch (NoSuchMethodException | SecurityException e) {
                throw new RuntimeException("Unable to get Network constructor", e);
            }
        }

        static NetworkCapabilities getCapabilities(int transport) {
            // Create a NetworkRequest with corresponding capabilities
            NetworkRequest request = new NetworkRequest.Builder()
                                             .addCapability(NET_CAPABILITY_INTERNET)
                                             .addTransportType(transport)
                                             .build();
            // Extract the NetworkCapabilities from the NetworkRequest.
            try {
                return (NetworkCapabilities) request.getClass()
                        .getDeclaredField("networkCapabilities")
                        .get(request);
            } catch (NoSuchFieldException | IllegalAccessException e) {
                return null;
            }
        }
        // Create Network object given a NetID.
        static Network netIdToNetwork(int netId) {
            try {
                return sNetworkConstructor.newInstance(netId);
            } catch (
            InstantiationException | InvocationTargetException | IllegalAccessException e) {
                throw new IllegalStateException("Trying to create Network when not allowed");
            }
        }
    }

    private static void triggerApplicationStateChange(
            final RegistrationPolicyApplicationStatus policy, final int applicationState) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                policy.onApplicationStateChange(applicationState);
            }
        });
    }

    /**
     * Mocks out calls to the ConnectivityManager.
     */
    private class MockConnectivityManagerDelegate extends ConnectivityManagerDelegate {
        // A network we're pretending is currently connected.
        private class MockNetwork {
            // Network identifier
            final int mNetId;
            // Transport, one of android.net.NetworkCapabilities.TRANSPORT_*
            final int mTransport;
            // Is this VPN accessible to the current user?
            final boolean mVpnAccessible;

            NetworkCapabilities getCapabilities() {
                return Helper.getCapabilities(mTransport);
            }

            /**
             * @param netId Network identifier
             * @param transport Transport, one of android.net.NetworkCapabilities.TRANSPORT_*
             * @param vpnAccessible Is this VPN accessible to the current user?
             */
            MockNetwork(int netId, int transport, boolean vpnAccessible) {
                mNetId = netId;
                mTransport = transport;
                mVpnAccessible = vpnAccessible;
            }
        }

        // List of networks we're pretending are currently connected.
        private final ArrayList<MockNetwork> mMockNetworks = new ArrayList<>();

        private boolean mActiveNetworkExists;
        private int mNetworkType;
        private int mNetworkSubtype;
        private boolean mIsPrivateDnsActive;
        private String mPrivateDnsServerName;
        private NetworkCallback mLastRegisteredNetworkCallback;
        private NetworkCallback mLastRegisteredDefaultNetworkCallback;

        @Override
        public NetworkState getNetworkState(WifiManagerDelegate wifiManagerDelegate) {
            return new NetworkState(mActiveNetworkExists, mNetworkType, mNetworkSubtype,
                    mNetworkType == ConnectivityManager.TYPE_WIFI
                            ? wifiManagerDelegate.getWifiSsid()
                            : null,
                    mIsPrivateDnsActive, mPrivateDnsServerName);
        }

        @Override
        protected NetworkCapabilities getNetworkCapabilities(Network network) {
            int netId = demungeNetId(NetworkChangeNotifierAutoDetect.networkToNetId(network));
            for (MockNetwork mockNetwork : mMockNetworks) {
                if (netId == mockNetwork.mNetId) {
                    return mockNetwork.getCapabilities();
                }
            }
            return null;
        }

        @Override
        protected boolean vpnAccessible(Network network) {
            int netId = demungeNetId(NetworkChangeNotifierAutoDetect.networkToNetId(network));
            for (MockNetwork mockNetwork : mMockNetworks) {
                if (netId == mockNetwork.mNetId) {
                    return mockNetwork.mVpnAccessible;
                }
            }
            return false;
        }

        @Override
        protected Network[] getAllNetworksUnfiltered() {
            Network[] networks = new Network[mMockNetworks.size()];
            for (int i = 0; i < networks.length; i++) {
                networks[i] = Helper.netIdToNetwork(mMockNetworks.get(i).mNetId);
            }
            return networks;
        }

        // Dummy implementations to avoid NullPointerExceptions in default implementations:

        @Override
        public Network getDefaultNetwork() {
            return null;
        }

        @Override
        public int getConnectionType(Network network) {
            return ConnectionType.CONNECTION_NONE;
        }

        @Override
        public void unregisterNetworkCallback(NetworkCallback networkCallback) {}

        // Dummy implementation that also records the last registered callback.
        @Override
        public void registerNetworkCallback(
                NetworkRequest networkRequest, NetworkCallback networkCallback, Handler handler) {
            mLastRegisteredNetworkCallback = networkCallback;
        }

        // Dummy implementation that also records the last registered callback.
        @Override
        public void registerDefaultNetworkCallback(
                NetworkCallback networkCallback, Handler handler) {
            mLastRegisteredDefaultNetworkCallback = networkCallback;
        }

        public void setActiveNetworkExists(boolean networkExists) {
            mActiveNetworkExists = networkExists;
        }

        public void setNetworkType(int networkType) {
            mNetworkType = networkType;
        }

        public void setNetworkSubtype(int networkSubtype) {
            mNetworkSubtype = networkSubtype;
        }

        public void setIsPrivateDnsActive(boolean isPrivateDnsActive) {
            mIsPrivateDnsActive = isPrivateDnsActive;
        }

        public void setPrivateDnsServerName(String privateDnsServerName) {
            mPrivateDnsServerName = privateDnsServerName;
        }

        public NetworkCallback getLastRegisteredNetworkCallback() {
            return mLastRegisteredNetworkCallback;
        }

        public NetworkCallback getDefaultNetworkCallback() {
            return mLastRegisteredDefaultNetworkCallback;
        }

        /**
         * Pretends a network connects.
         * @param netId Network identifier
         * @param transport Transport, one of android.net.NetworkCapabilities.TRANSPORT_*
         * @param vpnAccessible Is this VPN accessible to the current user?
         */
        public void addNetwork(int netId, int transport, boolean vpnAccessible) {
            mMockNetworks.add(new MockNetwork(netId, transport, vpnAccessible));
            mLastRegisteredNetworkCallback.onAvailable(Helper.netIdToNetwork(netId));
        }

        /**
         * Pretends a network disconnects.
         * @param netId Network identifier
         */
        public void removeNetwork(int netId) {
            for (MockNetwork mockNetwork : mMockNetworks) {
                if (mockNetwork.mNetId == netId) {
                    mMockNetworks.remove(mockNetwork);
                    mLastRegisteredNetworkCallback.onLost(Helper.netIdToNetwork(netId));
                    break;
                }
            }
        }
    }

    /**
     * Mocks out calls to the WifiManager.
     */
    private static class MockWifiManagerDelegate extends WifiManagerDelegate {
        private String mWifiSSID;

        @Override
        public String getWifiSsid() {
            return mWifiSSID;
        }

        public void setWifiSSID(String wifiSSID) {
            mWifiSSID = wifiSSID;
        }
    }

    private static int demungeNetId(long netId) {
        // On Marshmallow, demunge the NetID to undo munging done in Network.getNetworkHandle().
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            netId >>= 32;
        }
        // Now that the NetID has been demunged it is a true NetID which means it's only a 16-bit
        // value (see ConnectivityService.MAX_NET_ID) so it should be safe to cast to int.
        return (int) netId;
    }

    // Types of network changes. Each is associated with a NetworkChangeNotifierAutoDetect.Observer
    // callback, and NONE is provided to indicate no callback observed.
    private static enum ChangeType { NONE, CONNECT, SOON_TO_DISCONNECT, DISCONNECT, PURGE_LIST }

    // Recorded information about a network change that took place.
    private static class ChangeInfo {
        // The type of change.
        final ChangeType mChangeType;
        // The network identifier of the network changing.
        final int mNetId;

        /**
         * @param changeType the type of change.
         * @param netId the network identifier of the network changing.
         */
        ChangeInfo(ChangeType changeType, long netId) {
            mChangeType = changeType;
            mNetId = demungeNetId(netId);
        }
    }

    // NetworkChangeNotifierAutoDetect.Observer used to verify proper notifications are sent out.
    // Notifications come back on UI thread. assertLastChange() called on test thread.
    private static class TestNetworkChangeNotifierAutoDetectObserver
            implements NetworkChangeNotifierAutoDetect.Observer {
        // The list of network changes that have been witnessed.
        final ArrayList<ChangeInfo> mChanges = new ArrayList<>();

        @Override
        public void onConnectionTypeChanged(int newConnectionType) {}
        @Override
        public void onConnectionSubtypeChanged(int newConnectionSubtype) {}

        @Override
        public void onNetworkConnect(long netId, int connectionType) {
            ThreadUtils.assertOnUiThread();
            mChanges.add(new ChangeInfo(ChangeType.CONNECT, netId));
        }

        @Override
        public void onNetworkSoonToDisconnect(long netId) {
            ThreadUtils.assertOnUiThread();
            mChanges.add(new ChangeInfo(ChangeType.SOON_TO_DISCONNECT, netId));
        }

        @Override
        public void onNetworkDisconnect(long netId) {
            ThreadUtils.assertOnUiThread();
            mChanges.add(new ChangeInfo(ChangeType.DISCONNECT, netId));
        }

        @Override
        public void purgeActiveNetworkList(long[] activeNetIds) {
            ThreadUtils.assertOnUiThread();
            if (activeNetIds.length == 1) {
                mChanges.add(new ChangeInfo(ChangeType.PURGE_LIST, activeNetIds[0]));
            } else {
                mChanges.add(new ChangeInfo(ChangeType.PURGE_LIST, NetId.INVALID));
            }
        }

        // Verify last notification was the expected one.
        public void assertLastChange(ChangeType type, int netId) throws Exception {
            // Make sure notification processed.
            NetworkChangeNotifierTestUtil.flushUiThreadTaskQueue();
            Assert.assertNotNull(mChanges.get(0));
            Assert.assertEquals(type, mChanges.get(0).mChangeType);
            Assert.assertEquals(netId, mChanges.get(0).mNetId);
            mChanges.clear();
        }
    }

    // Network.Network(int netId) pointer.
    private TestNetworkChangeNotifier mNotifier;
    private NetworkChangeNotifierAutoDetect mReceiver;
    private MockConnectivityManagerDelegate mConnectivityDelegate;
    private MockWifiManagerDelegate mWifiDelegate;

    private static enum WatchForChanges {
        ALWAYS,
        ONLY_WHEN_APP_IN_FOREGROUND,
    }

    /**
     * Helper method to create a notifier and delegates for testing.
     * @param watchForChanges indicates whether app wants to watch for changes always or only when
     *            it is in the foreground.
     */
    private void createTestNotifier(WatchForChanges watchForChanges) {
        Context context = new ContextWrapper(InstrumentationRegistry.getInstrumentation()
                                                     .getTargetContext()
                                                     .getApplicationContext()) {
            // Mock out to avoid unintended system interaction.
            @Override
            public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
                // Should not be used starting with Pie.
                Assert.assertFalse(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
                return null;
            }

            @Override
            public void unregisterReceiver(BroadcastReceiver receiver) {}

            // Don't allow escaping the mock via the application context.
            @Override
            public Context getApplicationContext() {
                return this;
            }
        };
        ContextUtils.initApplicationContextForTests(context);
        mNotifier = new TestNetworkChangeNotifier();
        NetworkChangeNotifier.resetInstanceForTests(mNotifier);
        if (watchForChanges == WatchForChanges.ALWAYS) {
            NetworkChangeNotifier.registerToReceiveNotificationsAlways();
        } else {
            NetworkChangeNotifier.setAutoDetectConnectivityState(true);
        }
        mReceiver = NetworkChangeNotifier.getAutoDetectorForTest();
        Assert.assertNotNull(mReceiver);

        mConnectivityDelegate =
                new MockConnectivityManagerDelegate();
        mConnectivityDelegate.setActiveNetworkExists(true);
        mReceiver.setConnectivityManagerDelegateForTests(mConnectivityDelegate);

        mWifiDelegate = new MockWifiManagerDelegate();
        mReceiver.setWifiManagerDelegateForTests(mWifiDelegate);
        mWifiDelegate.setWifiSSID("foo");
    }

    private int getCurrentConnectionSubtype() {
        return mReceiver.getCurrentNetworkState().getConnectionSubtype();
    }

    private int getCurrentConnectionType() {
        return mReceiver.getCurrentNetworkState().getConnectionType();
    }

    @Before
    public void setUp() throws Throwable {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        mUiThreadRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                createTestNotifier(WatchForChanges.ONLY_WHEN_APP_IN_FOREGROUND);
            }
        });
    }

    /**
     * Tests that the receiver registers for connectivity
     * broadcasts during construction when the registration policy dictates.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierRegistersWhenPolicyDictates() {
        NetworkChangeNotifierAutoDetect.Observer observer =
                new TestNetworkChangeNotifierAutoDetectObserver();

        NetworkChangeNotifierAutoDetect receiver = new NetworkChangeNotifierAutoDetect(
                observer, new RegistrationPolicyApplicationStatus() {
                    @Override
                    int getApplicationState() {
                        return ApplicationState.HAS_RUNNING_ACTIVITIES;
                    }
                });

        Assert.assertTrue(receiver.isReceiverRegisteredForTesting());

        receiver = new NetworkChangeNotifierAutoDetect(
                observer, new RegistrationPolicyApplicationStatus() {
                    @Override
                    int getApplicationState() {
                        return ApplicationState.HAS_PAUSED_ACTIVITIES;
                    }
                });

        Assert.assertFalse(receiver.isReceiverRegisteredForTesting());
    }

    /**
     * Tests that the receiver toggles registration for connectivity intents based on activity
     * state.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierRegistersForIntents() {
        RegistrationPolicyApplicationStatus policy =
                (RegistrationPolicyApplicationStatus) mReceiver.getRegistrationPolicy();
        triggerApplicationStateChange(policy, ApplicationState.HAS_RUNNING_ACTIVITIES);
        Assert.assertTrue(mReceiver.isReceiverRegisteredForTesting());

        triggerApplicationStateChange(policy, ApplicationState.HAS_PAUSED_ACTIVITIES);
        Assert.assertFalse(mReceiver.isReceiverRegisteredForTesting());

        triggerApplicationStateChange(policy, ApplicationState.HAS_RUNNING_ACTIVITIES);
        Assert.assertTrue(mReceiver.isReceiverRegisteredForTesting());
    }

    /**
     * Tests that changing the network type changes the connection subtype.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierConnectionSubtypeEthernet() {
        // Show that for Ethernet the link speed is unknown (+Infinity).
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_ETHERNET);
        Assert.assertEquals(ConnectionType.CONNECTION_ETHERNET, getCurrentConnectionType());
        Assert.assertEquals(ConnectionSubtype.SUBTYPE_UNKNOWN, getCurrentConnectionSubtype());
    }

    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierConnectionSubtypeWifi() {
        // Show that for WiFi the link speed is unknown (+Infinity).
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
        Assert.assertEquals(ConnectionType.CONNECTION_WIFI, getCurrentConnectionType());
        Assert.assertEquals(ConnectionSubtype.SUBTYPE_UNKNOWN, getCurrentConnectionSubtype());
    }

    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierConnectionSubtypeWiMax() {
        // Show that for WiMax the link speed is unknown (+Infinity), although the type is 4g.
        // TODO(jkarlin): Add support for CONNECTION_WIMAX as specified in
        // http://w3c.github.io/netinfo/.
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIMAX);
        Assert.assertEquals(ConnectionType.CONNECTION_4G, getCurrentConnectionType());
        Assert.assertEquals(ConnectionSubtype.SUBTYPE_UNKNOWN, getCurrentConnectionSubtype());
    }

    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierConnectionSubtypeBluetooth() {
        // Show that for bluetooth the link speed is unknown (+Infinity).
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_BLUETOOTH);
        Assert.assertEquals(ConnectionType.CONNECTION_BLUETOOTH, getCurrentConnectionType());
        Assert.assertEquals(ConnectionSubtype.SUBTYPE_UNKNOWN, getCurrentConnectionSubtype());
    }

    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierConnectionSubtypeMobile() {
        // Test that for mobile types the subtype is used to determine the connection subtype.
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_MOBILE);
        mConnectivityDelegate.setNetworkSubtype(TelephonyManager.NETWORK_TYPE_LTE);
        Assert.assertEquals(ConnectionType.CONNECTION_4G, getCurrentConnectionType());
        Assert.assertEquals(ConnectionSubtype.SUBTYPE_LTE, getCurrentConnectionSubtype());
    }

    /**
     * Indicate to NetworkChangeNotifierAutoDetect that a connectivity change has occurred.
     * Uses same signals that system would use.
     */
    private void notifyConnectivityChange() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mConnectivityDelegate.getDefaultNetworkCallback().onAvailable(null);
        } else {
            Intent connectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
            mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
        }
    }

    /**
     * Tests that when Chrome gets an intent indicating a change in network connectivity, it sends a
     * notification to Java observers.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierJavaObservers() {
        mReceiver.register();
        // Initialize the NetworkChangeNotifier with a connection.
        Intent connectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);

        // We shouldn't be re-notified if the connection hasn't actually changed.
        NetworkChangeNotifierTestObserver observer = new NetworkChangeNotifierTestObserver();
        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        notifyConnectivityChange();
        Assert.assertFalse(observer.hasReceivedNotification());

        // We shouldn't be notified if we're connected to non-Wifi and the Wifi SSID changes.
        mWifiDelegate.setWifiSSID("bar");
        notifyConnectivityChange();
        Assert.assertFalse(observer.hasReceivedNotification());
        // We should be notified when we change to Wifi.
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
        notifyConnectivityChange();
        Assert.assertTrue(observer.hasReceivedNotification());
        observer.resetHasReceivedNotification();
        // We should be notified when the Wifi SSID changes.
        mWifiDelegate.setWifiSSID("foo");
        notifyConnectivityChange();
        Assert.assertTrue(observer.hasReceivedNotification());
        observer.resetHasReceivedNotification();
        // We shouldn't be re-notified if the Wifi SSID hasn't actually changed.
        notifyConnectivityChange();
        Assert.assertFalse(observer.hasReceivedNotification());

        // We should be notified if use of DNS-over-TLS changes.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // Verify notification for enabling private DNS.
            mConnectivityDelegate.setIsPrivateDnsActive(true);
            mConnectivityDelegate.getDefaultNetworkCallback().onLinkPropertiesChanged(null, null);
            Assert.assertTrue(observer.hasReceivedNotification());
            observer.resetHasReceivedNotification();
            // Verify notification for specifying private DNS server.
            mConnectivityDelegate.setPrivateDnsServerName("dotserver.com");
            mConnectivityDelegate.getDefaultNetworkCallback().onLinkPropertiesChanged(null, null);
            Assert.assertTrue(observer.hasReceivedNotification());
            observer.resetHasReceivedNotification();
            // Verify no notification for no change.
            mConnectivityDelegate.getDefaultNetworkCallback().onLinkPropertiesChanged(null, null);
            Assert.assertFalse(observer.hasReceivedNotification());
            // Verify notification for disabling.
            mConnectivityDelegate.setIsPrivateDnsActive(false);
            mConnectivityDelegate.getDefaultNetworkCallback().onLinkPropertiesChanged(null, null);
            Assert.assertTrue(observer.hasReceivedNotification());
            observer.resetHasReceivedNotification();
        }

        // Mimic that connectivity has been lost and ensure that Chrome notifies our observer.
        mConnectivityDelegate.setActiveNetworkExists(false);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mConnectivityDelegate.getDefaultNetworkCallback().onLost(null);
        } else {
            mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
        }
        Assert.assertTrue(observer.hasReceivedNotification());

        observer.resetHasReceivedNotification();
        // Pretend we got moved to the background.
        final RegistrationPolicyApplicationStatus policy =
                (RegistrationPolicyApplicationStatus) mReceiver.getRegistrationPolicy();
        triggerApplicationStateChange(policy, ApplicationState.HAS_PAUSED_ACTIVITIES);
        // Change the state.
        mConnectivityDelegate.setActiveNetworkExists(true);
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
        // The NetworkChangeNotifierAutoDetect doesn't receive any notification while we are in the
        // background, but when we get back to the foreground the state changed should be detected
        // and a notification sent.
        triggerApplicationStateChange(policy, ApplicationState.HAS_RUNNING_ACTIVITIES);
        Assert.assertTrue(observer.hasReceivedNotification());
    }

    /**
     * Tests that when Chrome gets an intent indicating a change in max bandwidth, it sends a
     * notification to Java observers.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierConnectionSubtypeNotifications() {
        mReceiver.register();
        // Initialize the NetworkChangeNotifier with a connection.
        mConnectivityDelegate.setActiveNetworkExists(true);
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
        Intent connectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
        Assert.assertTrue(mNotifier.hasReceivedConnectionSubtypeNotification());
        mNotifier.resetHasReceivedConnectionSubtypeNotification();

        // We shouldn't be re-notified if the connection hasn't actually changed.
        NetworkChangeNotifierTestObserver observer = new NetworkChangeNotifierTestObserver();
        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
        Assert.assertFalse(mNotifier.hasReceivedConnectionSubtypeNotification());

        // We should be notified if bandwidth and connection type changed.
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_ETHERNET);
        mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
        Assert.assertTrue(mNotifier.hasReceivedConnectionSubtypeNotification());
        mNotifier.resetHasReceivedConnectionSubtypeNotification();

        // We should be notified if the connection type changed, but not the bandwidth.
        // Note that TYPE_ETHERNET and TYPE_BLUETOOTH have the same +INFINITY max bandwidth.
        // This test will fail if that changes.
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_BLUETOOTH);
        mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
        Assert.assertTrue(mNotifier.hasReceivedConnectionSubtypeNotification());
    }

    /**
     * Tests that when setting {@code registerToReceiveNotificationsAlways()},
     * a NetworkChangeNotifierAutoDetect object is successfully created.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testCreateNetworkChangeNotifierAlwaysWatchForChanges() {
        createTestNotifier(WatchForChanges.ALWAYS);
        Assert.assertTrue(mReceiver.isReceiverRegisteredForTesting());

        // Make sure notifications can be received.
        NetworkChangeNotifierTestObserver observer = new NetworkChangeNotifierTestObserver();
        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        Intent connectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), connectivityIntent);
        Assert.assertTrue(observer.hasReceivedNotification());
    }

    /**
     * Tests that ConnectivityManagerDelegate doesn't crash. This test cannot rely on having any
     * active network connections so it cannot usefully check results, but it can at least check
     * that the functions don't crash.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testConnectivityManagerDelegateDoesNotCrash() {
        ConnectivityManagerDelegate delegate =
                new ConnectivityManagerDelegate(InstrumentationRegistry.getTargetContext());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            delegate.getNetworkState(null);
        } else {
            delegate.getNetworkState(
                    new WifiManagerDelegate(InstrumentationRegistry.getTargetContext()));
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // getConnectionType(Network) doesn't crash upon invalid Network argument.
            Network invalidNetwork = Helper.netIdToNetwork(NetId.INVALID);
            Assert.assertEquals(
                    ConnectionType.CONNECTION_NONE, delegate.getConnectionType(invalidNetwork));

            Network[] networks = delegate.getAllNetworksUnfiltered();
            Assert.assertNotNull(networks);
            if (networks.length >= 1) {
                delegate.getConnectionType(networks[0]);
            }
            delegate.getDefaultNetwork();
            NetworkCallback networkCallback = new NetworkCallback();
            NetworkRequest networkRequest = new NetworkRequest.Builder().build();
            delegate.registerNetworkCallback(
                    networkRequest, networkCallback, new Handler(Looper.myLooper()));
            delegate.unregisterNetworkCallback(networkCallback);
        }
    }

    /**
     * Tests that NetworkChangeNotifierAutoDetect queryable APIs don't crash. This test cannot rely
     * on having any active network connections so it cannot usefully check results, but it can at
     * least check that the functions don't crash.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testQueryableAPIsDoNotCrash() {
        NetworkChangeNotifierAutoDetect.Observer observer =
                new TestNetworkChangeNotifierAutoDetectObserver();
        NetworkChangeNotifierAutoDetect ncn = new NetworkChangeNotifierAutoDetect(observer,
                new RegistrationPolicyAlwaysRegister());
        ncn.getNetworksAndTypes();
        ncn.getDefaultNetId();
    }

    /**
     * Tests that NetworkChangeNotifierAutoDetect query-able APIs return expected
     * values from the inserted mock ConnectivityManager.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testQueryableAPIsReturnExpectedValuesFromMockDelegate() {
        NetworkChangeNotifierAutoDetect.Observer observer =
                new TestNetworkChangeNotifierAutoDetectObserver();

        NetworkChangeNotifierAutoDetect ncn = new NetworkChangeNotifierAutoDetect(
                observer, new RegistrationPolicyApplicationStatus() {
                    @Override
                    int getApplicationState() {
                        return ApplicationState.HAS_PAUSED_ACTIVITIES;
                    }
                });

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            Assert.assertEquals(0, ncn.getNetworksAndTypes().length);
            Assert.assertEquals(NetId.INVALID, ncn.getDefaultNetId());
            return;
        }

        // Insert a mocked dummy implementation for the ConnectivityDelegate.
        ncn.setConnectivityManagerDelegateForTests(new ConnectivityManagerDelegate() {
            public final Network[] mNetworks =
                    new Network[] {Helper.netIdToNetwork(111), Helper.netIdToNetwork(333)};

            @Override
            protected Network[] getAllNetworksUnfiltered() {
                return mNetworks;
            }

            @Override
            Network getDefaultNetwork() {
                return mNetworks[1];
            }

            @Override
            protected NetworkCapabilities getNetworkCapabilities(Network network) {
                return Helper.getCapabilities(TRANSPORT_WIFI);
            }

            @Override
            public int getConnectionType(Network network) {
                return ConnectionType.CONNECTION_NONE;
            }
        });

        // Verify that the mock delegate connectivity manager is being used
        // by the network change notifier auto-detector.
        Assert.assertEquals(333, demungeNetId(ncn.getDefaultNetId()));

        // The api {@link NetworkChangeNotifierAutoDetect#getNetworksAndTypes()}
        // returns an array of a repeated sequence of: (NetID, ConnectionType).
        // There are 4 entries in the array, two for each network.
        Assert.assertEquals(4, ncn.getNetworksAndTypes().length);
        Assert.assertEquals(111, demungeNetId(ncn.getNetworksAndTypes()[0]));
        Assert.assertEquals(ConnectionType.CONNECTION_NONE, ncn.getNetworksAndTypes()[1]);
        Assert.assertEquals(333, demungeNetId(ncn.getNetworksAndTypes()[2]));
        Assert.assertEquals(ConnectionType.CONNECTION_NONE, ncn.getNetworksAndTypes()[3]);
    }

    /**
     * Tests that callbacks are issued to Observers when NetworkChangeNotifierAutoDetect receives
     * the right signals (via its NetworkCallback).
     */
    @Test
    @MediumTest
    @Feature({"Android-AppBase"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    public void testNetworkCallbacks() throws Exception {
        // Setup NetworkChangeNotifierAutoDetect
        final TestNetworkChangeNotifierAutoDetectObserver observer =
                new TestNetworkChangeNotifierAutoDetectObserver();
        Callable<NetworkChangeNotifierAutoDetect> callable =
                new Callable<NetworkChangeNotifierAutoDetect>() {
                    @Override
                    public NetworkChangeNotifierAutoDetect call() {
                        return new NetworkChangeNotifierAutoDetect(
                                observer, new RegistrationPolicyApplicationStatus() {
                                    // This override prevents NetworkChangeNotifierAutoDetect from
                                    // registering for events right off the bat. We'll delay this
                                    // until our MockConnectivityManagerDelegate is first installed
                                    // to prevent inadvertent communication with the real
                                    // ConnectivityManager.
                                    @Override
                                    int getApplicationState() {
                                        return ApplicationState.HAS_PAUSED_ACTIVITIES;
                                    }
                                });
                    }
                };
        FutureTask<NetworkChangeNotifierAutoDetect> task = new FutureTask<>(callable);
        ThreadUtils.postOnUiThread(task);
        NetworkChangeNotifierAutoDetect ncn = task.get();

        // Insert mock ConnectivityDelegate
        mConnectivityDelegate = new MockConnectivityManagerDelegate();
        ncn.setConnectivityManagerDelegateForTests(mConnectivityDelegate);
        // Now that mock ConnectivityDelegate is inserted, pretend app is foregrounded
        // so NetworkChangeNotifierAutoDetect will register its NetworkCallback.
        Assert.assertFalse(ncn.isReceiverRegisteredForTesting());

        RegistrationPolicyApplicationStatus policy =
                (RegistrationPolicyApplicationStatus) ncn.getRegistrationPolicy();
        triggerApplicationStateChange(policy, ApplicationState.HAS_RUNNING_ACTIVITIES);
        Assert.assertTrue(ncn.isReceiverRegisteredForTesting());

        // Find NetworkChangeNotifierAutoDetect's NetworkCallback, which should have been registered
        // with mConnectivityDelegate.
        NetworkCallback networkCallback = mConnectivityDelegate.getLastRegisteredNetworkCallback();
        Assert.assertNotNull(networkCallback);

        // First thing we'll receive is a purge to initialize any network lists.
        observer.assertLastChange(ChangeType.PURGE_LIST, NetId.INVALID);

        // Test connected signal is passed along.
        mConnectivityDelegate.addNetwork(100, TRANSPORT_WIFI, false);
        observer.assertLastChange(ChangeType.CONNECT, 100);

        // Test soon-to-be-disconnected signal is passed along.
        networkCallback.onLosing(Helper.netIdToNetwork(100), 30);
        observer.assertLastChange(ChangeType.SOON_TO_DISCONNECT, 100);

        // Test connected signal is passed along.
        mConnectivityDelegate.removeNetwork(100);
        observer.assertLastChange(ChangeType.DISCONNECT, 100);

        // Simulate app backgrounding then foregrounding.
        Assert.assertTrue(ncn.isReceiverRegisteredForTesting());
        triggerApplicationStateChange(policy, ApplicationState.HAS_PAUSED_ACTIVITIES);
        Assert.assertFalse(ncn.isReceiverRegisteredForTesting());
        triggerApplicationStateChange(policy, ApplicationState.HAS_RUNNING_ACTIVITIES);
        Assert.assertTrue(ncn.isReceiverRegisteredForTesting());
        // Verify network list purged.
        observer.assertLastChange(ChangeType.PURGE_LIST, NetId.INVALID);

        //
        // VPN testing
        //

        // Add a couple normal networks
        mConnectivityDelegate.addNetwork(100, TRANSPORT_WIFI, false);
        observer.assertLastChange(ChangeType.CONNECT, 100);
        mConnectivityDelegate.addNetwork(101, TRANSPORT_CELLULAR, false);
        observer.assertLastChange(ChangeType.CONNECT, 101);

        // Verify inaccessible VPN is ignored
        mConnectivityDelegate.addNetwork(102, TRANSPORT_VPN, false);
        NetworkChangeNotifierTestUtil.flushUiThreadTaskQueue();
        Assert.assertEquals(observer.mChanges.size(), 0);
        // The disconnect will be ignored in
        // NetworkChangeNotifierDelegateAndroid::NotifyOfNetworkDisconnect() because no
        // connect event was witnessed, but it will be sent to {@code observer}
        mConnectivityDelegate.removeNetwork(102);
        observer.assertLastChange(ChangeType.DISCONNECT, 102);

        // Verify when an accessible VPN connects, all other network disconnect
        mConnectivityDelegate.addNetwork(103, TRANSPORT_VPN, true);
        NetworkChangeNotifierTestUtil.flushUiThreadTaskQueue();
        Assert.assertEquals(2, observer.mChanges.size());
        Assert.assertEquals(ChangeType.CONNECT, observer.mChanges.get(0).mChangeType);
        Assert.assertEquals(103, observer.mChanges.get(0).mNetId);
        Assert.assertEquals(ChangeType.PURGE_LIST, observer.mChanges.get(1).mChangeType);
        Assert.assertEquals(103, observer.mChanges.get(1).mNetId);
        observer.mChanges.clear();

        // Verify when an accessible VPN disconnects, all other networks reconnect
        mConnectivityDelegate.removeNetwork(103);
        NetworkChangeNotifierTestUtil.flushUiThreadTaskQueue();
        Assert.assertEquals(3, observer.mChanges.size());
        Assert.assertEquals(ChangeType.DISCONNECT, observer.mChanges.get(0).mChangeType);
        Assert.assertEquals(103, observer.mChanges.get(0).mNetId);
        Assert.assertEquals(ChangeType.CONNECT, observer.mChanges.get(1).mChangeType);
        Assert.assertEquals(100, observer.mChanges.get(1).mNetId);
        Assert.assertEquals(ChangeType.CONNECT, observer.mChanges.get(2).mChangeType);
        Assert.assertEquals(101, observer.mChanges.get(2).mNetId);
    }

    /**
     * Tests that isOnline() returns the correct result.
     */
    @Test
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierIsOnline() {
        mReceiver.register();
        Intent intent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        // For any connection type it should return true.
        for (int i = ConnectivityManager.TYPE_MOBILE; i < ConnectivityManager.TYPE_VPN; i++) {
            mConnectivityDelegate.setActiveNetworkExists(true);
            mConnectivityDelegate.setNetworkType(i);
            mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), intent);
            Assert.assertTrue(NetworkChangeNotifier.isOnline());
        }
        mConnectivityDelegate.setActiveNetworkExists(false);
        mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), intent);
        Assert.assertFalse(NetworkChangeNotifier.isOnline());
    }

    /**
     * Tests NetworkChangeNotifier.isProcessBoundToNetwork().
     */
    @Test
    @MediumTest
    @Feature({"Android-AppBase"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testIsProcessBoundToNetwork() {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) InstrumentationRegistry.getTargetContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        Network network = connectivityManager.getActiveNetwork();
        Assert.assertFalse(NetworkChangeNotifier.isProcessBoundToNetwork());
        if (network != null) {
            ConnectivityManager.setProcessDefaultNetwork(network);
            Assert.assertTrue(NetworkChangeNotifier.isProcessBoundToNetwork());
        }
        ConnectivityManager.setProcessDefaultNetwork(null);
        Assert.assertFalse(NetworkChangeNotifier.isProcessBoundToNetwork());
    }

    /**
     * Regression test for crbug.com/805424 where ConnectivityManagerDelegate.vpnAccessible() was
     * found to leak.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // android.net.Network available in L+.
    public void testVpnAccessibleDoesNotLeak() {
        ConnectivityManagerDelegate connectivityManagerDelegate = new ConnectivityManagerDelegate(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        StrictMode.VmPolicy oldPolicy = StrictMode.getVmPolicy();
        StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                                       .detectLeakedClosableObjects()
                                       .penaltyDeath()
                                       .penaltyLog()
                                       .build());
        try {
            // Test non-existent Network (NetIds only go to 65535).
            connectivityManagerDelegate.vpnAccessible(Helper.netIdToNetwork(65537));
            // Test existing Networks.
            for (Network network : connectivityManagerDelegate.getAllNetworksUnfiltered()) {
                connectivityManagerDelegate.vpnAccessible(network);
            }

            // Run GC and finalizers a few times to pick up leaked closeables
            for (int i = 0; i < 10; i++) {
                System.gc();
                System.runFinalization();
            }
            System.gc();
            System.runFinalization();
        } finally {
            StrictMode.setVmPolicy(oldPolicy);
        }
    }

    /**
     * Regression test for crbug.com/946531 where ConnectivityManagerDelegate.vpnAccessible()
     * triggered StrictMode's untagged socket prohibition.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O) // detectUntaggedSockets added in Oreo.
    public void testVpnAccessibleDoesNotCreateUntaggedSockets() {
        ConnectivityManagerDelegate connectivityManagerDelegate = new ConnectivityManagerDelegate(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        StrictMode.VmPolicy oldPolicy = StrictMode.getVmPolicy();
        StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                                       .detectUntaggedSockets()
                                       .penaltyDeath()
                                       .penaltyLog()
                                       .build());
        try {
            // Test non-existent Network (NetIds only go to 65535).
            connectivityManagerDelegate.vpnAccessible(Helper.netIdToNetwork(65537));
            // Test existing Networks.
            for (Network network : connectivityManagerDelegate.getAllNetworksUnfiltered()) {
                connectivityManagerDelegate.vpnAccessible(network);
            }
        } finally {
            StrictMode.setVmPolicy(oldPolicy);
        }
    }
}
