// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test;

import android.content.Context;

import org.junit.runners.model.FrameworkMethod;

import org.chromium.base.test.BaseJUnit4ClassRunner.TestHook;
import org.chromium.content.browser.ChildProcessLauncherHelperImpl;

/**
 * PreTestHook used to register the ChildProcessAllocatorSettings annotation.
 *
 * TODO(yolandyan): convert this to TestRule once content tests are changed JUnit4
 * */
public final class ChildProcessAllocatorSettingsHook implements TestHook {
    @Override
    public void run(Context targetContext, FrameworkMethod testMethod) {
        ChildProcessAllocatorSettings annotation =
                testMethod.getAnnotation(ChildProcessAllocatorSettings.class);
        if (annotation != null) {
            ChildProcessLauncherHelperImpl.setSandboxServicesSettingsForTesting(null /* factory */,
                    annotation.sandboxedServiceCount(), annotation.sandboxedServiceName());
        }
    }
}
