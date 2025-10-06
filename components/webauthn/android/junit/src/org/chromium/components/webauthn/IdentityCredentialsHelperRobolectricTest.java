// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import com.google.android.gms.identitycredentials.CreateCredentialRequest;
import com.google.android.gms.identitycredentials.SignalCredentialStateRequest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.AllAcceptedCredentialsOptions;
import org.chromium.blink.mojom.CurrentUserDetailsOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialReportOptions;
import org.chromium.content_public.browser.RenderFrameHost;

@RunWith(BaseRobolectricTestRunner.class)
public class IdentityCredentialsHelperRobolectricTest {
    private static final String ORIGIN_STRING = "https://subdomain.coolwebsitekayserispor.com";
    private static final byte[] CLIENT_DATA_HASH = new byte[] {1, 2, 3};
    private static final String REQUEST_JSON = "{\"a\":\"bc\"}";

    @Mock private Context mContext;
    @Mock private RenderFrameHost mFrameHost;
    @Mock private AuthenticationContextProvider mAuthenticationContextProviderMock;

    private IdentityCredentialsHelper mHelper;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.openMocks(this);

        Fido2ApiTestHelper.mockFido2CredentialRequestJni();
        Fido2ApiTestHelper.mockClientDataJson(REQUEST_JSON);

        when(mAuthenticationContextProviderMock.getIntentSender()).thenReturn(null);
        when(mAuthenticationContextProviderMock.getContext()).thenReturn(mContext);
        when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(mFrameHost);
        mHelper = new IdentityCredentialsHelper(mAuthenticationContextProviderMock);
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
    @SmallTest
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
    @SmallTest
    public void testBuildSignalCredentialStateRequest_unknownCredentialId() {
        PublicKeyCredentialReportOptions options = new PublicKeyCredentialReportOptions();
        options.relyingPartyId = ORIGIN_STRING;
        options.unknownCredentialId = new byte[] {1, 2, 3, 4};
        verifyBuildSignalCredentialStateRequest(
                options, "androidx.credentials.SIGNAL_UNKNOWN_CREDENTIAL_STATE_REQUEST_TYPE");
    }

    @Test
    @SmallTest
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
    @SmallTest
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
}
