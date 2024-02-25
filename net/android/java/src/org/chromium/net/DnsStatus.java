// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.net.InetAddress;
import java.util.List;

/** Class to access DNS server configuration. */
@JNINamespace("net::android")
public class DnsStatus {
    private final List<InetAddress> mDnsServers;

    private final boolean mPrivateDnsActive;

    private final String mPrivateDnsServerName;

    private final String mSearchDomains;

    public DnsStatus(
            List<InetAddress> dnsServers,
            boolean privateDnsActive,
            String privateDnsServerName,
            String searchDomains) {
        mDnsServers = dnsServers;
        mPrivateDnsActive = privateDnsActive;
        mPrivateDnsServerName = (privateDnsServerName != null) ? privateDnsServerName : "";
        mSearchDomains = (searchDomains != null) ? searchDomains : "";
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

    @CalledByNative
    public String getSearchDomains() {
        return mSearchDomains;
    }
}
