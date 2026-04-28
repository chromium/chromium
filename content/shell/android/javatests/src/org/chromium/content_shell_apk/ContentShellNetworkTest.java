// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.net.NetworkChangeNotifier;

/** Tests that the NetworkChangeNotifier is correctly initialized in ContentShell. */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests NetworkChangeNotifier initialization which happens once per process.")
public class ContentShellNetworkTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    @Test
    @SmallTest
    @Feature({"Main"})
    public void testNetworkChangeNotifierInitialized() {
        mActivityTestRule.launchContentShellWithUrl("about:blank");
        Assert.assertTrue(
                "NetworkChangeNotifier should be initialized",
                NetworkChangeNotifier.isInitialized());
        Assert.assertNotNull(
                "NetworkChangeNotifier auto-detector should be active",
                NetworkChangeNotifier.getAutoDetectorForTest());
    }
}
