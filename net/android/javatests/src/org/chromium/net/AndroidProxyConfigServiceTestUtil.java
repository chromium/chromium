// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.Looper;

import org.jni_zero.CalledByNative;

/** Utility functions for testing features implemented in ProxyConfigServiceAndroid. */
public class AndroidProxyConfigServiceTestUtil {
    /** Helper for tests that prepares the Looper on the current thread. */
    @CalledByNative
    private static void prepareLooper() {
        if (Looper.myLooper() == null) {
            Looper.prepare();
        }
    }
}
