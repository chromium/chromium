// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.net.ConnectivityManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;

/**
 * Triggers updates to the underlying network state in Chrome alongside NetworkChangeNotifier.
 *
 * Differently from NetworkChangeNotifier, this handles default network active type of
 * notifications. These are handled separately for two main reasons:
 * 1. They are more expensive to listen to and currently only used by bidi streams connection status
 *    check. Hence, we only enable them when they are actually required.
 * 2. The default network active value is not cacheable, what observers are interested in is the
 *    event itself (usually to send out packets which were being batched). For this reason the
 *    architecture of NetworkChangeNotifier doesn't make much sense for this notification type.
 *
 * Note: ConnectivityManager.OnNetworkActiveListener has been introduced in Android API level 21, so
 * loading this class will fail on older Android versions (no sdk checks are needed for this
 * reason).
 */
@JNINamespace("net")
public class NetworkActiveNotifier implements ConnectivityManager.OnNetworkActiveListener {
    private final ConnectivityManager mConnectivityManager;
    // Native-side observer of the default network active events.
    private final long mNativeNetworkActiveObserver;
    // Used for testing, keeps track of when platform notification are enabled (or disabled).
    private boolean mAreNotificationsEnabled;

    /** Used to build a Java object from native code. */
    @CalledByNative
    public static NetworkActiveNotifier build(long nativeNetworkActiveNotifier) {
        return new NetworkActiveNotifier(nativeNetworkActiveNotifier);
    }

    @CalledByNative
    public void enableNotifications() {
        mAreNotificationsEnabled = true;
        mConnectivityManager.addDefaultNetworkActiveListener(this);
    }

    @CalledByNative
    public void disableNotifications() {
        mAreNotificationsEnabled = false;
        mConnectivityManager.removeDefaultNetworkActiveListener(this);
    }

    @CalledByNative
    public boolean isDefaultNetworkActive() {
        return mConnectivityManager.isDefaultNetworkActive();
    }

    @Override
    public void onNetworkActive() {
        NetworkActiveNotifierJni.get().notifyOfDefaultNetworkActive(mNativeNetworkActiveObserver);
    }

    /** For testing, called by native code to trigger a fake platform notification. */
    @CalledByNative
    public void fakeDefaultNetworkActive() {
        if (mAreNotificationsEnabled) {
            // Platform notifications should only be received when enabled.
            onNetworkActive();
        }
    }

    @NativeMethods
    interface Natives {
        @NativeClassQualifiedName("NetworkChangeNotifierDelegateAndroid")
        void notifyOfDefaultNetworkActive(long nativePtr);
    }

    private NetworkActiveNotifier(long nativeNetworkActiveNotifier) {
        mNativeNetworkActiveObserver = nativeNetworkActiveNotifier;
        Context ctx = ContextUtils.getApplicationContext();
        mConnectivityManager =
                (ConnectivityManager) ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
    }
}
