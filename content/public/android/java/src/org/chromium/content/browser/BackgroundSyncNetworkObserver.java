// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.pm.PackageManager;
import android.os.Process;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifierAutoDetect;
import org.chromium.net.RegistrationPolicyAlwaysRegister;

import java.util.ArrayList;
import java.util.List;

/**
 * Contains the Java code used by the BackgroundSyncNetworkObserverAndroid C++ class.
 *
 * The purpose of this class is to listen for and forward network connectivity events to the
 * BackgroundSyncNetworkObserverAndroid objects even when the application is paused. The standard
 * NetworkChangeNotifier does not listen for connectivity events when the application is paused.
 *
 * This class maintains a NetworkChangeNotifierAutoDetect, which exists for as long as any
 * BackgroundSyncNetworkObserverAndroid objects are registered.
 *
 * This class lives on the main thread.
 */
@JNINamespace("content")
@VisibleForTesting
public class BackgroundSyncNetworkObserver implements NetworkChangeNotifierAutoDetect.Observer {
    private static final String TAG = "BgSyncNetObserver";
    private static boolean sSetConnectionTypeForTesting;
    private NetworkChangeNotifierAutoDetect mNotifier;

    // The singleton instance.
    @SuppressLint("StaticFieldLeak")
    private static BackgroundSyncNetworkObserver sInstance;

    // List of native observers. These are each called when the network state changes.
    private List<Long> mNativePtrs;

    private @ConnectionType int mLastBroadcastConnectionType;
    private boolean mHasBroadcastConnectionType;

    public static void setConnectionTypeForTesting(@ConnectionType int connectionType) {
        sSetConnectionTypeForTesting = true;
        getBackgroundSyncNetworkObserver().broadcastNetworkChangeIfNecessary(connectionType);
    }

    private BackgroundSyncNetworkObserver() {
        ThreadUtils.assertOnUiThread();
        mNativePtrs = new ArrayList<Long>();
        sSetConnectionTypeForTesting = false;
    }

    private static boolean canCreateObserver() {
        return ApiCompatibilityUtils.checkPermission(
                        ContextUtils.getApplicationContext(),
                        Manifest.permission.ACCESS_NETWORK_STATE,
                        Process.myPid(),
                        Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    private static BackgroundSyncNetworkObserver getBackgroundSyncNetworkObserver() {
        if (sInstance == null) {
            sInstance = new BackgroundSyncNetworkObserver();
        }
        return sInstance;
    }

    @CalledByNative
    private static BackgroundSyncNetworkObserver createObserver(long nativePtr) {
        ThreadUtils.assertOnUiThread();

        getBackgroundSyncNetworkObserver().registerObserver(nativePtr);
        return sInstance;
    }

    private void registerObserver(final long nativePtr) {
        ThreadUtils.assertOnUiThread();
        if (!canCreateObserver()) {
            RecordHistogram.recordBooleanHistogram(
                    "BackgroundSync.NetworkObserver.HasPermission", false);
            return;
        }

        // Create the NetworkChangeNotifierAutoDetect if it does not exist already.
        if (mNotifier == null) {
            mNotifier =
                    new NetworkChangeNotifierAutoDetect(
                            this, new RegistrationPolicyAlwaysRegister());
            RecordHistogram.recordBooleanHistogram(
                    "BackgroundSync.NetworkObserver.HasPermission", true);
        }
        mNativePtrs.add(nativePtr);
        // TODO(crbug.com/40936429): remove this call if it is not necessary.
        mNotifier.updateCurrentNetworkState();
        BackgroundSyncNetworkObserverJni.get()
                .notifyConnectionTypeChanged(
                        nativePtr,
                        BackgroundSyncNetworkObserver.this,
                        mNotifier.getCurrentNetworkState().getConnectionType());
    }

    @CalledByNative
    private void removeObserver(long nativePtr) {
        ThreadUtils.assertOnUiThread();
        mNativePtrs.remove(nativePtr);
        // Destroy the NetworkChangeNotifierAutoDetect if there are no more observers.
        if (mNativePtrs.size() == 0 && mNotifier != null) {
            mNotifier.destroy();
            mNotifier = null;
        }
    }

    private void broadcastNetworkChangeIfNecessary(@ConnectionType int newConnectionType) {
        if (mHasBroadcastConnectionType && newConnectionType == mLastBroadcastConnectionType) {
            return;
        }

        mHasBroadcastConnectionType = true;
        mLastBroadcastConnectionType = newConnectionType;
        for (Long nativePtr : mNativePtrs) {
            BackgroundSyncNetworkObserverJni.get()
                    .notifyConnectionTypeChanged(
                            nativePtr, BackgroundSyncNetworkObserver.this, newConnectionType);
        }
    }

    @Override
    public void onConnectionTypeChanged(@ConnectionType int newConnectionType) {
        ThreadUtils.assertOnUiThread();
        if (sSetConnectionTypeForTesting) return;

        broadcastNetworkChangeIfNecessary(newConnectionType);
    }

    @Override
    public void onConnectionCostChanged(int newConnectionCost) {}

    @Override
    public void onConnectionSubtypeChanged(int newConnectionSubtype) {}

    @Override
    public void onNetworkConnect(long netId, @ConnectionType int connectionType) {
        ThreadUtils.assertOnUiThread();
        if (sSetConnectionTypeForTesting) return;

        // If we're in doze mode (N+ devices), onConnectionTypeChanged may not
        // be called, but this function should. So update the connection type
        // if necessary.
        broadcastNetworkChangeIfNecessary(connectionType);
    }

    @Override
    public void onNetworkSoonToDisconnect(long netId) {}

    @Override
    public void onNetworkDisconnect(long netId) {
        ThreadUtils.assertOnUiThread();
        if (sSetConnectionTypeForTesting) return;

        // If we're in doze mode (N+ devices), onConnectionTypeChanged may not
        // be called, but this function should. So update the connection type
        // if necessary.
        // TODO(crbug.com/40936429): remove this call if it is not necessary.
        mNotifier.updateCurrentNetworkState();
        broadcastNetworkChangeIfNecessary(mNotifier.getCurrentNetworkState().getConnectionType());
    }

    @Override
    public void purgeActiveNetworkList(long[] activeNetIds) {}

    @NativeMethods
    interface Natives {
        @NativeClassQualifiedName("BackgroundSyncNetworkObserverAndroid::Observer")
        void notifyConnectionTypeChanged(
                long nativePtr, BackgroundSyncNetworkObserver caller, int newConnectionType);
    }
}
