// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.Authenticator.GetCredential_Response;
import org.chromium.blink.mojom.Authenticator.MakeCredential_Response;
import org.chromium.blink.mojom.Authenticator.Report_Response;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetCredentialResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;

/** Robolectric tests for {@link WebauthnRequestCallback}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebauthnRequestCallbackRobolectricTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MakeCredential_Response mMakeCredentialCallback;
    @Mock private GetCredential_Response mGetCredentialCallback;
    @Mock private Report_Response mReportCallback;
    @Mock private RecordOutcomeCallback mRecordOutcomeCallback;
    @Mock private Runnable mCompletionCallback;

    @Test
    @SmallTest
    public void testMakeCredential_success() {
        WebauthnRequestCallback callback =
                WebauthnRequestCallback.forMakeCredential(
                        mMakeCredentialCallback, mRecordOutcomeCallback);
        callback.setCompletionCallback(mCompletionCallback);

        MakeCredentialAuthenticatorResponse responseData =
                new MakeCredentialAuthenticatorResponse();
        WebauthnRequestResponse response =
                WebauthnRequestResponse.forSuccessfulMakeCredential(
                        responseData, MakeCredentialOutcome.SUCCESS);
        callback.onComplete(response);

        verify(mMakeCredentialCallback).call(AuthenticatorStatus.SUCCESS, responseData, null);
        verify(mRecordOutcomeCallback).record(MakeCredentialOutcome.SUCCESS);
        verify(mCompletionCallback).run();
    }

    @Test
    @SmallTest
    public void testMakeCredential_failure() {
        WebauthnRequestCallback callback =
                WebauthnRequestCallback.forMakeCredential(
                        mMakeCredentialCallback, mRecordOutcomeCallback);
        callback.setCompletionCallback(mCompletionCallback);

        WebauthnRequestResponse response =
                WebauthnRequestResponse.forFailedMakeCredential(
                        AuthenticatorStatus.NOT_ALLOWED_ERROR,
                        MakeCredentialOutcome.USER_CANCELLATION);
        callback.onComplete(response);

        verify(mMakeCredentialCallback).call(AuthenticatorStatus.NOT_ALLOWED_ERROR, null, null);
        verify(mRecordOutcomeCallback).record(MakeCredentialOutcome.USER_CANCELLATION);
        verify(mCompletionCallback).run();
    }

    @Test
    @SmallTest
    public void testGetCredential_success() {
        WebauthnRequestCallback callback =
                WebauthnRequestCallback.forGetCredential(
                        mGetCredentialCallback, mRecordOutcomeCallback);
        callback.setCompletionCallback(mCompletionCallback);

        GetCredentialResponse responseData = new GetCredentialResponse();
        WebauthnRequestResponse response =
                WebauthnRequestResponse.forSuccessfulGetCredential(responseData);
        callback.onComplete(response);

        verify(mGetCredentialCallback).call(responseData);
        verify(mRecordOutcomeCallback).record(GetAssertionOutcome.SUCCESS);
        verify(mCompletionCallback).run();
    }

    @Test
    @SmallTest
    public void testGetCredential_failure() {
        WebauthnRequestCallback callback =
                WebauthnRequestCallback.forGetCredential(
                        mGetCredentialCallback, mRecordOutcomeCallback);
        callback.setCompletionCallback(mCompletionCallback);

        WebauthnRequestResponse response =
                WebauthnRequestResponse.forFailedGetCredential(
                        AuthenticatorStatus.NOT_ALLOWED_ERROR,
                        GetAssertionOutcome.USER_CANCELLATION);
        callback.onComplete(response);

        verify(mGetCredentialCallback).call(response.getGetCredentialResponse());
        verify(mRecordOutcomeCallback).record(GetAssertionOutcome.USER_CANCELLATION);
        verify(mCompletionCallback).run();
    }

    @Test
    @SmallTest
    public void testReport() {
        WebauthnRequestCallback callback = WebauthnRequestCallback.forReport(mReportCallback);
        callback.setCompletionCallback(mCompletionCallback);

        WebauthnRequestResponse response =
                WebauthnRequestResponse.forReport(AuthenticatorStatus.SUCCESS);
        callback.onComplete(response);

        verify(mReportCallback).call(AuthenticatorStatus.SUCCESS, null);
        verify(mRecordOutcomeCallback, never()).record(0);
        verify(mCompletionCallback).run();
    }

    @Test
    @SmallTest
    public void testOnComplete_multipleCallsIgnored() {
        WebauthnRequestCallback callback =
                WebauthnRequestCallback.forMakeCredential(
                        mMakeCredentialCallback, mRecordOutcomeCallback);
        callback.setCompletionCallback(mCompletionCallback);

        MakeCredentialAuthenticatorResponse responseData =
                new MakeCredentialAuthenticatorResponse();
        WebauthnRequestResponse response =
                WebauthnRequestResponse.forSuccessfulMakeCredential(
                        responseData, MakeCredentialOutcome.SUCCESS);
        callback.onComplete(response);
        // Second call should be ignored.
        callback.onComplete(response);

        verify(mMakeCredentialCallback, times(1))
                .call(AuthenticatorStatus.SUCCESS, responseData, null);
        verify(mRecordOutcomeCallback, times(1)).record(MakeCredentialOutcome.SUCCESS);
        verify(mCompletionCallback, times(1)).run();
    }

    @Test
    @SmallTest
    public void testOnComplete_mismatchedResponseIsIgnored() {
        WebauthnRequestCallback callback =
                WebauthnRequestCallback.forMakeCredential(
                        mMakeCredentialCallback, mRecordOutcomeCallback);
        callback.setCompletionCallback(mCompletionCallback);

        // This is a get credential response.
        GetCredentialResponse getCredentialResponseData = new GetCredentialResponse();
        WebauthnRequestResponse response =
                WebauthnRequestResponse.forSuccessfulGetCredential(getCredentialResponseData);

        // The callback is for make credential, so it should ignore a get credential response.
        callback.onComplete(response);

        verify(mMakeCredentialCallback, never()).call(anyInt(), any(), any());
        verify(mRecordOutcomeCallback, never()).record(anyInt());
        verify(mCompletionCallback).run();
    }
}
