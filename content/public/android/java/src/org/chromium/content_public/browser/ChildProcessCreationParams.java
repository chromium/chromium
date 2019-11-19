// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.ChildProcessCreationParamsImpl;

/**
 * Allows specifying the package name for looking up child services
 * configuration and classes into (if it differs from the application package
 * name, like in the case of Android WebView). Also allows specifying additional
 * child service binging flags.
 */
public final class ChildProcessCreationParams {
    /**
     * Set params. This should be called once on start up. If null is passed for
     * privilegedServicesName or sandboxedServicesName, the default service names will be used.
     */
    public static void set(String packageNameForService, boolean isExternalSandboxedService,
            int libraryProcessType, boolean bindToCallerCheck,
            boolean ignoreVisibilityForImportance, String privilegedServicesName,
            String sandboxedServicesName) {
        ChildProcessCreationParamsImpl.set(packageNameForService, isExternalSandboxedService,
                libraryProcessType, bindToCallerCheck, ignoreVisibilityForImportance,
                privilegedServicesName, sandboxedServicesName);
    }
}
