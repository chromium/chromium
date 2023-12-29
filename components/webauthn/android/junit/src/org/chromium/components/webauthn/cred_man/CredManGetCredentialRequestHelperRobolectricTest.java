// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.credentials.CredentialOption;
import android.credentials.GetCredentialRequest;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            ShadowCredentialOption.class,
            ShadowCredentialOption.ShadowBuilder.class,
            ShadowGetCredentialRequest.class,
            ShadowGetCredentialRequest.ShadowBuilder.class,
            ShadowGetCredentialResponse.class,
            ShadowGetCredentialException.class
        })
public class CredManGetCredentialRequestHelperRobolectricTest {
    private static final String REQUEST_AS_JSON = "coolest-request-as-json";
    private static final byte[] CLIENT_DATA_HASH = new byte[] {1, 1, 2};

    @Mock private CredManRequestDecorator mDecorator;

    private CredManGetCredentialRequestHelper mHelper;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.openMocks(this);
        mHelper =
                new CredManGetCredentialRequestHelper.Builder(
                                REQUEST_AS_JSON,
                                CLIENT_DATA_HASH,
                                /* preferImmediatelyAvailable= */ false,
                                /* allowAutoSelect= */ false,
                                /* requestPasswords= */ false)
                        .build();
    }

    @Test
    @SmallTest
    public void testGetGetCredentialRequest_nullDecorator_setsBasicGetCredentialRequest() {
        GetCredentialRequest getCredentialRequest = mHelper.getGetCredentialRequest(null);

        assertThat(getCredentialRequest).isNotNull();
        assertThat(
                        getCredentialRequest
                                .getData()
                                .containsKey(
                                        "androidx.credentials.BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS"))
                .isTrue();
    }

    @Test
    @SmallTest
    public void testGetGetCredentialRequest_mockDecorator_setsBasicGetCredentialRequest() {
        GetCredentialRequest getCredentialRequest = mHelper.getGetCredentialRequest(mDecorator);

        verify(mDecorator).updateGetCredentialRequestBuilder(any(), eq(mHelper));
        verify(mDecorator).updateGetCredentialRequestBundle(any(), eq(mHelper));
        assertThat(getCredentialRequest).isNotNull();
        assertThat(
                        getCredentialRequest
                                .getData()
                                .containsKey(
                                        "androidx.credentials.BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS"))
                .isTrue();
    }

    @Test
    @SmallTest
    public void testGetGetCredentialRequest_nullDecorator_firstCredentialOptionIsPublicKey() {
        GetCredentialRequest getCredentialRequest = mHelper.getGetCredentialRequest(null);

        assertThat(getCredentialRequest).isNotNull();
        assertThat(getCredentialRequest.getCredentialOptions()).hasSize(1);

        CredentialOption option = getCredentialRequest.getCredentialOptions().get(0);
        assertThat(option.getType()).isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(
                        option.getCandidateQueryData()
                                .getString("androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo(REQUEST_AS_JSON);
        assertThat(
                        option.getCandidateQueryData()
                                .getByteArray("androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH"))
                .isEqualTo(CLIENT_DATA_HASH);
        assertThat(
                        option.getCandidateQueryData()
                                .getString("androidx.credentials.BUNDLE_KEY_SUBTYPE"))
                .isEqualTo(
                        "androidx.credentials.BUNDLE_VALUE_SUBTYPE_GET_PUBLIC_KEY_CREDENTIAL_OPTION");
    }

    @Test
    @SmallTest
    public void testGetGetCredentialRequest_mockDecorator_firstCredentialOptionIsPublicKey() {
        GetCredentialRequest getCredentialRequest = mHelper.getGetCredentialRequest(mDecorator);

        verify(mDecorator).updatePublicKeyCredentialOptionBundle(any(), eq(mHelper));

        assertThat(getCredentialRequest).isNotNull();
        assertThat(getCredentialRequest.getCredentialOptions()).hasSize(1);

        CredentialOption option = getCredentialRequest.getCredentialOptions().get(0);
        assertThat(option.getType()).isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(
                        option.getCandidateQueryData()
                                .getString("androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo(REQUEST_AS_JSON);
        assertThat(
                        option.getCandidateQueryData()
                                .getByteArray("androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH"))
                .isEqualTo(CLIENT_DATA_HASH);
        assertThat(
                        option.getCandidateQueryData()
                                .getString("androidx.credentials.BUNDLE_KEY_SUBTYPE"))
                .isEqualTo(
                        "androidx.credentials.BUNDLE_VALUE_SUBTYPE_GET_PUBLIC_KEY_CREDENTIAL_OPTION");
    }

    @Test
    @SmallTest
    public void
            testGetGetCredentialRequest_nullDecoratorAndRequestPasswordsIsTrue_BothOptionsInRequest() {
        mHelper =
                new CredManGetCredentialRequestHelper.Builder(
                                REQUEST_AS_JSON,
                                CLIENT_DATA_HASH,
                                /* preferImmediatelyAvailable= */ false,
                                /* allowAutoSelect= */ false,
                                /* requestPasswords= */ true)
                        .build();

        GetCredentialRequest getCredentialRequest = mHelper.getGetCredentialRequest(null);

        assertThat(getCredentialRequest).isNotNull();
        assertThat(getCredentialRequest.getCredentialOptions()).hasSize(2);
        CredentialOption option = getCredentialRequest.getCredentialOptions().get(1);
        assertThat(option.getType()).isEqualTo("android.credentials.TYPE_PASSWORD_CREDENTIAL");
    }

    @Test
    @SmallTest
    public void
            testGetGetCredentialRequest_mockDecoratorAndRequestPasswordsIsTrue_BothOptionsInRequest() {
        mHelper =
                new CredManGetCredentialRequestHelper.Builder(
                                REQUEST_AS_JSON,
                                CLIENT_DATA_HASH,
                                /* preferImmediatelyAvailable= */ false,
                                /* allowAutoSelect= */ false,
                                /* requestPasswords= */ true)
                        .build();

        GetCredentialRequest getCredentialRequest = mHelper.getGetCredentialRequest(mDecorator);

        verify(mDecorator).updatePasswordCredentialOptionBundle(any(), eq(mHelper));
        verify(mDecorator).updatePasswordCredentialOptionBuilder(any(), eq(mHelper));
        assertThat(getCredentialRequest).isNotNull();
        assertThat(getCredentialRequest.getCredentialOptions()).hasSize(2);
        CredentialOption option = getCredentialRequest.getCredentialOptions().get(1);
        assertThat(option.getType()).isEqualTo("android.credentials.TYPE_PASSWORD_CREDENTIAL");
    }
}
