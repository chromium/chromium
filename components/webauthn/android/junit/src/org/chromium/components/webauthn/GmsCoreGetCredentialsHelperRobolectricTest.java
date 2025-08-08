// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.webauthn.GmsCoreGetCredentialsHelper.GmsCoreGetCredentialsResult;
import org.chromium.components.webauthn.GmsCoreGetCredentialsHelper.Reason;

import java.util.ArrayList;
import java.util.List;

/** Robolectric tests for {@link GmsCoreGetCredentialsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({WebauthnFeatures.WEBAUTHN_ANDROID_PASSKEY_CACHE_MIGRATION})
public class GmsCoreGetCredentialsHelperRobolectricTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String RP_ID = "rp.id";

    @Mock private Fido2ApiCallHelper mFido2ApiCallHelperMock;
    @Mock private AuthenticationContextProvider mAuthenticationContextProviderMock;
    @Mock private GmsCoreGetCredentialsHelper.GetCredentialsCallback mSuccessCallbackMock;
    @Mock private OnFailureListener mFailureCallbackMock;

    private GmsCoreGetCredentialsHelper mHelper;
    private List<WebauthnCredentialDetails> mCredentials;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Fido2ApiCallHelper.overrideInstanceForTesting(mFido2ApiCallHelperMock);
        mHelper = GmsCoreGetCredentialsHelper.getInstance();
        mCredentials = new ArrayList<>();
        mCredentials.add(new WebauthnCredentialDetails());
    }

    @After
    public void tearDown() {
        Fido2ApiCallHelper.overrideInstanceForTesting(null);
        GmsCoreGetCredentialsHelper.overrideInstanceForTesting(null);
    }

    @Test
    @DisableFeatures({WebauthnFeatures.WEBAUTHN_ANDROID_PASSKEY_CACHE_MIGRATION})
    public void testGetCredentials_featureDisabled_fido2ApiSuccess() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "WebAuthentication.Android.GmsCoreGetCredentialsResult",
                        GmsCoreGetCredentialsResult.FIDO2_SUCCESS);

        mHelper.getCredentials(
                mAuthenticationContextProviderMock,
                RP_ID,
                Reason.GET_ASSERTION_NON_GOOGLE,
                mSuccessCallbackMock,
                mFailureCallbackMock);

        @SuppressWarnings("unchecked")
        ArgumentCaptor<OnSuccessListener<List<WebauthnCredentialDetails>>> successCallbackCaptor =
                ArgumentCaptor.forClass(OnSuccessListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokeFido2GetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        successCallbackCaptor.capture(),
                        any());
        successCallbackCaptor.getValue().onSuccess(mCredentials);

        verify(mSuccessCallbackMock).onCredentialsReceived(mCredentials);
        histogramWatcher.assertExpected();
    }

    @Test
    @DisableFeatures({WebauthnFeatures.WEBAUTHN_ANDROID_PASSKEY_CACHE_MIGRATION})
    public void testGetCredentials_featureDisabled_fido2ApiFailure() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "WebAuthentication.Android.GmsCoreGetCredentialsResult",
                        GmsCoreGetCredentialsResult.FIDO2_FAILURE);
        Exception e = new Exception();

        mHelper.getCredentials(
                mAuthenticationContextProviderMock,
                RP_ID,
                Reason.GET_ASSERTION_NON_GOOGLE,
                mSuccessCallbackMock,
                mFailureCallbackMock);

        ArgumentCaptor<OnFailureListener> failureCallbackCaptor =
                ArgumentCaptor.forClass(OnFailureListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokeFido2GetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        any(),
                        failureCallbackCaptor.capture());
        failureCallbackCaptor.getValue().onFailure(e);

        verify(mFailureCallbackMock).onFailure(e);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testGetCredentials_featureEnabled_cacheSuccess() {
        GmsCoreUtils.setGmsCoreVersionForTesting(244400000);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "WebAuthentication.Android.GmsCoreGetCredentialsResult",
                                GmsCoreGetCredentialsResult.CACHE_SUCCESS)
                        .expectAnyRecord("WebAuthentication.CredentialFetchDuration.GmsCore")
                        .expectAnyRecord("WebAuthentication.CredentialFetchDuration.GmsCore.Cache")
                        .build();

        mHelper.getCredentials(
                mAuthenticationContextProviderMock,
                RP_ID,
                Reason.GET_ASSERTION_NON_GOOGLE,
                mSuccessCallbackMock,
                mFailureCallbackMock);

        ArgumentCaptor<OnSuccessListener<List<WebauthnCredentialDetails>>> successCallbackCaptor =
                ArgumentCaptor.forClass(OnSuccessListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokePasskeyCacheGetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        successCallbackCaptor.capture(),
                        any());
        successCallbackCaptor.getValue().onSuccess(mCredentials);

        verify(mSuccessCallbackMock).onCredentialsReceived(mCredentials);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testGetCredentials_featureEnabledGoogleDomain_invokesFido2() {
        GmsCoreUtils.setGmsCoreVersionForTesting(244400000);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "WebAuthentication.Android.GmsCoreGetCredentialsResult",
                                GmsCoreGetCredentialsResult.FIDO2_SUCCESS)
                        .expectAnyRecord("WebAuthentication.CredentialFetchDuration.GmsCore")
                        .expectAnyRecord("WebAuthentication.CredentialFetchDuration.GmsCore.Fido2")
                        .build();

        mHelper.getCredentials(
                mAuthenticationContextProviderMock,
                "google.com",
                Reason.GET_ASSERTION_GOOGLE_RP,
                mSuccessCallbackMock,
                mFailureCallbackMock);

        ArgumentCaptor<OnSuccessListener<List<WebauthnCredentialDetails>>> successCallbackCaptor =
                ArgumentCaptor.forClass(OnSuccessListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokeFido2GetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq("google.com"),
                        successCallbackCaptor.capture(),
                        any());
        verify(mFido2ApiCallHelperMock, never())
                .invokePasskeyCacheGetCredentials(any(), any(), any(), any());

        successCallbackCaptor.getValue().onSuccess(mCredentials);

        verify(mSuccessCallbackMock).onCredentialsReceived(mCredentials);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testGetCredentials_paymentRequest_invokesFido2() {
        GmsCoreUtils.setGmsCoreVersionForTesting(244400000);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "WebAuthentication.Android.GmsCoreGetCredentialsResult",
                                GmsCoreGetCredentialsResult.FIDO2_SUCCESS)
                        .expectNoRecords("WebAuthentication.CredentialFetchDuration.GmsCore")
                        .build();

        mHelper.getCredentials(
                mAuthenticationContextProviderMock,
                RP_ID,
                Reason.PAYMENT,
                mSuccessCallbackMock,
                mFailureCallbackMock);

        ArgumentCaptor<OnSuccessListener<List<WebauthnCredentialDetails>>> successCallbackCaptor =
                ArgumentCaptor.forClass(OnSuccessListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokeFido2GetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        successCallbackCaptor.capture(),
                        any());
        verify(mFido2ApiCallHelperMock, never())
                .invokePasskeyCacheGetCredentials(any(), any(), any(), any());

        successCallbackCaptor.getValue().onSuccess(mCredentials);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testGetCredentials_getMatchingCredentialIds_invokesFido2() {
        GmsCoreUtils.setGmsCoreVersionForTesting(244400000);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "WebAuthentication.Android.GmsCoreGetCredentialsResult",
                                GmsCoreGetCredentialsResult.FIDO2_SUCCESS)
                        .expectNoRecords("WebAuthentication.CredentialFetchDuration.GmsCore")
                        .build();

        mHelper.getCredentials(
                mAuthenticationContextProviderMock,
                RP_ID,
                Reason.GET_MATCHING_CREDENTIAL_IDS,
                mSuccessCallbackMock,
                mFailureCallbackMock);

        ArgumentCaptor<OnSuccessListener<List<WebauthnCredentialDetails>>> successCallbackCaptor =
                ArgumentCaptor.forClass(OnSuccessListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokeFido2GetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        successCallbackCaptor.capture(),
                        any());
        verify(mFido2ApiCallHelperMock, never())
                .invokePasskeyCacheGetCredentials(any(), any(), any(), any());

        successCallbackCaptor.getValue().onSuccess(mCredentials);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testGetCredentials_checkForMatchingCredentials_invokesFido2() {
        GmsCoreUtils.setGmsCoreVersionForTesting(244400000);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "WebAuthentication.Android.GmsCoreGetCredentialsResult",
                                GmsCoreGetCredentialsResult.FIDO2_SUCCESS)
                        .expectNoRecords("WebAuthentication.CredentialFetchDuration.GmsCore")
                        .build();

        mHelper.getCredentials(
                mAuthenticationContextProviderMock,
                RP_ID,
                Reason.CHECK_FOR_MATCHING_CREDENTIALS,
                mSuccessCallbackMock,
                mFailureCallbackMock);

        ArgumentCaptor<OnSuccessListener<List<WebauthnCredentialDetails>>> successCallbackCaptor =
                ArgumentCaptor.forClass(OnSuccessListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokeFido2GetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        successCallbackCaptor.capture(),
                        any());
        verify(mFido2ApiCallHelperMock, never())
                .invokePasskeyCacheGetCredentials(any(), any(), any(), any());

        successCallbackCaptor.getValue().onSuccess(mCredentials);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testGetCredentials_featureEnabled_cacheFailureFallbackSuccess() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "WebAuthentication.Android.GmsCoreGetCredentialsResult",
                                GmsCoreGetCredentialsResult.CACHE_FAILURE_FALLBACK_SUCCESS)
                        .expectAnyRecord("WebAuthentication.CredentialFetchDuration.GmsCore")
                        .expectAnyRecord(
                                "WebAuthentication.CredentialFetchDuration.GmsCore.CacheFallback")
                        .build();

        Exception e = new Exception();

        mHelper.getCredentials(
                mAuthenticationContextProviderMock,
                RP_ID,
                Reason.GET_ASSERTION_NON_GOOGLE,
                mSuccessCallbackMock,
                mFailureCallbackMock);

        ArgumentCaptor<OnFailureListener> cacheFailureCaptor =
                ArgumentCaptor.forClass(OnFailureListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokePasskeyCacheGetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        any(),
                        cacheFailureCaptor.capture());
        cacheFailureCaptor.getValue().onFailure(e);

        @SuppressWarnings("unchecked")
        ArgumentCaptor<OnSuccessListener<List<WebauthnCredentialDetails>>> fido2SuccessCaptor =
                ArgumentCaptor.forClass(OnSuccessListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokeFido2GetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        fido2SuccessCaptor.capture(),
                        any());
        fido2SuccessCaptor.getValue().onSuccess(mCredentials);

        verify(mSuccessCallbackMock).onCredentialsReceived(mCredentials);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testGetCredentials_featureEnabled_cacheFailureFallbackFailure() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "WebAuthentication.Android.GmsCoreGetCredentialsResult",
                        GmsCoreGetCredentialsResult.CACHE_FAILURE_FALLBACK_FAILURE);
        Exception e1 = new Exception("cache");
        Exception e2 = new Exception("fido2");

        mHelper.getCredentials(
                mAuthenticationContextProviderMock,
                RP_ID,
                Reason.GET_ASSERTION_NON_GOOGLE,
                mSuccessCallbackMock,
                mFailureCallbackMock);

        ArgumentCaptor<OnFailureListener> cacheFailureCaptor =
                ArgumentCaptor.forClass(OnFailureListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokePasskeyCacheGetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        any(),
                        cacheFailureCaptor.capture());
        cacheFailureCaptor.getValue().onFailure(e1);

        ArgumentCaptor<OnFailureListener> fido2FailureCaptor =
                ArgumentCaptor.forClass(OnFailureListener.class);
        verify(mFido2ApiCallHelperMock)
                .invokeFido2GetCredentials(
                        eq(mAuthenticationContextProviderMock),
                        eq(RP_ID),
                        any(),
                        fido2FailureCaptor.capture());
        fido2FailureCaptor.getValue().onFailure(e2);

        verify(mFailureCallbackMock).onFailure(e2);
        histogramWatcher.assertExpected();
    }
}
