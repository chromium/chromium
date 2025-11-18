// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Barrier class is responsible for waiting the completion of Fido2 and/or Android Credential
 * Manager APIs. It supports either single mode (just one of the calls) or parallel mode (both
 * calls).
 *
 * In parallel mode, Fido2 API call results are privileged. For example, if Fido2 API call fails
 * with error code 1 and Android Credential Manager fails with error code 2, the resulting error
 * code is 1.
 */
@NullMarked
public class Barrier {
    @IntDef({Status.NONE, Status.WAITING, Status.SUCCESS, Status.FAILURE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Status {
        int NONE = 0;
        int WAITING = 1;
        int SUCCESS = 2;
        int FAILURE = 3;
    }

    @IntDef({Mode.ONLY_FIDO_2_API, Mode.ONLY_CRED_MAN, Mode.BOTH})
    public @interface Mode {
        // Barrier will complete once the Fido2Api calls are complete.
        int ONLY_FIDO_2_API = 0;
        // Barrier will complete once the CredMan calls are complete.
        int ONLY_CRED_MAN = 1;
        // Barrier will complete when both calls are complete.
        int BOTH = 2;
    }

    @IntDef({
        CallbacksToRun.RUN_FIDO_2_CALLBACK,
        CallbacksToRun.RUN_CRED_MAN_CALLBACK,
        CallbacksToRun.RUN_BOTH
    })
    private @interface CallbacksToRun {
        int RUN_FIDO_2_CALLBACK = 0;
        int RUN_CRED_MAN_CALLBACK = 1;
        int RUN_BOTH = 2;
    }

    private final Callback<Integer> mErrorCallback;
    @Nullable private Runnable mFido2ApiRunnable;
    @Nullable private Runnable mCredManRunnable;
    private @Status int mFido2ApiStatus;
    private @Status int mCredManStatus;
    private int mFido2ApiError;
    private boolean mFido2ApiCancelled;
    private boolean mCredManCancelled;
    private boolean mIsImmediateIncognito;

    public Barrier(Callback<Integer> errorCallback) {
        mErrorCallback = errorCallback;
        mFido2ApiStatus = Status.NONE;
        mCredManStatus = Status.NONE;
    }

    public void resetAndSetWaitStatus(@Mode int mode) {
        reset();
        switch (mode) {
            case Mode.ONLY_FIDO_2_API:
                mFido2ApiStatus = Status.WAITING;
                break;
            case Mode.ONLY_CRED_MAN:
                mCredManStatus = Status.WAITING;
                break;
            case Mode.BOTH:
                mFido2ApiStatus = Status.WAITING;
                mCredManStatus = Status.WAITING;
                break;
            default:
                assert false : "Unhandled Barrier mode: " + mode;
        }
    }

    public void onCredManSuccessful(Runnable onBarrierComplete) {
        mCredManRunnable = onBarrierComplete;
        switch (mFido2ApiStatus) {
            case Status.SUCCESS:
                maybeRunSuccessCallbacks(CallbacksToRun.RUN_BOTH);
                break;
            case Status.WAITING:
                mCredManStatus = Status.SUCCESS;
                break;
            case Status.NONE:
            case Status.FAILURE:
                maybeRunSuccessCallbacks(CallbacksToRun.RUN_CRED_MAN_CALLBACK);
                break;
        }
    }

    public void onCredManFailed(int error) {
        switch (mFido2ApiStatus) {
            case Status.SUCCESS:
                maybeRunSuccessCallbacks(CallbacksToRun.RUN_FIDO_2_CALLBACK);
                break;
            case Status.WAITING:
                mCredManStatus = Status.FAILURE;
                break;
            case Status.NONE:
                mErrorCallback.onResult(error);
                break;
            case Status.FAILURE:
                mErrorCallback.onResult(mFido2ApiError);
                break;
        }
    }

    public void onFido2ApiSuccessful(Runnable onBarrierComplete) {
        mFido2ApiRunnable = onBarrierComplete;
        switch (mCredManStatus) {
            case Status.SUCCESS:
                maybeRunSuccessCallbacks(CallbacksToRun.RUN_BOTH);
                break;
            case Status.WAITING:
                mFido2ApiStatus = Status.SUCCESS;
                break;
            case Status.NONE:
            case Status.FAILURE:
                maybeRunSuccessCallbacks(CallbacksToRun.RUN_FIDO_2_CALLBACK);
                break;
        }
    }

    public void onFido2ApiFailed(int error) {
        switch (mCredManStatus) {
            case Status.SUCCESS:
                maybeRunSuccessCallbacks(CallbacksToRun.RUN_CRED_MAN_CALLBACK);
                break;
            case Status.WAITING:
                mFido2ApiError = error;
                mFido2ApiStatus = Status.FAILURE;
                break;
            case Status.NONE:
            case Status.FAILURE:
                mErrorCallback.onResult(error);
                break;
        }
    }

    public void onCredManCancelled(int error) {
        if (mFido2ApiStatus == Status.NONE || mFido2ApiCancelled) {
            mErrorCallback.onResult(error);
            mFido2ApiCancelled = false;
            return;
        }
        mCredManCancelled = true;
    }

    public void onFido2ApiCancelled() {
        onFido2ApiCancelled(AuthenticatorStatus.ABORT_ERROR);
    }

    /**
     * The same as the method above but allows a custom @AuthenticatorStatus rather than the default
     * ABORT_ERROR.
     */
    public void onFido2ApiCancelled(int error) {
        if (mCredManStatus == Status.NONE || mCredManCancelled) {
            mErrorCallback.onResult(error);
            mCredManCancelled = false;
            return;
        }
        mFido2ApiCancelled = true;
    }

    /**
     * Called to indicate that the current request is an Immediate get that is in incognito mode. In
     * this case, the Barrier will always trigger a cancellation with `NOT_ALLOWED_ERROR`, but only
     * after it has finished querying available credentials.
     *
     * <p>This makes the incognito behaviour indistinguishable to the behaviour when no credentials
     * are available, even if the Relying Party is monitoring the call duration.
     */
    public void setImmediateIncognito() {
        mIsImmediateIncognito = true;
    }

    private void maybeRunSuccessCallbacks(@CallbacksToRun int callbacks) {
        if (mIsImmediateIncognito) {
            mErrorCallback.onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
            return;
        }

        if (callbacks != CallbacksToRun.RUN_CRED_MAN_CALLBACK) {
            assumeNonNull(mFido2ApiRunnable).run();
        }

        if (callbacks != CallbacksToRun.RUN_FIDO_2_CALLBACK) {
            assumeNonNull(mCredManRunnable).run();
        }
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
