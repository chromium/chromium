// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Simplified internal representation of Android's android.net.NetworkCapabilities (as it's hidden).
 * This class contains only the information from that class that we need and is safer than moving
 * raw longs or int[]s around.
 */
@NullMarked
/* package */ class NetworkCapabilitiesWrapper {
    // Either this wrapped object should be set or the fields below it should be set. Never both.
    private final android.net.@Nullable NetworkCapabilities mWrapped;
    private final long mNetworkCapabilities;
    private final long mTransportTypes;

    /**
     * Construct a new NetworkCapabilitiesWrapper.
     *
     * @param networkCapabilities the return value of {@code NetworkRequest.getCapabilities()}.
     * @param transportTypes the return value of {@code NetworkRequest.getTransportTypes()}.
     */
    NetworkCapabilitiesWrapper(int[] networkCapabilities, int[] transportTypes) {
        mNetworkCapabilities = packIntoLong(networkCapabilities);
        mTransportTypes = packIntoLong(transportTypes);
        mWrapped = null;
    }

    NetworkCapabilitiesWrapper(android.net.@Nullable NetworkCapabilities other) {
        mWrapped = other;
        mNetworkCapabilities = -1;
        mTransportTypes = -1;
    }

    public boolean hasCapability(int capability) {
        if (mWrapped != null) {
            return mWrapped.hasCapability(capability);
        }
        // Note that we don't do any range checking. However, as long as the capability is within
        // the size of a long, it should return false on any out of range bits.
        return capability >= 0
                && capability < Long.SIZE
                && ((mNetworkCapabilities & (1L << capability)) != 0);
    }

    public boolean hasTransport(int transportType) {
        if (mWrapped != null) {
            return mWrapped.hasTransport(transportType);
        }
        return transportType >= 0
                && transportType < Long.SIZE
                && ((mTransportTypes & (1 << transportType)) != 0);
    }

    private static long packIntoLong(int[] bits) {
        // The flags are packed into an int[] where each int represents the position of a 1 in the
        // resulting long. It's much easier to query using the long representation so convert it
        // back to a long.
        long packed = 0;
        for (int b : bits) {
            packed |= (1L << b);
        }
        return packed;
    }
}
