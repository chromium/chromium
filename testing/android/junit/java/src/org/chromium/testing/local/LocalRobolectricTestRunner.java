// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.junit.runners.model.InitializationError;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/**
 * A custom Robolectric Junit4 Test Runner with Chromium specific settings.
 */
public class LocalRobolectricTestRunner extends RobolectricTestRunner {
    public static final int DEFAULT_SDK = 28;
    private static final String DEFAULT_PACKAGE_NAME = "org.robolectric.default";

    static {
        // Setting robolectric.offline which tells Robolectric to look for runtime dependency
        // JARs from a local directory and to not download them from Maven.
        System.setProperty("robolectric.offline", "true");
    }

    public LocalRobolectricTestRunner(Class<?> testClass) throws InitializationError {
        super(testClass);
    }

    @Override
    protected Config buildGlobalConfig() {
        return new Config.Builder()
                .setSdk(DEFAULT_SDK)
                // Shadows to fix robolectric shortcomings.
                .setShadows(new Class[] {CustomShadowApplicationPackageManager.class})
                .build();
    }
}
