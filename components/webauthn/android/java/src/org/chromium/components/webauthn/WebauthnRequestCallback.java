// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.webauthn.WebauthnLogger.log;
import static org.chromium.components.webauthn.WebauthnLogger.logError;

import androidx.annotation.IntDef;

import org.chromium.blink.mojom.Authenticator.GetCredential_Response;
import org.chromium.blink.mojom.Authenticator.MakeCredential_Response;
import org.chromium.blink.mojom.Authenticator.Report_Response;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetCredentialResponse;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Wrapper to manage the set of callbacks for a WebAuthn operation, enforcing that only one of
 * GetCredential, MakeCredential, or Report callbacks can be active.
 */
@NullMarked
public class WebauthnRequestCallback {
    private static final String TAG = "WebauthnRequestCallback";

    @IntDef({
        CallbackType.GET_CREDENTIAL,
        CallbackType.MAKE_CREDENTIAL,
        CallbackType.REPORT,
    })
    public @interface CallbackType {
        int GET_CREDENTIAL = 0;
        int MAKE_CREDENTIAL = 1;
        int REPORT = 2;
    }

    private final @CallbackType int mCallbackType;
    private @Nullable Runnable mCompletionCallback;

    // Only one of the following callbacks should be non-null at any given time.
    private @Nullable GetCredential_Response mGetCredentialCallback;
    private @Nullable MakeCredential_Response mMakeCredentialCallback;
    private @Nullable Report_Response mReportCallback;

    private @Nullable RecordOutcomeCallback mRecordingCallback;

    // Private constructor to enforce creation via static factory methods.
    private WebauthnRequestCallback(
            @CallbackType int callbackType,
            @Nullable GetCredential_Response getCredentialCallback,
            @Nullable MakeCredential_Response makeCredentialCallback,
            @Nullable Report_Response reportCallback,
            @Nullable RecordOutcomeCallback recordingCallback) {
        mCallbackType = callbackType;
        if (callbackType == CallbackType.GET_CREDENTIAL) {
            assert getCredentialCallback != null;
            assert makeCredentialCallback == null;
            assert reportCallback == null;
        } else if (callbackType == CallbackType.MAKE_CREDENTIAL) {
            assert getCredentialCallback == null;
            assert makeCredentialCallback != null;
            assert reportCallback == null;
        } else if (callbackType == CallbackType.REPORT) {
            assert getCredentialCallback == null;
            assert makeCredentialCallback == null;
            assert reportCallback != null;
        } else {
            assert false;
        }
        mGetCredentialCallback = getCredentialCallback;
        mMakeCredentialCallback = makeCredentialCallback;
        mReportCallback = reportCallback;
        mRecordingCallback = recordingCallback;
    }

    // Static factory for GetCredential
    public static WebauthnRequestCallback forGetCredential(
            GetCredential_Response getCredentialCallback,
            @Nullable RecordOutcomeCallback recordingCallback) {
        log(TAG, "forGetCredential");
        return new WebauthnRequestCallback(
                CallbackType.GET_CREDENTIAL, getCredentialCallback, null, null, recordingCallback);
    }

    // Static factory for MakeCredential
    public static WebauthnRequestCallback forMakeCredential(
            MakeCredential_Response makeCredentialCallback,
            @Nullable RecordOutcomeCallback recordingCallback) {
        log(TAG, "forMakeCredential");
        return new WebauthnRequestCallback(
                CallbackType.MAKE_CREDENTIAL,
                null,
                makeCredentialCallback,
                null,
                recordingCallback);
    }

    // Static factory for Report
    public static WebauthnRequestCallback forReport(Report_Response reportCallback) {
        log(TAG, "forReport");
        return new WebauthnRequestCallback(CallbackType.REPORT, null, null, reportCallback, null);
    }

    public @CallbackType int getCallbackType() {
        return mCallbackType;
    }

    public void onComplete(WebauthnRequestResponse response) {
        log(TAG, "onComplete, callbackType: %d", mCallbackType);
        if (isRequestComplete()) {
            logError(TAG, "No callbacks to handle response.");
            return;
        }
        if (response.getRequestMetrics() != null) {
            recordOutcome(response.getRequestMetrics());
        }

        switch (mCallbackType) {
            case CallbackType.GET_CREDENTIAL:
                assert mGetCredentialCallback != null;
                if (response.getGetCredentialResponse() != null) {
                    handleGetCredentialResponse(response);
                }
                break;
            case CallbackType.MAKE_CREDENTIAL:
                assert mMakeCredentialCallback != null;
                // A successful getCredential response has a null makeCredentialCallback and a
                // status of SUCCESS. So this check prevents a mismatched response from being
                // processed.
                if (response.getMakeCredentialResponse() != null
                        || response.getAuthenticatorStatus() != AuthenticatorStatus.SUCCESS) {
                    handleMakeCredentialResponse(response);
                }
                break;
            case CallbackType.REPORT:
                assert mReportCallback != null;
                mReportCallback.call(response.getAuthenticatorStatus(), null);
                break;
            default:
                assert false;
        }
        clearCallbacks();
        if (mCompletionCallback != null) {
            mCompletionCallback.run();
            mCompletionCallback = null;
        }
    }

    public void setCompletionCallback(Runnable completionCallback) {
        mCompletionCallback = completionCallback;
    }

    private void recordOutcome(RequestMetrics result) {
        if ((mCallbackType == CallbackType.MAKE_CREDENTIAL
                        && result.getMakeCredentialOutcome() == null)
                || (mCallbackType == CallbackType.GET_CREDENTIAL
                        && result.getGetAssertionOutcome() == null)) {
            return;
        }
        if (result.getMakeCredentialOutcome() != null) {
            log(TAG, "recordOutcome: makeCredentialOutcome=%d", result.getMakeCredentialOutcome());
        } else if (result.getGetAssertionOutcome() != null) {
            log(TAG, "recordOutcome: getAssertionOutcome=%d", result.getGetAssertionOutcome());
        }
        if (mRecordingCallback != null) {
            mRecordingCallback.record(result);
        }
    }

    private void handleGetCredentialResponse(WebauthnRequestResponse response) {
        assumeNonNull(mGetCredentialCallback);
        GetCredentialResponse getCredentialResponse = response.getGetCredentialResponse();
        assumeNonNull(getCredentialResponse);
        if (getCredentialResponse.which() == GetCredentialResponse.Tag.GetAssertionResponse) {
            log(
                    TAG,
                    "handleGetCredentialResponse: status=%d",
                    getCredentialResponse.getGetAssertionResponse().status);
        } else {
            log(TAG, "handleGetCredentialResponse: called with password credential");
        }
        mGetCredentialCallback.call(response.getGetCredentialResponse());
    }

    private void handleMakeCredentialResponse(WebauthnRequestResponse response) {
        log(TAG, "handleMakeCredentialResponse: status=%d", response.getAuthenticatorStatus());
        assumeNonNull(mMakeCredentialCallback);
        if (response.getAuthenticatorStatus() == AuthenticatorStatus.SUCCESS) {
            mMakeCredentialCallback.call(
                    AuthenticatorStatus.SUCCESS, response.getMakeCredentialResponse(), null);
        } else {
            mMakeCredentialCallback.call(response.getAuthenticatorStatus(), null, null);
        }
    }

    private boolean isRequestComplete() {
        return mGetCredentialCallback == null
                && mMakeCredentialCallback == null
                && mReportCallback == null;
    }

    // Clears all response callbacks to prevent multiple invocations.
    private void clearCallbacks() {
        mGetCredentialCallback = null;
        mMakeCredentialCallback = null;
        mReportCallback = null;
        mRecordingCallback = null;
    }
}
