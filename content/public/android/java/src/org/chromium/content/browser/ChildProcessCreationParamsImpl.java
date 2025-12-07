// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Build;
import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ContentFeatureList;

/** Implementation of the interface {@link ChildProcessCreationParams}. */
@NullMarked
public class ChildProcessCreationParamsImpl {
    private static final String EXTRA_LIBRARY_PROCESS_TYPE =
            "org.chromium.content.common.child_service_params.library_process_type";
    private static final String PRIVILEGED_SERVICES_NAME =
            "org.chromium.content.app.PrivilegedProcessService";
    private static final String SANDBOXED_SERVICES_NAME =
            "org.chromium.content.app.SandboxedProcessService";
    private static final String NATIVE_SANDBOXED_SERVICES_NAME =
            "org.chromium.content.app.NativeServiceSandboxedProcessService";

    // Members should all be immutable to avoid worrying about thread safety.
    private static @Nullable String sPackageNameForPrivilegedService;
    private static @Nullable String sPackageNameForSandboxedService;
    private static boolean sIsSandboxedServiceExternal;
    private static int sLibraryProcessType;
    private static boolean sBindToCallerCheck;
    // Use only the explicit WebContents.setImportance signal, and ignore other implicit
    // signals in content.
    private static boolean sIgnoreVisibilityForImportance;

    private static boolean sInitialized;

    private ChildProcessCreationParamsImpl() {}

    /** Set params. This should be called once on start up. */
    public static void set(
            String privilegedPackageName,
            String sandboxedPackageName,
            boolean isExternalSandboxedService,
            int libraryProcessType,
            boolean bindToCallerCheck,
            boolean ignoreVisibilityForImportance) {
        assert !sInitialized;
        sPackageNameForPrivilegedService = privilegedPackageName;
        sPackageNameForSandboxedService = sandboxedPackageName;
        sIsSandboxedServiceExternal = isExternalSandboxedService;
        sLibraryProcessType = libraryProcessType;
        sBindToCallerCheck = bindToCallerCheck;
        sIgnoreVisibilityForImportance = ignoreVisibilityForImportance;
        sInitialized = true;
    }

    public static void addIntentExtras(Bundle extras) {
        if (sInitialized) extras.putInt(EXTRA_LIBRARY_PROCESS_TYPE, sLibraryProcessType);
    }

    public static int getLibraryProcessType() {
        return sInitialized ? sLibraryProcessType : LibraryProcessType.PROCESS_CHILD;
    }

    public static String getPackageNameForPrivilegedService() {
        return sPackageNameForPrivilegedService != null
                ? sPackageNameForPrivilegedService
                : ContextUtils.getApplicationContext().getPackageName();
    }

    public static String getPackageNameForSandboxedService() {
        return sPackageNameForSandboxedService != null
                ? sPackageNameForSandboxedService
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
        return PRIVILEGED_SERVICES_NAME;
    }

    public static String getSandboxedServicesName() {
        if (BuildConfig.JAVALESS_RENDERERS_AVAILABLE
                && Build.VERSION.SDK_INT >= 35
                && ContentFeatureList.sJavalessRenderers.isEnabled()) {
            return NATIVE_SANDBOXED_SERVICES_NAME;
        }
        return SANDBOXED_SERVICES_NAME;
    }
}
