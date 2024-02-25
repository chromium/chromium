// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.content.Context;
import android.os.PowerManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;

/** Test that verifies preconditions for tests to run. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ContentShellPreconditionsTest {
    @Test
    @SuppressWarnings("deprecation")
    @MediumTest
    @Feature({"TestInfrastructure"})
    public void testScreenIsOn() {
        PowerManager pm =
                (PowerManager)
                        InstrumentationRegistry.getContext()
                                .getSystemService(Context.POWER_SERVICE);

        Assert.assertTrue("Many tests will fail if the screen is not on.", pm.isInteractive());
    }
}
