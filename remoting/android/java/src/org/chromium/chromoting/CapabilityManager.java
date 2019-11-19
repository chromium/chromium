// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.Log;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A manager for the capabilities of the Android client. Based on the negotiated set of
 * capabilities, it creates the associated ClientExtensions, and enables their communication with
 * the Chromoting host by dispatching extension messages appropriately.
 *
 * The CapabilityManager mirrors how the Chromoting host handles extension messages. For each
 * incoming extension message, runs through a list of HostExtensionSession objects, giving each one
 * a chance to handle the message.
 */
public class CapabilityManager {
    /** Used to allow objects to receive notifications when the host capabilites are received. */
    public interface CapabilitiesChangedListener {
        void onCapabilitiesChanged(List<String> newCapabilities);
    }

    /** Tracks whether the remote host supports a capability. */
    @IntDef({HostCapability.UNKNOWN, HostCapability.SUPPORTED, HostCapability.UNSUPPORTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HostCapability {
        int UNKNOWN = 0;
        int SUPPORTED = 1;
        int UNSUPPORTED = 2;
    }

    public static boolean hostCapabilityIsSet(@HostCapability int capability) {
        return capability != HostCapability.UNKNOWN;
    }

    public static boolean hostCapabilityIsSupported(@HostCapability int capability) {
        assert hostCapabilityIsSet(capability);
        return capability == HostCapability.SUPPORTED;
    }

    private static final String TAG = "Chromoting";

    /** List of all capabilities that are supported by the application. */
    private List<String> mLocalCapabilities;

    /** List of negotiated capabilities received from the host. */
    private List<String> mNegotiatedCapabilities;

    /** List of extensions to the client based on capabilities negotiated with the host. */
    private List<ClientExtension> mClientExtensions;

    /** Maintains a list of listeners to notify when host capabilities are received. */
    private List<CapabilitiesChangedListener> mCapabilitiesChangedListeners;

    public CapabilityManager() {
        mLocalCapabilities = new ArrayList<String>();
        mClientExtensions = new ArrayList<ClientExtension>();

        mLocalCapabilities.add(Capabilities.CAST_CAPABILITY);
        mLocalCapabilities.add(Capabilities.TOUCH_CAPABILITY);

        mCapabilitiesChangedListeners = new ArrayList<CapabilitiesChangedListener>();
    }

    /**
     * Cleans up host specific state when the connection has been terminated.
     */
    public void onHostDisconnect() {
        mNegotiatedCapabilities = null;
    }

    /**
     * Returns a space-separated list (required by host) of the capabilities supported by
     * this client.
     */
    public String getLocalCapabilities() {
        return TextUtils.join(" ", mLocalCapabilities);
    }

    /**
     * Registers the given listener object so it is notified when host capabilities are negotiated.
     */
    public void addListener(CapabilitiesChangedListener listener) {
        assert !mCapabilitiesChangedListeners.contains(listener);
        mCapabilitiesChangedListeners.add(listener);

        // If we have already received the host capabilities before this listener was registered,
        // then fire the event for this listener immediately.
        if (mNegotiatedCapabilities != null) {
            // Clone the capabilities list passed to the caller to prevent them from mutating it.
            listener.onCapabilitiesChanged(new ArrayList<>(mNegotiatedCapabilities));
        }
    }

    /**
     * Removes the given listener object from the list of change listeners.
     */
    public void removeListener(CapabilitiesChangedListener listener) {
        assert mCapabilitiesChangedListeners.contains(listener);

        mCapabilitiesChangedListeners.remove(listener);
    }

    /**
     * Returns the ActivityLifecycleListener associated with the specified capability, if
     * |capability| is enabled and such a listener exists.
     *
     * Activities that call this method agree to appropriately notify the listener of lifecycle
     * events., thus supporting |capability|. This allows extensions like the CastExtensionHandler
     * to hook into an existing activity's lifecycle.
     */
    public ActivityLifecycleListener onActivityAcceptingListener(
            Activity activity, String capability) {

        ActivityLifecycleListener listener;

        if (isCapabilityEnabled(capability)) {
            for (ClientExtension ext : mClientExtensions) {
                if (ext.getCapability().equals(capability)) {
                    listener = ext.onActivityAcceptingListener(activity);
                    if (listener != null) return listener;
                }
            }
        }

        return new DummyActivityLifecycleListener();
    }

    /**
     * Receives the capabilities negotiated between client and host, creates the appropriate
     * extension handlers, and notifies registered listeners of the change.
     */
    public void setNegotiatedCapabilities(String capabilities) {
        mNegotiatedCapabilities = Arrays.asList(capabilities.split(" "));
        mClientExtensions.clear();
        if (isCapabilityEnabled(Capabilities.CAST_CAPABILITY)) {
            mClientExtensions.add(maybeCreateCastExtensionHandler());
        }

        // Clone the list of listeners to prevent problems if the callback calls back into this
        // object and removes itself from the list of listeners.
        List<CapabilitiesChangedListener> listeners =
                new ArrayList<>(mCapabilitiesChangedListeners);
        for (CapabilitiesChangedListener listener : listeners) {
            // Clone the capabilities list passed to the caller to prevent them from mutating it.
            listener.onCapabilitiesChanged(new ArrayList<>(mNegotiatedCapabilities));
        }
    }

    /**
     * Passes the deconstructed extension message to each ClientExtension in turn until the message
     * is handled or none remain. Returns true if the message was handled.
     */
    public boolean onExtensionMessage(String type, String data) {
        if (type == null || type.isEmpty()) {
            return false;
        }

        for (ClientExtension ext : mClientExtensions) {
            if (ext.onExtensionMessage(type, data)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Return true if the capability is enabled for this connection with the host.
     */
    private boolean isCapabilityEnabled(String capability) {
        return (mNegotiatedCapabilities != null && mNegotiatedCapabilities.contains(capability));
    }

    /**
     * Tries to reflectively instantiate a CastExtensionHandler object.
     *
     * Note: The ONLY reason this is done is that by default, the regular android application
     * will be built, without this experimental extension.
     */
    private ClientExtension maybeCreateCastExtensionHandler() {
        try {
            Class<?> cls = Class.forName("org.chromium.chromoting.CastExtensionHandler");
            return (ClientExtension) cls.newInstance();
        } catch (ClassNotFoundException e) {
            Log.w(TAG, "Failed to create CastExtensionHandler.");
            return new DummyClientExtension();
        } catch (InstantiationException e) {
            Log.w(TAG, "Failed to create CastExtensionHandler.");
            return new DummyClientExtension();
        } catch (IllegalAccessException e) {
            Log.w(TAG, "Failed to create CastExtensionHandler.");
            return new DummyClientExtension();
        }
    }
}
