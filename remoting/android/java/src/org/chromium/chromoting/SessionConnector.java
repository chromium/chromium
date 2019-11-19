// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import org.chromium.chromoting.jni.Client;
import org.chromium.chromoting.jni.ConnectionListener;
import org.chromium.chromoting.jni.DirectoryService;
import org.chromium.chromoting.jni.DirectoryServiceRequestError;

/**
 * This class manages making a connection to a host, with logic for reloading the host list and
 * retrying the connection in the case of a stale host JID.
 */
public class SessionConnector implements ConnectionListener, DirectoryService.HostListCallback {
    private Client mClient;
    private ConnectionListener mConnectionListener;
    private DirectoryService.HostListCallback mHostListCallback;
    private DirectoryService mDirectoryService;
    private SessionAuthenticator mAuthenticator;

    private String mAccountName;
    private String mAuthToken;

    /* HostInfo for the host we are connecting to. */
    private HostInfo mHost;

    private String mFlags;

    /**
     * Tracks whether the connection has been established. Auto-reloading and reconnecting should
     * only happen if connection has not yet occurred.
     */
    private boolean mWasConnected;
    private boolean mTriedReloadingHostList;

    /**
     * @param connectionListener Object to be notified on connection success/failure.
     * @param hostListCallback Object to be notified whenever the host list is reloaded.
     * @param directoryService The object used for reloading the host list.
     */
    public SessionConnector(Client client, ConnectionListener connectionListener,
            DirectoryService.HostListCallback hostListCallback, DirectoryService directoryService) {
        mClient = client;
        mConnectionListener = connectionListener;
        mHostListCallback = hostListCallback;
        mDirectoryService = directoryService;
    }

    /** Initiates a connection to the host. */
    public void connectToHost(String accountName, String authToken, HostInfo host,
            SessionAuthenticator authenticator, String flags) {
        mAccountName = accountName;
        mAuthToken = authToken;
        mHost = host;
        mAuthenticator = authenticator;
        mFlags = flags;

        if (host.isIncomplete()) {
            // These keys might not be present in a newly-registered host, so treat this as a
            // connection failure and reload the host list.
            reloadHostListAndConnect();
            return;
        }

        doConnect();
    }

    private void doConnect() {
        mClient.connectToHost(mAccountName, mAuthToken, mHost.jabberId, mHost.ftlId, mHost.id,
                mHost.publicKey, mAuthenticator, mFlags, mHost.hostVersion, mHost.hostOs,
                mHost.hostOsVersion, this);
    }

    private void reloadHostListAndConnect() {
        mTriedReloadingHostList = true;
        mDirectoryService.retrieveHostList(this);
    }

    @Override
    public void onConnectionState(
            @ConnectionListener.State int state, @ConnectionListener.Error int error) {
        switch (state) {
            case ConnectionListener.State.CONNECTED:
                mWasConnected = true;
                break;
            case ConnectionListener.State.FAILED:
                // The host is offline, which may mean the JID is out of date, so refresh the host
                // list and try to connect again.
                if (error == ConnectionListener.Error.PEER_IS_OFFLINE && !mWasConnected
                        && !mTriedReloadingHostList) {
                    reloadHostListAndConnect();
                    return;
                }
                break;
            default:
                break;
        }

         // Pass the state/error back to the caller.
        mConnectionListener.onConnectionState(state, error);
    }

    @Override
    public void onHostListReceived(HostInfo[] hosts) {
        // Notify the caller, so the UI is updated.
        mHostListCallback.onHostListReceived(hosts);

        HostInfo foundHost = null;
        for (HostInfo host : hosts) {
            if (host.id.equals(mHost.id)) {
                foundHost = host;
                break;
            }
        }

        if (foundHost == null || foundHost.ftlId.equals(mHost.ftlId) || foundHost.isIncomplete()) {
            // Cannot reconnect to this host, or there's no point in trying because the FtlId is
            // unchanged, so report connection error to the client.
            mConnectionListener.onConnectionState(ConnectionListener.State.FAILED,
                    ConnectionListener.Error.PEER_IS_OFFLINE);
        } else {
            mHost = foundHost;
            doConnect();
        }
    }

    @Override
    public void onError(@DirectoryServiceRequestError int error) {
        // Connection failed and reloading the host list also failed, so report the connection
        // error.
        mConnectionListener.onConnectionState(ConnectionListener.State.FAILED,
                ConnectionListener.Error.PEER_IS_OFFLINE);

        // Notify the caller that the host list failed to load, so the UI is updated accordingly.
        // The currently-displayed host list is not likely to be valid any more.
        mHostListCallback.onError(error);
    }
}
