// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.Authenticator.GetCredential_Response;
import org.chromium.blink.mojom.Authenticator.MakeCredential_Response;
import org.chromium.blink.mojom.Authenticator.Report_Response;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
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
                        responseData,
                        new RequestMetrics.Builder()
                                .setMakeCredentialOutcome(MakeCredentialOutcome.SUCCESS)
                                .build());
        callback.onComplete(response);

        verify(mMakeCredentialCallback).call(AuthenticatorStatus.SUCCESS, responseData, null);
        ArgumentCaptor<RequestMetrics> captor = ArgumentCaptor.forClass(RequestMetrics.class);
        verify(mRecordOutcomeCallback).record(captor.capture());
        RequestMetrics metrics = captor.getValue();
        assertEquals((Integer) MakeCredentialOutcome.SUCCESS, metrics.getMakeCredentialOutcome());
        assertNull(metrics.getGetAssertionOutcome());
        assertNull(metrics.getGetAssertionResult());

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
                        new RequestMetrics.Builder()
                                .setMakeCredentialOutcome(MakeCredentialOutcome.USER_CANCELLATION)
                                .build());
        callback.onComplete(response);

        verify(mMakeCredentialCallback).call(AuthenticatorStatus.NOT_ALLOWED_ERROR, null, null);
        ArgumentCaptor<RequestMetrics> captor = ArgumentCaptor.forClass(RequestMetrics.class);
        verify(mRecordOutcomeCallback).record(captor.capture());
        RequestMetrics metrics = captor.getValue();
        assertEquals(
                (Integer) MakeCredentialOutcome.USER_CANCELLATION,
                metrics.getMakeCredentialOutcome());
        assertNull(metrics.getGetAssertionOutcome());
        assertNull(metrics.getGetAssertionResult());

        verify(mCompletionCallback).run();
    }

    @Test
    @SmallTest
    public void testGetCredential_success() {
        WebauthnRequestCallback callback =
                WebauthnRequestCallback.forGetCredential(
                        mGetCredentialCallback, mRecordOutcomeCallback);
        callback.setCompletionCallback(mCompletionCallback);

        WebauthnRequestResponse response =
                WebauthnRequestResponse.forSuccessfulGetAssertion(
                        new GetAssertionAuthenticatorResponse(),
                        new RequestMetrics.Builder()
                                .setGetAssertionOutcome(GetAssertionOutcome.SUCCESS)
                                .build());
        callback.onComplete(response);

        verify(mGetCredentialCallback).call(response.getGetCredentialResponse());
        ArgumentCaptor<RequestMetrics> captor = ArgumentCaptor.forClass(RequestMetrics.class);
        verify(mRecordOutcomeCallback).record(captor.capture());
        RequestMetrics metrics = captor.getValue();
        assertEquals((Integer) GetAssertionOutcome.SUCCESS, metrics.getGetAssertionOutcome());
        assertNull(metrics.getMakeCredentialOutcome());
        assertNull(metrics.getGetAssertionResult());

        verify(mCompletionCallback).run();
    }

    @Test
    @SmallTest
    public void testGetCredential_failure() {
        WebauthnRequestCallback callback =
                WebauthnRequestCallback.forGetCredential(
                        mGetCredentialCallback, mRecordOutcomeCallback);
        callback.setCompletionCallback(mCompletionCallback);

        RequestMetrics requestMetrics =
                new RequestMetrics.Builder()
                        .setGetAssertionOutcome(GetAssertionOutcome.USER_CANCELLATION)
                        .build();
        WebauthnRequestResponse response =
                WebauthnRequestResponse.forFailedGetCredential(
                        AuthenticatorStatus.NOT_ALLOWED_ERROR, requestMetrics);
        callback.onComplete(response);

        verify(mGetCredentialCallback).call(response.getGetCredentialResponse());
        ArgumentCaptor<RequestMetrics> captor = ArgumentCaptor.forClass(RequestMetrics.class);
        verify(mRecordOutcomeCallback).record(captor.capture());
        RequestMetrics metrics = captor.getValue();
        assertEquals(
                (Integer) GetAssertionOutcome.USER_CANCELLATION, metrics.getGetAssertionOutcome());
        assertNull(metrics.getMakeCredentialOutcome());
        assertNull(metrics.getGetAssertionResult());

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
        verify(mRecordOutcomeCallback, never()).record(any(RequestMetrics.class));
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
                        responseData,
                        new RequestMetrics.Builder()
                                .setMakeCredentialOutcome(MakeCredentialOutcome.SUCCESS)
                                .build());
        callback.onComplete(response);
        // Second call should be ignored.
        callback.onComplete(response);

        verify(mMakeCredentialCallback, times(1))
                .call(AuthenticatorStatus.SUCCESS, responseData, null);
        verify(mRecordOutcomeCallback, times(1)).record(any(RequestMetrics.class));
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
                WebauthnRequestResponse.forSuccessfulGetCredential(
                        getCredentialResponseData,
                        new RequestMetrics.Builder()
                                .setGetAssertionOutcome(GetAssertionOutcome.SUCCESS)
                                .build());

        // The callback is for make credential, so it should ignore a get credential response.
        callback.onComplete(response);

        verify(mMakeCredentialCallback, never()).call(anyInt(), any(), any());
        verify(mRecordOutcomeCallback, never()).record(any(RequestMetrics.class));
        verify(mCompletionCallback).run();
    }
}
