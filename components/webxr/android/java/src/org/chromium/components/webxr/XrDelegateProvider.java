// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.base.Log;

/** Class used to create XrDelegate instances. */
public class XrDelegateProvider {
    private static final String TAG = "XrDelegateProvider";
    private static final boolean DEBUG_LOGS = false;

    /**
     * Cached instance of XrDelegate implementation. It is ok to cache since the
     * inclusion of XrDelegateImpl is controlled at build time.
     */
    private static XrDelegate sDelegate;

    /** True if sDelegate already contains cached result, false otherwise. */
    private static boolean sDelegateInitialized;

    /** Provides an instance of XrDelegate. */
    public static XrDelegate getDelegate() {
        if (DEBUG_LOGS) {
            Log.i(
                    TAG,
                    "XrDelegate.getDelegate(): sDelegateInitialized="
                            + sDelegateInitialized
                            + ", is sDelegate null? "
                            + (sDelegate == null));
        }

        if (sDelegateInitialized) return sDelegate;

        try {
            sDelegate =
                    (XrDelegate)
                            Class.forName("org.chromium.components.webxr.XrDelegateImpl")
                                    .newInstance();
        } catch (ClassNotFoundException e) {
        } catch (InstantiationException e) {
        } catch (IllegalAccessException e) {
        } finally {
            sDelegateInitialized = true;
        }

        if (DEBUG_LOGS) {
            Log.i(TAG, "Is sDelegate null? " + (sDelegate == null));
        }

        return sDelegate;
    }
}
