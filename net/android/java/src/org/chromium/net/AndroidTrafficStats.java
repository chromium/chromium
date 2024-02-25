// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.net.TrafficStats;
import android.os.Process;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** This class interacts with TrafficStats API provided by Android. */
@JNINamespace("net::android::traffic_stats")
public class AndroidTrafficStats {
    private AndroidTrafficStats() {}

    /**
     * @return Number of bytes transmitted since device boot. Counts packets across all network
     *         interfaces, and always increases monotonically since device boot. Statistics are
     *         measured at the network layer, so they include both TCP and UDP usage.
     */
    @CalledByNative
    private static long getTotalTxBytes() {
        long bytes = TrafficStats.getTotalTxBytes();
        return bytes != TrafficStats.UNSUPPORTED ? bytes : TrafficStatsError.ERROR_NOT_SUPPORTED;
    }

    /**
     * @return Number of bytes received since device boot. Counts packets across all network
     *         interfaces, and always increases monotonically since device boot. Statistics are
     *         measured at the network layer, so they include both TCP and UDP usage.
     */
    @CalledByNative
    private static long getTotalRxBytes() {
        long bytes = TrafficStats.getTotalRxBytes();
        return bytes != TrafficStats.UNSUPPORTED ? bytes : TrafficStatsError.ERROR_NOT_SUPPORTED;
    }

    /**
     * @return Number of bytes transmitted since device boot that were attributed to caller's UID.
     *         Counts packets across all network interfaces, and always increases monotonically
     *         since device boot. Statistics are measured at the network layer, so they include
     *         both TCP and UDP usage.
     */
    @CalledByNative
    private static long getCurrentUidTxBytes() {
        long bytes = TrafficStats.getUidTxBytes(Process.myUid());
        return bytes != TrafficStats.UNSUPPORTED ? bytes : TrafficStatsError.ERROR_NOT_SUPPORTED;
    }

    /**
     * @return Number of bytes received since device boot that were attributed to caller's UID.
     *         Counts packets across all network interfaces, and always increases monotonically
     *         since device boot. Statistics are measured at the network layer, so they include
     *         both TCP and UDP usage.
     */
    @CalledByNative
    private static long getCurrentUidRxBytes() {
        long bytes = TrafficStats.getUidRxBytes(Process.myUid());
        return bytes != TrafficStats.UNSUPPORTED ? bytes : TrafficStatsError.ERROR_NOT_SUPPORTED;
    }
}
