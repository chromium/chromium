// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.AuthenticatorStatus;

/**
 * Barrier class is responsible for waiting the completion of Fido2 and/or Android Credential
 * Manager APIs. It supports either single mode (just one of the calls) or parallel mode (both
 * calls).
 *
 * In parallel mode, Fido2 API call results are privileged. For example, if Fido2 API call fails
 * with error code 1 and Android Credential Manager fails with error code 2, the resulting error
 * code is 1.
 */
public class Barrier {
    private enum Status {
        NONE,
        WAITING,
        SUCCESS,
        FAILURE,
    }

    public enum Mode {
        // Barrier will complete once the Fido2Api calls are complete.
        ONLY_FIDO_2_API,
        // Barrier will complete once the CredMan calls are complete.
        ONLY_CRED_MAN,
        // Barrier will complete when both calls are complete.
        BOTH,
    }

    private Callback<Integer> mErrorCallback;
    private Runnable mFido2ApiRunnable;
    private Runnable mCredManRunnable;
    private Status mFido2ApiStatus;
    private Status mCredManStatus;
    private int mFido2ApiError;
    private boolean mFido2ApiCancelled;
    private boolean mCredManCancelled;

    public Barrier(Callback<Integer> errorCallback) {
        mErrorCallback = errorCallback;
        mFido2ApiStatus = Status.NONE;
        mCredManStatus = Status.NONE;
    }

    public void resetAndSetWaitStatus(Mode mode) {
        reset();
        switch (mode) {
            case ONLY_FIDO_2_API:
                mFido2ApiStatus = Status.WAITING;
                break;
            case ONLY_CRED_MAN:
                mCredManStatus = Status.WAITING;
                break;
            case BOTH:
                mFido2ApiStatus = Status.WAITING;
                mCredManStatus = Status.WAITING;
                break;
            default:
                assert false : "Unhandled Barrier mode: " + mode;
        }
    }

    public void onCredManSuccessful(Runnable onBarrierComplete) {
        if (mFido2ApiStatus == Status.FAILURE) {
            onBarrierComplete.run();
        } else if (mFido2ApiStatus == Status.SUCCESS) {
            onBarrierComplete.run();
            mFido2ApiRunnable.run();
        } else if (mFido2ApiStatus == Status.WAITING) {
            mCredManRunnable = onBarrierComplete;
            mCredManStatus = Status.SUCCESS;
        } else {
            onBarrierComplete.run();
        }
    }

    public void onCredManFailed(int error) {
        if (mFido2ApiStatus == Status.FAILURE) {
            mErrorCallback.onResult(mFido2ApiError);
        } else if (mFido2ApiStatus == Status.SUCCESS) {
            mFido2ApiRunnable.run();
        } else if (mFido2ApiStatus == Status.WAITING) {
            mCredManStatus = Status.FAILURE;
        } else {
            mErrorCallback.onResult(error);
        }
    }

    public void onFido2ApiSuccessful(Runnable onBarrierComplete) {
        if (mCredManStatus == Status.FAILURE) {
            onBarrierComplete.run();
        } else if (mCredManStatus == Status.SUCCESS) {
            mCredManRunnable.run();
            onBarrierComplete.run();
        } else if (mCredManStatus == Status.WAITING) {
            mFido2ApiRunnable = onBarrierComplete;
            mFido2ApiStatus = Status.SUCCESS;
        } else {
            onBarrierComplete.run();
        }
    }

    public void onFido2ApiFailed(int error) {
        if (mCredManStatus == Status.FAILURE) {
            mErrorCallback.onResult(error);
        } else if (mCredManStatus == Status.SUCCESS) {
            mCredManRunnable.run();
        } else if (mCredManStatus == Status.WAITING) {
            mFido2ApiError = error;
            mFido2ApiStatus = Status.FAILURE;
        } else {
            mErrorCallback.onResult(error);
        }
    }

    public void onCredManCancelled() {
        if (mFido2ApiStatus == Status.NONE || mFido2ApiCancelled) {
            mErrorCallback.onResult(AuthenticatorStatus.ABORT_ERROR);
            mFido2ApiCancelled = false;
            return;
        }
        mCredManCancelled = true;
    }

    public void onFido2ApiCancelled() {
        if (mCredManStatus == Status.NONE || mCredManCancelled) {
            mErrorCallback.onResult(AuthenticatorStatus.ABORT_ERROR);
            mCredManCancelled = false;
            return;
        }
        mFido2ApiCancelled = true;
    }

    private void reset() {
        mFido2ApiRunnable = null;
        mCredManRunnable = null;
        mFido2ApiStatus = Status.NONE;
        mCredManStatus = Status.NONE;
        mCredManCancelled = false;
        mFido2ApiCancelled = false;
    }
}
