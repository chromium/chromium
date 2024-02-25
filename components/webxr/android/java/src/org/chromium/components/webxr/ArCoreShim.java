// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface used to wrap around ArCore SDK Java interface.
 *
 * For detailed documentation of the below methods, please see:
 * https://developers.google.com/ar/reference/java/arcore/reference/com/google/ar/core/ArCoreApk
 */
public interface ArCoreShim {
    /**
     * Equivalent of ArCoreApk.ArInstallStatus enum.
     *
     * For detailed description, please see:
     * https://developers.google.com/ar/reference/java/arcore/reference/com/google/ar/core/ArCoreApk.InstallStatus
     */
    @IntDef({InstallStatus.INSTALLED, InstallStatus.INSTALL_REQUESTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface InstallStatus {
        int INSTALLED = 0;
        int INSTALL_REQUESTED = 1;
    }

    /** Equivalent of ArCoreApk.checkAvailability. */
    public @ArCoreAvailability int checkAvailability(Context applicationContext);

    /** Equivalent of ArCoreApk.requestInstall. */
    public @InstallStatus int requestInstall(Activity activity, boolean userRequestedInstall)
            throws UnavailableDeviceNotCompatibleException,
                    UnavailableUserDeclinedInstallationException;

    /** Thrown by requestInstall() when device is not compatible with ARCore. */
    public class UnavailableDeviceNotCompatibleException extends Exception {
        public UnavailableDeviceNotCompatibleException(Exception cause) {
            super(cause);
        }
    }

    /** Thrown by requestInstall() when user declined to install ARCore. */
    public class UnavailableUserDeclinedInstallationException extends Exception {
        UnavailableUserDeclinedInstallationException(Exception cause) {
            super(cause);
        }
    }
}
