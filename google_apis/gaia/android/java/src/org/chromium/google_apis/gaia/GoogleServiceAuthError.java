// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.google_apis.gaia;

import static org.chromium.build.NullUtil.assertNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This class mirrors the native GoogleServiceAuthError class from:
 * google_apis/gaia/google_service_auth_error.h.
 */
@NullMarked
public class GoogleServiceAuthError {
    private final @GoogleServiceAuthErrorState int mState;
    private final @Nullable DeviceManagementErrorDetails mDeviceManagementErrorDetails;

    /**
     * Creates an instance from the error `state`. Only supports the following error states:
     *
     * <ul>
     *   <li>{@link GoogleServiceAuthErrorState.NONE}
     *   <li>{@link GoogleServiceAuthErrorState.SCOPE_LIMITED_UNRECOVERABLE_ERROR}
     *   <li>{@link GoogleServiceAuthErrorState.INVALID_GAIA_CREDENTIALS}
     *   <li>{@link GoogleServiceAuthErrorState.CONNECTION_FAILED}
     *   <li>{@link GoogleServiceAuthErrorState.REQUEST_CANCELED}
     *   <li>{@link GoogleServiceAuthErrorState.ACCOUNT_NOT_FOUND}
     * </ul>
     *
     * <p>For {@link GoogleServiceAuthErrorState.DEVICE_MANAGEMENT_ERROR}, use {@link
     * #fromDeviceManagementError(DeviceManagementErrorDetails)}.
     */
    @CalledByNative
    public GoogleServiceAuthError(@GoogleServiceAuthErrorState int state) {
        mState = state;
        assert mState != GoogleServiceAuthErrorState.DEVICE_MANAGEMENT_ERROR;
        mDeviceManagementErrorDetails = null;
    }

    private GoogleServiceAuthError(DeviceManagementErrorDetails details) {
        mState = GoogleServiceAuthErrorState.DEVICE_MANAGEMENT_ERROR;
        mDeviceManagementErrorDetails = details;
    }

    /** Creates an instance for {@link GoogleServiceAuthErrorState.DEVICE_MANAGEMENT_ERROR}. */
    @CalledByNative
    public static GoogleServiceAuthError fromDeviceManagementError(
            DeviceManagementErrorDetails details) {
        return new GoogleServiceAuthError(details);
    }

    @CalledByNative
    public @GoogleServiceAuthErrorState int getState() {
        return mState;
    }

    /** Returns the details for {@link GoogleServiceAuthErrorState.DEVICE_MANAGEMENT_ERROR}. */
    @CalledByNative
    public DeviceManagementErrorDetails getDeviceManagementErrorDetails() {
        assert mState == GoogleServiceAuthErrorState.DEVICE_MANAGEMENT_ERROR;
        return assertNonNull(mDeviceManagementErrorDetails);
    }

    public boolean isTransientError() {
        return GoogleServiceAuthErrorJni.get().isTransientError(mState);
    }

    @NativeMethods
    interface Natives {
        boolean isTransientError(@GoogleServiceAuthErrorState int state);
    }
}
