// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import javax.annotation.concurrent.GuardedBy;

/**
 * A NetworkActivationRequest asks to activate and maintain an Internet-connected network interface
 * matching some specified constraints. As long as the object exists and is registered, the best
 * available matching network (if any, and according to the sytsem) will be kept active by the
 * system. This may mean, for example, keeping a cellular radio in a higher-power state even while a
 * Wi-Fi network is selected as the device's default, because a NetworkActivationRequest requests a
 * mobile network.
 */
@JNINamespace("net::android")
public class NetworkActivationRequest extends NetworkCallback {
    private final ConnectivityManager mConnectivityManager;
    private final Object mNativePtrLock = new Object();

    @GuardedBy("mNativePtrLock")
    private long mNativePtr;

    /**
     * Initiates a new network request for any network with the given {@code transportType}. The
     * system will attempt to select an Internet-connected network which uses this type of
     * transport and will keep that network active as long as this object exists or until the system
     * selects and notifies us of a better network for this request.
     */
    private NetworkActivationRequest(long nativePtr, int transportType) {
        mConnectivityManager =
                (ConnectivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        if (mConnectivityManager == null) return;

        try {
            mConnectivityManager.requestNetwork(
                    new NetworkRequest.Builder()
                            .addTransportType(transportType)
                            .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                            .build(),
                    this);

            // Only track the native pointer (and thus allow future unregistration) if the above
            // call succeeds.
            mNativePtr = nativePtr;
        } catch (SecurityException e) {
            // On some older devices the CHANGE_NETWORK_STATE permission is not sufficient to allow
            // use of {@code requestNetwork} above and it will throw a SecurityException. Do nothing
            // in this case.
        }
    }

    /**
     * Unregisters this NetworkCallback from the system. Note that this synchronizes the reset of
     * {@code mNativePtr} to ensure that no calls back to C++ will ever be issued again once this
     * returns. This is important because the owning native object is thread-affine and may live on
     * a different thread than that which invokes the NetworkCallback.
     */
    @CalledByNative
    private void unregister() {
        boolean shouldUnregister = false;
        synchronized (mNativePtrLock) {
            shouldUnregister = mNativePtr != 0;
            mNativePtr = 0;
        }
        if (shouldUnregister) mConnectivityManager.unregisterNetworkCallback(this);
    }

    @Override
    public void onAvailable(Network network) {
        synchronized (mNativePtrLock) {
            if (mNativePtr == 0) return;
            NetworkActivationRequestJni.get().notifyAvailable(
                    mNativePtr, NetworkChangeNotifierAutoDetect.networkToNetId(network));
        }
    }

    /**
     * Creates a new {@code NetworkActivationRequest} for any available mobile network and returns
     * a reference to it for ownership by a JNI caller. See the C++
     * {@code NetworkActivationRequest} type.
     */
    @CalledByNative
    public static NetworkActivationRequest createMobileNetworkRequest(long nativePtr) {
        return new NetworkActivationRequest(nativePtr, NetworkCapabilities.TRANSPORT_CELLULAR);
    }

    @NativeMethods
    interface Natives {
        void notifyAvailable(long nativeNetworkActivationRequest, long netId);
    }
}
