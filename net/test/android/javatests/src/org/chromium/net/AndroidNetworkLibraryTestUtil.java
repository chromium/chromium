// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.annotations.CalledByNative;

/**
 * Utility functions for testing features implemented in AndroidNetworkLibrary.
 */
public class AndroidNetworkLibraryTestUtil {
    private static int sPerHostCleartextCheckCount;
    private static int sDefaultCleartextCheckCount;
    /**
     * Helper for tests that simulates an app controlling cleartext traffic on M and newer.
     */
    @CalledByNative
    public static void setUpSecurityPolicyForTesting(boolean cleartextPermitted) {
        sDefaultCleartextCheckCount = 0;
        sPerHostCleartextCheckCount = 0;
        AndroidNetworkLibrary.NetworkSecurityPolicyProxy.setInstanceForTesting(
                new AndroidNetworkLibrary.NetworkSecurityPolicyProxy() {
                    @Override
                    public boolean isCleartextTrafficPermitted(String host) {
                        ++sPerHostCleartextCheckCount;
                        if (host.startsWith(".")) {
                            throw new IllegalArgumentException("hostname can not start with .");
                        }
                        return cleartextPermitted;
                    }
                    @Override
                    public boolean isCleartextTrafficPermitted() {
                        ++sDefaultCleartextCheckCount;
                        return cleartextPermitted;
                    }
                });
    }

    @CalledByNative
    private static int getPerHostCleartextCheckCount() {
        return sPerHostCleartextCheckCount;
    }

    @CalledByNative
    private static int getDefaultCleartextCheckCount() {
        return sDefaultCleartextCheckCount;
    }
}
