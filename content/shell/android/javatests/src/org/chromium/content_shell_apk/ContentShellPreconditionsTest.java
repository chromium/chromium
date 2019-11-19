// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.PowerManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;

/**
 * Test that verifies preconditions for tests to run.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ContentShellPreconditionsTest {
    @Test
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH)
    @SuppressWarnings("deprecation")
    @MediumTest
    @Feature({"TestInfrastructure"})
    public void testScreenIsOn() {
        PowerManager pm = (PowerManager) InstrumentationRegistry.getContext().getSystemService(
                Context.POWER_SERVICE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            Assert.assertTrue("Many tests will fail if the screen is not on.", pm.isInteractive());
        } else {
            Assert.assertTrue("Many tests will fail if the screen is not on.", pm.isScreenOn());
        }
    }
}
