// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.net.InetAddress;
import java.util.List;

/**
 * Class to access DNS server configuration.
 */
@JNINamespace("net::android")
public class DnsStatus {
    private final List<InetAddress> mDnsServers;

    private final boolean mPrivateDnsActive;

    private final String mPrivateDnsServerName;

    public DnsStatus(
            List<InetAddress> dnsServers, boolean privateDnsActive, String privateDnsServerName) {
        mDnsServers = dnsServers;
        mPrivateDnsActive = privateDnsActive;
        mPrivateDnsServerName = (privateDnsServerName != null) ? privateDnsServerName : "";
    }

    @CalledByNative
    public byte[][] getDnsServers() {
        byte[][] dnsServers = new byte[mDnsServers.size()][];
        for (int i = 0; i < mDnsServers.size(); i++) {
            dnsServers[i] = mDnsServers.get(i).getAddress();
        }
        return dnsServers;
    }

    @CalledByNative
    public boolean getPrivateDnsActive() {
        return mPrivateDnsActive;
    }

    @CalledByNative
    public String getPrivateDnsServerName() {
        return mPrivateDnsServerName;
    }
}
