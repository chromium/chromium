// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryProcessType;

/**
 * Implementation of the interface {@link ChildProcessCreationParams}.
 */
public class ChildProcessCreationParamsImpl {
    private static final String EXTRA_LIBRARY_PROCESS_TYPE =
            "org.chromium.content.common.child_service_params.library_process_type";
    private static final String PRIVILEGED_SERVICES_NAME =
            "org.chromium.content.app.PrivilegedProcessService";
    private static final String SANDBOXED_SERVICES_NAME =
            "org.chromium.content.app.SandboxedProcessService";

    // Members should all be immutable to avoid worrying about thread safety.
    private static String sPackageNameForService;
    private static boolean sIsSandboxedServiceExternal;
    private static int sLibraryProcessType;
    private static boolean sBindToCallerCheck;
    // Use only the explicit WebContents.setImportance signal, and ignore other implicit
    // signals in content.
    private static boolean sIgnoreVisibilityForImportance;
    private static String sPrivilegedServicesName;
    private static String sSandboxedServicesName;

    private static boolean sInitialized;

    private ChildProcessCreationParamsImpl() {}

    /** Set params. This should be called once on start up. */
    public static void set(String packageNameForService, boolean isExternalSandboxedService,
            int libraryProcessType, boolean bindToCallerCheck,
            boolean ignoreVisibilityForImportance, String privilegedServicesName,
            String sandboxedServicesName) {
        assert !sInitialized;
        sPackageNameForService = packageNameForService;
        sIsSandboxedServiceExternal = isExternalSandboxedService;
        sLibraryProcessType = libraryProcessType;
        sBindToCallerCheck = bindToCallerCheck;
        sIgnoreVisibilityForImportance = ignoreVisibilityForImportance;
        sPrivilegedServicesName =
                privilegedServicesName == null ? PRIVILEGED_SERVICES_NAME : privilegedServicesName;
        sSandboxedServicesName =
                sandboxedServicesName == null ? SANDBOXED_SERVICES_NAME : sandboxedServicesName;
        sInitialized = true;
    }

    public static void addIntentExtras(Bundle extras) {
        if (sInitialized) extras.putInt(EXTRA_LIBRARY_PROCESS_TYPE, sLibraryProcessType);
    }

    public static String getPackageNameForService() {
        return sInitialized ? sPackageNameForService
                            : ContextUtils.getApplicationContext().getPackageName();
    }

    public static boolean getIsSandboxedServiceExternal() {
        return sInitialized && sIsSandboxedServiceExternal;
    }

    public static boolean getBindToCallerCheck() {
        return sInitialized && sBindToCallerCheck;
    }

    public static boolean getIgnoreVisibilityForImportance() {
        return sInitialized && sIgnoreVisibilityForImportance;
    }

    public static int getLibraryProcessType(Bundle extras) {
        return extras.getInt(EXTRA_LIBRARY_PROCESS_TYPE, LibraryProcessType.PROCESS_CHILD);
    }

    public static String getPrivilegedServicesName() {
        return sInitialized ? sPrivilegedServicesName : PRIVILEGED_SERVICES_NAME;
    }

    public static String getSandboxedServicesName() {
        return sInitialized ? sSandboxedServicesName : SANDBOXED_SERVICES_NAME;
    }
}
