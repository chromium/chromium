// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import androidx.annotation.IntDef;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.identitycredentials.CreateCredentialException;
import com.google.android.gms.tasks.Task;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Shared helpers for recording Started and single-emission terminal outcome histograms for Android
 * platform (CredMan) and GMSCore WebAuthn API calls.
 */
@NullMarked
public final class AndroidApiCallMetrics {
    // LINT.IfChange(ApiCallEvent)
    //
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({
        ApiCallEvent.STARTED,
        ApiCallEvent.SUCCEEDED,
        ApiCallEvent.CANCELLED,
        ApiCallEvent.TIMED_OUT,
        ApiCallEvent.FAILED,
        ApiCallEvent.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ApiCallEvent {
        int STARTED = 0;
        int SUCCEEDED = 1;
        int CANCELLED = 2;
        int TIMED_OUT = 3;
        int FAILED = 4;
        int NUM_ENTRIES = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:WebAuthenticationAndroidApiCallEvent)

    /**
     * Emits {@link ApiCallEvent#STARTED} upon creation and guarantees at most one terminal outcome
     * sample is recorded via {@link #finish}. It may be created on any thread (recording histograms
     * is thread-safe), but it is not otherwise thread-safe: {@link #finish} must always be called
     * on the same thread, which is the UI thread in production.
     */
    public static final class OneShotRecorder {
        private final String mHistogramName;
        private boolean mFinished;

        private OneShotRecorder(String histogramName) {
            mHistogramName = histogramName;
            RecordHistogram.recordEnumeratedHistogram(
                    mHistogramName, ApiCallEvent.STARTED, ApiCallEvent.NUM_ENTRIES);
        }

        public void finish(@ApiCallEvent int terminalEvent) {
            assert terminalEvent != ApiCallEvent.STARTED;
            if (mFinished) {
                return;
            }
            mFinished = true;
            RecordHistogram.recordEnumeratedHistogram(
                    mHistogramName, terminalEvent, ApiCallEvent.NUM_ENTRIES);
        }
    }

    /**
     * Variant of {@link OneShotRecorder} that can be created before an operation is dispatched (for
     * example, so that an outcome callback can capture it) but only emits {@link
     * ApiCallEvent#STARTED} once {@link #markStarted} is called at dispatch time. Pre-dispatch
     * failures therefore record nothing. If {@link #finish} is invoked before {@link #markStarted}
     * (such as a synchronous callback delivered before {@link FidoIntentSender#showIntent}
     * returns), {@link ApiCallEvent#STARTED} is emitted first.
     */
    public static final class DeferredOneShotRecorder {
        private final String mHistogramName;
        private @Nullable OneShotRecorder mRecorder;

        private DeferredOneShotRecorder(String histogramName) {
            mHistogramName = histogramName;
        }

        /** Emits {@link ApiCallEvent#STARTED} if not already started. */
        public void markStarted() {
            if (mRecorder == null) {
                mRecorder = startCall(mHistogramName);
            }
        }

        /**
         * Emits {@link ApiCallEvent#STARTED} and runs {@code apiCall}. If {@code apiCall} throws
         * synchronously, records {@link ApiCallEvent#FAILED} so that the call is not mistaken for a
         * hung one, then rethrows.
         */
        public void dispatch(Runnable apiCall) {
            markStarted();
            try {
                apiCall.run();
            } catch (RuntimeException e) {
                finish(ApiCallEvent.FAILED);
                throw e;
            }
        }

        /**
         * Ensures {@link ApiCallEvent#STARTED} was emitted and records {@code terminalEvent} once.
         */
        public void finish(@ApiCallEvent int terminalEvent) {
            markStarted();
            assert mRecorder != null;
            mRecorder.finish(terminalEvent);
        }
    }

    private AndroidApiCallMetrics() {}

    public static OneShotRecorder startCall(String histogramName) {
        return new OneShotRecorder(histogramName);
    }

    public static DeferredOneShotRecorder deferredCall(String histogramName) {
        return new DeferredOneShotRecorder(histogramName);
    }

    public static @ApiCallEvent int classifyTaskException(@Nullable Exception e) {
        if (e instanceof CreateCredentialException createException
                && CreateCredentialException.ERROR_TYPE_USER_CANCELED.equals(
                        createException.getType())) {
            return ApiCallEvent.CANCELLED;
        }
        if (e instanceof ApiException apiException) {
            switch (apiException.getStatusCode()) {
                case CommonStatusCodes.TIMEOUT:
                    return ApiCallEvent.TIMED_OUT;
                case CommonStatusCodes.CANCELED:
                    return ApiCallEvent.CANCELLED;
                default:
                    return ApiCallEvent.FAILED;
            }
        }
        return ApiCallEvent.FAILED;
    }

    public static <Result> Task<Result> attachToTask(Task<Result> task, OneShotRecorder recorder) {
        return attachToTask(task, recorder, /* nullResultIsFailure= */ false);
    }

    /**
     * Records the terminal outcome of {@code task} on {@code recorder}, on the UI thread. A
     * successful task with a null result is recorded as {@link ApiCallEvent#FAILED} if {@code
     * nullResultIsFailure}.
     */
    public static <Result> Task<Result> attachToTask(
            Task<Result> task, OneShotRecorder recorder, boolean nullResultIsFailure) {
        task.addOnCompleteListener(
                Runnable::run,
                GmsCoreUtils.wrapCompleteCallback(
                        completedTask -> {
                            if (completedTask.isCanceled()) {
                                recorder.finish(ApiCallEvent.CANCELLED);
                            } else if (completedTask.isSuccessful()) {
                                recorder.finish(
                                        nullResultIsFailure && completedTask.getResult() == null
                                                ? ApiCallEvent.FAILED
                                                : ApiCallEvent.SUCCEEDED);
                            } else {
                                recorder.finish(
                                        classifyTaskException(completedTask.getException()));
                            }
                        }));
        return task;
    }
}
