// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Bundle;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.identitycredentials.CreateCredentialException;
import com.google.android.gms.identitycredentials.CreateCredentialHandle;
import com.google.android.gms.identitycredentials.CreateCredentialRequest;
import com.google.android.gms.identitycredentials.CreateCredentialResponse;
import com.google.android.gms.identitycredentials.IdentityCredentialClient;
import com.google.android.gms.identitycredentials.SignalCredentialStateRequest;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.TaskCompletionSource;
import com.google.android.gms.tasks.Tasks;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.AllAcceptedCredentialsOptions;
import org.chromium.blink.mojom.CurrentUserDetailsOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialReportOptions;
import org.chromium.components.webauthn.AndroidApiCallMetrics.ApiCallEvent;
import org.chromium.content_public.browser.RenderFrameHost;

@RunWith(BaseRobolectricTestRunner.class)
public class IdentityCredentialsHelperRobolectricTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private static final String ORIGIN_STRING = "https://subdomain.coolwebsitekayserispor.com";
    private static final byte[] CLIENT_DATA_HASH = new byte[] {1, 2, 3};
    private static final String REQUEST_JSON = "{\"a\":\"bc\"}";
    private static final String CREATE_HISTOGRAM =
            "WebAuthentication.Android.IdentityCredentialsApiCall.CreateCredential";
    private static final String SIGNAL_HISTOGRAM =
            "WebAuthentication.Android.IdentityCredentialsApiCall.SignalCredentialState";

    @Mock private Context mContext;
    @Mock private RenderFrameHost mFrameHost;
    @Mock private AuthenticationContextProvider mAuthenticationContextProviderMock;
    @Mock private IdentityCredentialClient mClientMock;
    @Mock private WebauthnRequestCallback mRequestCallback;

    private IdentityCredentialsHelper mHelper;

    @Before
    public void setUp() throws Exception {

        Fido2ApiTestHelper.mockFido2CredentialRequestJni();
        Fido2ApiTestHelper.mockClientDataJson(REQUEST_JSON);

        when(mAuthenticationContextProviderMock.getIntentSender()).thenReturn(null);
        when(mAuthenticationContextProviderMock.getContext()).thenReturn(mContext);
        when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(mFrameHost);
        when(mAuthenticationContextProviderMock.getRequestCallback()).thenReturn(mRequestCallback);
        mHelper = spy(new IdentityCredentialsHelper(mAuthenticationContextProviderMock));
        doReturn(mClientMock).when(mHelper).getClient();
    }

    private void verifyBuildSignalCredentialStateRequest(
            PublicKeyCredentialReportOptions options, String expectedType) {
        SignalCredentialStateRequest request =
                mHelper.buildSignalCredentialStateRequest(options, ORIGIN_STRING);

        assertThat(request).isNotNull();
        assertThat(request.getType()).isEqualTo(expectedType);
        assertThat(request.getOrigin()).isEqualTo(ORIGIN_STRING);

        Bundle expectedRequestData = new Bundle();
        expectedRequestData.putString(
                "androidx.credentials.signal_request_json_key",
                Fido2ApiTestHelper.TEST_SERIALIZED_REPORT_REQUEST_JSON);
        assertThat(request.getRequestData().toString()).isEqualTo(expectedRequestData.toString());
    }

    @Test
    public void testBuildConditionalCreateRequest() {
        PublicKeyCredentialCreationOptions options =
                Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        options.isConditional = true;
        CreateCredentialRequest request =
                mHelper.buildConditionalCreateRequest(options, ORIGIN_STRING, CLIENT_DATA_HASH);

        assertThat(request).isNotNull();
        assertThat(request.getType()).isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(request.getOrigin()).isEqualTo(ORIGIN_STRING);
        assertThat(request.getRequestJson())
                .isEqualTo(Fido2ApiTestHelper.TEST_SERIALIZED_MAKE_CREDENTIAL_REQUEST_JSON);

        Bundle expectedCredentialData = new Bundle();
        expectedCredentialData.putString(
                "androidx.credentials.BUNDLE_KEY_SUBTYPE",
                "androidx.credentials.BUNDLE_VALUE_SUBTYPE_CREATE_PUBLIC_KEY_CREDENTIAL_REQUEST");
        expectedCredentialData.putString(
                "androidx.credentials.BUNDLE_KEY_REQUEST_JSON",
                Fido2ApiTestHelper.TEST_SERIALIZED_MAKE_CREDENTIAL_REQUEST_JSON);
        expectedCredentialData.putByteArray(
                "androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH", CLIENT_DATA_HASH);
        Bundle displayInfo = new Bundle();
        displayInfo.putCharSequence("androidx.credentials.BUNDLE_KEY_USER_ID", options.user.name);
        displayInfo.putCharSequence(
                "androidx.credentials.BUNDLE_KEY_USER_DISPLAY_NAME", options.user.displayName);
        expectedCredentialData.putBundle(
                "androidx.credentials.BUNDLE_KEY_REQUEST_DISPLAY_INFO", displayInfo);
        assertThat(request.getCredentialData().toString())
                .isEqualTo(expectedCredentialData.toString());

        Bundle expectedQueryData = new Bundle();
        expectedQueryData.putString(
                "androidx.credentials.BUNDLE_KEY_SUBTYPE",
                "androidx.credentials.BUNDLE_VALUE_SUBTYPE_CREATE_PUBLIC_KEY_CREDENTIAL_REQUEST");
        expectedQueryData.putString(
                "androidx.credentials.BUNDLE_KEY_REQUEST_JSON",
                Fido2ApiTestHelper.TEST_SERIALIZED_MAKE_CREDENTIAL_REQUEST_JSON);
        expectedQueryData.putByteArray(
                "androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH", CLIENT_DATA_HASH);
        expectedQueryData.putBoolean(
                "androidx.credentials.BUNDLE_KEY_IS_CONDITIONAL_REQUEST", true);
        assertThat(request.getCandidateQueryData().toString())
                .isEqualTo(expectedQueryData.toString());
    }

    @Test
    public void testBuildSignalCredentialStateRequest_unknownCredentialId() {
        PublicKeyCredentialReportOptions options = new PublicKeyCredentialReportOptions();
        options.relyingPartyId = ORIGIN_STRING;
        options.unknownCredentialId = new byte[] {1, 2, 3, 4};
        verifyBuildSignalCredentialStateRequest(
                options, "androidx.credentials.SIGNAL_UNKNOWN_CREDENTIAL_STATE_REQUEST_TYPE");
    }

    @Test
    public void testBuildSignalCredentialStateRequest_allAcceptedCredentials() {
        PublicKeyCredentialReportOptions options = new PublicKeyCredentialReportOptions();
        options.relyingPartyId = ORIGIN_STRING;
        options.allAcceptedCredentials = new AllAcceptedCredentialsOptions();
        options.allAcceptedCredentials.userId = new byte[] {1, 2, 3, 4};
        options.allAcceptedCredentials.allAcceptedCredentialsIds =
                new byte[][] {{6, 5, 4}, {3, 2, 1}};
        verifyBuildSignalCredentialStateRequest(
                options, "androidx.credentials.SIGNAL_ALL_ACCEPTED_CREDENTIALS_REQUEST_TYPE");
    }

    @Test
    public void testBuildSignalCredentialStateRequest_currentUserDetails() {
        PublicKeyCredentialReportOptions options = new PublicKeyCredentialReportOptions();
        options.relyingPartyId = ORIGIN_STRING;
        options.currentUserDetails = new CurrentUserDetailsOptions();
        options.currentUserDetails.userId = new byte[] {1, 2, 3, 4};
        options.currentUserDetails.name = "username";
        options.currentUserDetails.displayName = "displayName";
        verifyBuildSignalCredentialStateRequest(
                options, "androidx.credentials.SIGNAL_CURRENT_USER_DETAILS_STATE_REQUEST_TYPE");
    }

    @Test
    public void testHandleConditionalCreateRequest_recordsTerminalOutcome() {
        assertCreateOutcome(Tasks.forResult(createHandle()), ApiCallEvent.SUCCEEDED);
        assertCreateOutcome(Tasks.forCanceled(), ApiCallEvent.CANCELLED);
        assertCreateOutcome(
                Tasks.forException(
                        new CreateCredentialException(
                                CreateCredentialException.ERROR_TYPE_USER_CANCELED, "canceled")),
                ApiCallEvent.CANCELLED);
        assertCreateOutcome(
                Tasks.forException(new ApiException(Status.RESULT_TIMEOUT)),
                ApiCallEvent.TIMED_OUT);
        assertCreateOutcome(
                Tasks.forException(new ApiException(Status.RESULT_INTERNAL_ERROR)),
                ApiCallEvent.FAILED);
    }

    @Test
    public void testHandleConditionalCreateRequest_unresolved_recordsOnlyStarted() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(CREATE_HISTOGRAM, ApiCallEvent.STARTED);
        when(mClientMock.createCredential(any()))
                .thenReturn(new TaskCompletionSource<CreateCredentialHandle>().getTask());
        startConditionalCreate();
        watcher.assertExpected();
    }

    @Test
    public void testHandleReportRequest_success_recordsSucceeded() {
        HistogramWatcher watcher = expectApiCall(SIGNAL_HISTOGRAM, ApiCallEvent.SUCCEEDED);
        when(mClientMock.signalCredentialState(any())).thenReturn(Tasks.forResult(null));
        startReport();
        watcher.assertExpected();
    }

    @Test
    public void testApiCallMetrics_synchronousDispatchException_recordsFailed() {
        HistogramWatcher createWatcher = expectApiCall(CREATE_HISTOGRAM, ApiCallEvent.FAILED);
        HistogramWatcher signalWatcher = expectApiCall(SIGNAL_HISTOGRAM, ApiCallEvent.FAILED);
        when(mClientMock.createCredential(any())).thenThrow(new IllegalStateException());
        when(mClientMock.signalCredentialState(any())).thenThrow(new IllegalStateException());
        startConditionalCreate();
        startReport();
        createWatcher.assertExpected();
        signalWatcher.assertExpected();
    }

    /** Expects exactly one STARTED and one {@code terminalEvent} sample in {@code histogram}. */
    private static HistogramWatcher expectApiCall(
            String histogram, @ApiCallEvent int terminalEvent) {
        return HistogramWatcher.newBuilder()
                .expectIntRecords(histogram, ApiCallEvent.STARTED, terminalEvent)
                .build();
    }

    private void assertCreateOutcome(Task<CreateCredentialHandle> task, @ApiCallEvent int event) {
        HistogramWatcher watcher = expectApiCall(CREATE_HISTOGRAM, event);
        when(mClientMock.createCredential(any())).thenReturn(task);
        startConditionalCreate();
        watcher.assertExpected();
    }

    private void startConditionalCreate() {
        PublicKeyCredentialCreationOptions options =
                Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        mHelper.handleConditionalCreateRequest(options, ORIGIN_STRING, null, CLIENT_DATA_HASH);
        ShadowLooper.idleMainLooper();
    }

    private void startReport() {
        PublicKeyCredentialReportOptions options = new PublicKeyCredentialReportOptions();
        options.relyingPartyId = ORIGIN_STRING;
        options.unknownCredentialId = new byte[] {1, 2, 3, 4};
        mHelper.handleReportRequest(options, ORIGIN_STRING);
    }

    private static CreateCredentialHandle createHandle() {
        Bundle data = new Bundle();
        data.putString("androidx.credentials.BUNDLE_KEY_REGISTRATION_RESPONSE_JSON", "{}");
        return new CreateCredentialHandle(
                /* pendingIntent= */ null,
                new CreateCredentialResponse(
                        "androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL", data));
    }
}
