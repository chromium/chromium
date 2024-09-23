// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.content.Context;

import com.google.ar.core.ArCoreApk;

import org.chromium.base.StrictModeContext;
import org.chromium.build.annotations.UsedByReflection;

@UsedByReflection("ArCoreInstallUtils.java")
class ArCoreShimImpl implements ArCoreShim {
    @UsedByReflection("ArCoreInstallUtils.java")
    public ArCoreShimImpl() {}

    @Override
    public @InstallStatus int requestInstall(Activity activity, boolean userRequestedInstall)
            throws UnavailableDeviceNotCompatibleException,
                    UnavailableUserDeclinedInstallationException {
        try {
            ArCoreApk.InstallStatus installStatus =
                    ArCoreApk.getInstance().requestInstall(activity, userRequestedInstall);
            return mapArCoreApkInstallStatus(installStatus);
        } catch (com.google.ar.core.exceptions.UnavailableDeviceNotCompatibleException e) {
            throw new UnavailableDeviceNotCompatibleException(e);
        } catch (com.google.ar.core.exceptions.UnavailableUserDeclinedInstallationException e) {
            throw new UnavailableUserDeclinedInstallationException(e);
        }
    }

    @Override
    public @ArCoreAvailability int checkAvailability(Context applicationContext) {
        // ARCore's checkAvailability reads shared preferences via ArCoreContentProvider, need to
        // turn off strict mode to allow that.
        // TODO(crbug.com/40666477): Remove the disk write context when the disk write is
        // fixed on ArCore's end.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads();
                StrictModeContext ignored2 = StrictModeContext.allowDiskWrites()) {
            ArCoreApk.Availability availability =
                    ArCoreApk.getInstance().checkAvailability(applicationContext);
            return mapArCoreApkAvailability(availability);
        }
    }

    private @InstallStatus int mapArCoreApkInstallStatus(ArCoreApk.InstallStatus installStatus) {
        switch (installStatus) {
            case INSTALLED:
                return InstallStatus.INSTALLED;
            case INSTALL_REQUESTED:
                return InstallStatus.INSTALL_REQUESTED;
            default:
                throw new RuntimeException(
                        String.format("Unknown value of InstallStatus: %s", installStatus));
        }
    }

    private @ArCoreAvailability int mapArCoreApkAvailability(ArCoreApk.Availability availability) {
        switch (availability) {
            case SUPPORTED_APK_TOO_OLD:
                return ArCoreAvailability.SUPPORTED_APK_TOO_OLD;
            case SUPPORTED_INSTALLED:
                return ArCoreAvailability.SUPPORTED_INSTALLED;
            case SUPPORTED_NOT_INSTALLED:
                return ArCoreAvailability.SUPPORTED_NOT_INSTALLED;
            case UNKNOWN_CHECKING:
                return ArCoreAvailability.UNKNOWN_CHECKING;
            case UNKNOWN_ERROR:
                return ArCoreAvailability.UNKNOWN_ERROR;
            case UNKNOWN_TIMED_OUT:
                return ArCoreAvailability.UNKNOWN_TIMED_OUT;
            case UNSUPPORTED_DEVICE_NOT_CAPABLE:
                return ArCoreAvailability.UNSUPPORTED_DEVICE_NOT_CAPABLE;
            default:
                throw new RuntimeException(
                        String.format("Unknown value of Availability: %s", availability));
        }
    }
}
