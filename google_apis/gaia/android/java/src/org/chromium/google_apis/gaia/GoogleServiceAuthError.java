// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.google_apis.gaia;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * This class mirrors the native GoogleServiceAuthError class from:
 * google_apis/gaia/google_service_auth_error.h.
 */
@NullMarked
public class GoogleServiceAuthError {
    private final @GoogleServiceAuthErrorState int mState;

    @CalledByNative
    public GoogleServiceAuthError(@GoogleServiceAuthErrorState int state) {
        mState = state;
    }

    @CalledByNative
    public @GoogleServiceAuthErrorState int getState() {
        return mState;
    }

    public boolean isTransientError() {
        return GoogleServiceAuthErrorJni.get().isTransientError(mState);
    }

    @NativeMethods
    interface Natives {
        boolean isTransientError(@GoogleServiceAuthErrorState int state);
    }
}
