// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.Build;
import android.security.NetworkSecurityPolicy;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.MinAndroidSdkLevel;

@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AndroidNetworkLibraryTest {
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testGetDnsStatus_searchDomains() {
        DnsStatus dnsStatus = AndroidNetworkLibrary.getDnsStatus(/* network= */ null);
        if (dnsStatus == null) {
            return;
        }

        String searchDomains = dnsStatus.getSearchDomains();
        if (searchDomains == null || searchDomains.isEmpty()) {
            return;
        }

        // Expect a comma-separated list of unknown length.
        String[] domains = searchDomains.split(",");
        for (String domain : domains) {
            Assert.assertNotEquals("", domain);
        }
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testGetEchMode_mapping() {
        class MockProxy extends AndroidNetworkLibrary.NetworkSecurityPolicyProxy {
            private int mMode;

            @Override
            public int getDomainEncryptionMode(String host) {
                return mMode;
            }

            public void setMode(int mode) {
                mMode = mode;
            }
        }
        MockProxy mockProxy = new MockProxy();
        var originalProxy = AndroidNetworkLibrary.NetworkSecurityPolicyProxy.getInstance();
        AndroidNetworkLibrary.NetworkSecurityPolicyProxy.setInstanceForTesting(mockProxy);

        try {
            mockProxy.setMode(NetworkSecurityPolicy.DOMAIN_ENCRYPTION_MODE_ENABLED);
            Assert.assertEquals(EchMode.OPPORTUNISTIC, AndroidNetworkLibrary.getEchMode("foo.com"));

            mockProxy.setMode(NetworkSecurityPolicy.DOMAIN_ENCRYPTION_MODE_DISABLED);
            Assert.assertEquals(EchMode.DISABLED, AndroidNetworkLibrary.getEchMode("foo.com"));

            mockProxy.setMode(NetworkSecurityPolicy.DOMAIN_ENCRYPTION_MODE_UNKNOWN);
            Assert.assertEquals(EchMode.OPPORTUNISTIC, AndroidNetworkLibrary.getEchMode("foo.com"));

            mockProxy.setMode(NetworkSecurityPolicy.DOMAIN_ENCRYPTION_MODE_OPPORTUNISTIC);
            Assert.assertEquals(EchMode.OPPORTUNISTIC, AndroidNetworkLibrary.getEchMode("foo.com"));
        } finally {
            AndroidNetworkLibrary.NetworkSecurityPolicyProxy.setInstanceForTesting(originalProxy);
        }
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    // Regression test for https://crbug.com/558378904.
    public void testGetEchMode_illegalArgumentException() {
        AndroidNetworkLibrary.NetworkSecurityPolicyProxy.setInstanceForTesting(
                new AndroidNetworkLibrary.NetworkSecurityPolicyProxy() {
                    @Override
                    public int getDomainEncryptionMode(String host) {
                        throw new IllegalArgumentException("Illegal hostname");
                    }
                });

        Assert.assertEquals(
                EchMode.OPPORTUNISTIC, AndroidNetworkLibrary.getEchMode(".example.com"));
        Assert.assertEquals(
                EchMode.OPPORTUNISTIC, AndroidNetworkLibrary.getEchMode("."));
    }
}
