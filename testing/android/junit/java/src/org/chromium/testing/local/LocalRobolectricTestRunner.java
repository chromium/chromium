// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.junit.runners.model.InitializationError;
import org.robolectric.RobolectricTestRunner;

/**
 * Most test cases should prefer {@link org.chromium.base.test.BaseRobolectricTestRunner}
 * in order to initialize base globals.
 * ParameterizedRobolectricTestRunner does not pick up settings from this class, so configuring
 * defaults via command-line flags / .properties files is what we've moved to instead of setting
 * things here.
 */
public class LocalRobolectricTestRunner extends RobolectricTestRunner {
    public LocalRobolectricTestRunner(Class<?> testClass) throws InitializationError {
        super(testClass);
    }
}
