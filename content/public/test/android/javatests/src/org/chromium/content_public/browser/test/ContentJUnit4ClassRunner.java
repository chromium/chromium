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
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.test.util.UiDisableIfSkipCheck;
import org.chromium.ui.test.util.UiRestriction;

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
        UiRestriction.registerChecks(mRestrictionSkipCheck);
        DeviceRestriction.registerChecks(mRestrictionSkipCheck);
        GmsCoreVersionRestriction.registerChecks(mRestrictionSkipCheck);

        // Display ui scale-up on auto for tests by default, individual tests can restore this
        // scaling.
        DisplayUtil.setUiScalingFactorForAutomotiveForTesting(1.0f);
        EmbeddedTestServer.initCerts();
    }

    @Override
    protected List<SkipCheck> getSkipChecks() {
        return addToList(
                super.getSkipChecks(),
                new UiDisableIfSkipCheck(InstrumentationRegistry.getTargetContext()));
    }
}
