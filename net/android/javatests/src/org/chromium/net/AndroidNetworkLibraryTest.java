// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.Build;

import androidx.test.filters.MediumTest;

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
}
