// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chromoting.CapabilityManager;
import org.chromium.chromoting.InputStub;
import org.chromium.chromoting.Preconditions;
import org.chromium.chromoting.RenderStub;
import org.chromium.chromoting.SessionAuthenticator;

/**
 * Class to manage a client connection to the host. This class controls the lifetime of the
 * corresponding C++ object which implements the connection. A new object should be created for
 * each connection to the host, so that notifications from a connection are always sent to the
 * right object.
 * This class is used entirely on the UI thread.
 */
@JNINamespace("remoting")
public class Client implements InputStub {
    // Pointer to the C++ object, cast to a |long|.
    private final long mNativeJniClient;

    private RenderStub mRenderStub;

    // The global Client instance (may be null). This needs to be a global singleton so that the
    // Client can be passed between Activities.
    private static Client sClient;

    public Client() {
        if (sClient != null) {
            throw new RuntimeException("Client instance already created.");
        }

        sClient = this;
        mNativeJniClient = ClientJni.get().init(Client.this);
    }

    // Suppress FindBugs warning, since |sClient| is only used on the UI thread.
    public void destroy() {
        if (sClient != null) {
            disconnectFromHost();
            ClientJni.get().destroy(mNativeJniClient, Client.this);
            sClient = null;
        }
    }

    public void setRenderStub(RenderStub stub) {
        Preconditions.isNull(mRenderStub);
        Preconditions.notNull(stub);
        mRenderStub = stub;
    }

    public RenderStub getRenderStub() {
        return mRenderStub;
    }

    /** Returns the current Client instance, or null. */
    public static Client getInstance() {
        return sClient;
    }

    /** Used for authentication-related UX during connection. */
    private SessionAuthenticator mAuthenticator;

    /** Whether the native code is attempting a connection. */
    private boolean mConnected;

    /** Notified upon successful connection or disconnection. */
    private ConnectionListener mConnectionListener;

    /** Capability Manager through which capabilities and extensions are handled. */
    private CapabilityManager mCapabilityManager = new CapabilityManager();

    public CapabilityManager getCapabilityManager() {
        return mCapabilityManager;
    }

    /** Returns whether the client is connected. */
    public boolean isConnected() {
        return mConnected;
    }

    /** Attempts to form a connection to the user-selected host. */
    public void connectToHost(String username, String authToken, String hostJid, String hostFtlId,
            String hostId, String hostPubkey, SessionAuthenticator authenticator, String flags,
            String hostVersion, String hostOs, String hostOsVersion, ConnectionListener listener) {
        disconnectFromHost();

        mConnectionListener = listener;
        mAuthenticator = authenticator;
        ClientJni.get().connect(mNativeJniClient, Client.this, username, authToken, hostJid,
                hostFtlId, hostId, hostPubkey, mAuthenticator.getPairingId(hostId),
                mAuthenticator.getPairingSecret(hostId), mCapabilityManager.getLocalCapabilities(),
                flags, hostVersion, hostOs, hostOsVersion);
        mConnected = true;
    }

    /** Severs the connection and cleans up. */
    public void disconnectFromHost() {
        if (!mConnected) {
            return;
        }

        mConnectionListener.onConnectionState(
                ConnectionListener.State.CLOSED, ConnectionListener.Error.OK);

        disconnectFromHostWithoutNotification();
    }

    /** Same as disconnectFromHost() but without notifying the ConnectionListener. */
    private void disconnectFromHostWithoutNotification() {
        if (!mConnected) {
            return;
        }

        ClientJni.get().disconnect(mNativeJniClient, Client.this);
        mConnectionListener = null;
        mConnected = false;
        mCapabilityManager.onHostDisconnect();
    }

    /** Called whenever the connection status changes. */
    @CalledByNative
    void onConnectionState(
            @ConnectionListener.State int state, @ConnectionListener.Error int error) {
        mConnectionListener.onConnectionState(state, error);
        if (state == ConnectionListener.State.FAILED || state == ConnectionListener.State.CLOSED) {
            // Disconnect from the host here, otherwise the next time connectToHost() is called,
            // it will try to disconnect, triggering an incorrect status notification.

            // TODO(lambroslambrou): Connection state notifications for separate sessions should
            // go to separate Client instances. Once this is true, we can remove this line and
            // simplify the disconnectFromHost() code.
            disconnectFromHostWithoutNotification();
        }
    }

    /**
     * Called to prompt the user to enter a PIN.
     */
    @CalledByNative
    void displayAuthenticationPrompt(boolean pairingSupported) {
        mAuthenticator.displayAuthenticationPrompt(pairingSupported);
    }

    /**
     * Called by the SessionAuthenticator after the user enters a PIN.
     * @param pin The entered PIN.
     * @param createPair Whether to create a new pairing for this client.
     * @param deviceName The device name to appear in the pairing registry. Only used if createPair
     *                   is true.
     */
    public void handleAuthenticationResponse(
            String pin, boolean createPair, String deviceName) {
        assert mConnected;
        ClientJni.get().authenticationResponse(
                mNativeJniClient, Client.this, pin, createPair, deviceName);
    }

    /**
     * Called to save newly-received pairing credentials to permanent storage.
     */
    @CalledByNative
    void commitPairingCredentials(String host, String id, String secret) {
        mAuthenticator.commitPairingCredentials(host, id, secret);
    }

    /**
     * Moves the mouse cursor, possibly while clicking the specified (nonnegative) button.
     */
    @Override
    public void sendMouseEvent(int x, int y, int whichButton, boolean buttonDown) {
        if (!mConnected) {
            return;
        }

        ClientJni.get().sendMouseEvent(
                mNativeJniClient, Client.this, x, y, whichButton, buttonDown);
    }

    /** Injects a mouse-wheel event with delta values. */
    @Override
    public void sendMouseWheelEvent(int deltaX, int deltaY) {
        if (!mConnected) {
            return;
        }

        ClientJni.get().sendMouseWheelEvent(mNativeJniClient, Client.this, deltaX, deltaY);
    }

    /**
     * Presses or releases the specified key. If scanCode is not zero then
     * keyCode is ignored.
     */
    @Override
    public boolean sendKeyEvent(int scanCode, int keyCode, boolean keyDown) {
        if (!mConnected) {
            return false;
        }

        return ClientJni.get().sendKeyEvent(
                mNativeJniClient, Client.this, scanCode, keyCode, keyDown);
    }

    /** Sends TextEvent to the host. */
    @Override
    public void sendTextEvent(String text) {
        if (!mConnected) {
            return;
        }

        ClientJni.get().sendTextEvent(mNativeJniClient, Client.this, text);
    }

    /** Sends an array of TouchEvents to the host. */
    @Override
    public void sendTouchEvent(@TouchEventData.EventType int eventType, TouchEventData[] data) {
        if (!mConnected) {
            return;
        }

        ClientJni.get().sendTouchEvent(mNativeJniClient, Client.this, eventType, data);
    }

    /**
     * Enables or disables the video channel. Called in response to Activity lifecycle events.
     */
    public void enableVideoChannel(boolean enable) {
        if (!mConnected) {
            return;
        }

        ClientJni.get().enableVideoChannel(mNativeJniClient, Client.this, enable);
    }

    //
    // Third Party Authentication
    //

    /**
     * Pops up a third party login page to fetch the token required for authentication.
     */
    @CalledByNative
    void fetchThirdPartyToken(String tokenUrl, String clientId, String scope) {
        mAuthenticator.fetchThirdPartyToken(tokenUrl, clientId, scope);
    }

    /**
     * Called by the SessionAuthenticator to pass the |token| and |sharedSecret| to native code to
     * continue authentication.
     */
    public void onThirdPartyTokenFetched(String token, String sharedSecret) {
        if (!mConnected) {
            return;
        }

        ClientJni.get().onThirdPartyTokenFetched(
                mNativeJniClient, Client.this, token, sharedSecret);
    }

    //
    // Host and Client Capabilities
    //

    /**
     * Sets the list of negotiated capabilities between host and client.
     */
    @CalledByNative
    void setCapabilities(String capabilities) {
        mCapabilityManager.setNegotiatedCapabilities(capabilities);
    }

    //
    // Extension Message Handling
    //

    /**
     * Passes on the deconstructed ExtensionMessage to the app.
     */
    @CalledByNative
    void handleExtensionMessage(String type, String data) {
        mCapabilityManager.onExtensionMessage(type, data);
    }

    /** Sends an extension message to the Chromoting host. */
    public void sendExtensionMessage(String type, String data) {
        if (!mConnected) {
            return;
        }

        ClientJni.get().sendExtensionMessage(mNativeJniClient, Client.this, type, data);
    }

    /**
     * Sends client resolution to the host so that the host can resize itself to fit the client
     * without showing letterboxes.
     *
     * @param dipsWidth The width of the screen in density independent pixels.
     * @param dipsHeight The height of the screen in density independent pixels.
     * @param density The pixel density of the screen.
     */
    public void sendClientResolution(int dipsWidth, int dipsHeight, float density) {
        if (!mConnected) {
            return;
        }

        ClientJni.get().sendClientResolution(
                mNativeJniClient, Client.this, dipsWidth, dipsHeight, density);
    }

    @NativeMethods
    interface Natives {
        long init(Client caller);
        void destroy(long nativeJniClient, Client caller);
        /** Performs the native portion of the connection. */
        void connect(long nativeJniClient, Client caller, String username, String authToken,
                String hostJid, String hostFtlId, String hostId, String hostPubkey, String pairId,
                String pairSecret, String capabilities, String flags, String hostVersion,
                String hostOs, String hostOsVersion);

        /** Native implementation of Client.handleAuthenticationResponse(). */
        void authenticationResponse(long nativeJniClient, Client caller, String pin,
                boolean createPair, String deviceName);

        /** Performs the native portion of the cleanup. */
        void disconnect(long nativeJniClient, Client caller);

        /** Passes authentication data to the native handling code. */
        void onThirdPartyTokenFetched(
                long nativeJniClient, Client caller, String token, String sharedSecret);

        /** Passes mouse information to the native handling code. */
        void sendMouseEvent(long nativeJniClient, Client caller, int x, int y, int whichButton,
                boolean buttonDown);

        /** Passes mouse-wheel information to the native handling code. */
        void sendMouseWheelEvent(long nativeJniClient, Client caller, int deltaX, int deltaY);

        /** Passes key press information to the native handling code. */
        boolean sendKeyEvent(
                long nativeJniClient, Client caller, int scanCode, int keyCode, boolean keyDown);

        /** Passes text event information to the native handling code. */
        void sendTextEvent(long nativeJniClient, Client caller, String text);

        /** Passes touch event information to the native handling code. */
        void sendTouchEvent(
                long nativeJniClient, Client caller, int eventType, TouchEventData[] data);

        /** Native implementation of Client.enableVideoChannel() */
        void enableVideoChannel(long nativeJniClient, Client caller, boolean enable);

        /** Passes extension message to the native code. */
        void sendExtensionMessage(long nativeJniClient, Client caller, String type, String data);

        /** Sends client resolution to the host. */
        void sendClientResolution(
                long nativeJniClient, Client caller, int dipsWidth, int dipsHeight, float scale);
    }
}
