// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Initializes the Chromium remoting library, and provides JNI calls into it.
 * All interaction with the native code is centralized in this class.
 */
@JNINamespace("remoting")
public class JniInterface {
    private static final String TAG = "Chromoting";
    private static final String LIBRARY_NAME = "remoting_client_jni";

    /**
     * To be called once from the Application context singleton. Loads and initializes the native
     * code. Called on the UI thread.
     */
    public static void loadLibrary() {
        try {
            System.loadLibrary(LIBRARY_NAME);
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Couldn't load " + LIBRARY_NAME + ", trying " + LIBRARY_NAME + ".cr");
            System.loadLibrary(LIBRARY_NAME + ".cr");
        }
        JniInterfaceJni.get().loadNative();
    }

    @NativeMethods
    interface Natives {
        /** Performs the native portion of the initialization. */
        void loadNative();
    }
}
