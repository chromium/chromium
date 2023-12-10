// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test;

import androidx.test.InstrumentationRegistry;

import org.junit.runners.model.InitializationError;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.SkipCheck;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.test.util.DeviceRestrictionSkipCheck;
import org.chromium.ui.test.util.UiDisableIfSkipCheck;
import org.chromium.ui.test.util.UiRestrictionSkipCheck;

import java.util.List;

/** A custom runner for //content JUnit4 tests. */
public class ContentJUnit4ClassRunner extends BaseJUnit4ClassRunner {
    /**
     * Create a ContentJUnit4ClassRunner to run {@code klass} and initialize values
     *
     * @throws InitializationError if the test class malformed
     */
    public ContentJUnit4ClassRunner(final Class<?> klass) throws InitializationError {
        super(klass);

        // Display ui scale-up on auto for tests by default, individual tests can restore this
        // scaling.
        DisplayUtil.setUiScalingFactorForAutomotiveForTesting(1.0f);
    }

    @Override
    protected List<SkipCheck> getSkipChecks() {
        return addToList(
                super.getSkipChecks(),
                new UiRestrictionSkipCheck(InstrumentationRegistry.getTargetContext()),
                new DeviceRestrictionSkipCheck(InstrumentationRegistry.getTargetContext()),
                new UiDisableIfSkipCheck(InstrumentationRegistry.getTargetContext()));
    }

    /** Change this static function to add default {@code PreTestHook}s. */
    @Override
    protected List<TestHook> getPreTestHooks() {
        return addToList(super.getPreTestHooks(), new ChildProcessAllocatorSettingsHook());
    }

    @Override
    protected List<ClassHook> getPreClassHooks() {
        return addToList(super.getPreClassHooks(), EmbeddedTestServer.getPreClassHook());
    }
}
