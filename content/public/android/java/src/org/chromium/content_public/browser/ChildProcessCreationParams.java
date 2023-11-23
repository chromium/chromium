// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.ChildProcessCreationParamsImpl;

/**
 * Allows specifying the package name for looking up child services
 * configuration and classes into (if it differs from the application package
 * name, like in the case of Android WebView). Also allows specifying additional
 * child service binding flags.
 */
public final class ChildProcessCreationParams {
    /**
     * Set params. This should be called once on start up. If null is passed for
     * privilegedServicesName or sandboxedServicesName, the default service names will be used.
     */
    public static void set(
            String privilegedPackageName,
            String privilegedServicesName,
            String sandboxedPackageName,
            String sandboxedServicesName,
            boolean isExternalSandboxedService,
            int libraryProcessType,
            boolean bindToCallerCheck,
            boolean ignoreVisibilityForImportance) {
        ChildProcessCreationParamsImpl.set(
                privilegedPackageName,
                privilegedServicesName,
                sandboxedPackageName,
                sandboxedServicesName,
                isExternalSandboxedService,
                libraryProcessType,
                bindToCallerCheck,
                ignoreVisibilityForImportance);
    }
}
