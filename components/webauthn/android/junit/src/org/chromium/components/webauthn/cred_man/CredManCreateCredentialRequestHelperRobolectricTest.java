// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.credentials.CreateCredentialRequest;

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
            ShadowCreateCredentialRequest.class,
            ShadowCreateCredentialRequest.ShadowBuilder.class,
            ShadowCreateCredentialResponse.class,
            ShadowCreateCredentialException.class
        })
public class CredManCreateCredentialRequestHelperRobolectricTest {
    private static final String REQUEST_AS_JSON = "coolest-request-as-json";
    private static final byte[] CLIENT_DATA_HASH = new byte[] {1, 1, 2};
    private static final String ORIGIN = "www.coolwebsite.com";
    private static final byte[] USER_ID = new byte[] {3, 5, 8};

    @Mock private CredManRequestDecorator mDecorator;

    private CredManCreateCredentialRequestHelper mHelper;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.openMocks(this);

        mHelper =
                new CredManCreateCredentialRequestHelper.Builder(REQUEST_AS_JSON, CLIENT_DATA_HASH)
                        .setOrigin(ORIGIN)
                        .setUserId(USER_ID)
                        .build();
    }

    @Test
    @SmallTest
    public void testGetCreateCredentialRequest_nullDecorator_onlyRequiredValues() {
        CreateCredentialRequest createCredentialRequest = mHelper.getCreateCredentialRequest(null);

        assertThat(createCredentialRequest).isNotNull();
        assertThat(createCredentialRequest.getType())
                .isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(
                        createCredentialRequest
                                .getCredentialData()
                                .getString("androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo(REQUEST_AS_JSON);
        assertThat(
                        createCredentialRequest
                                .getCredentialData()
                                .getByteArray("androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH"))
                .isEqualTo(CLIENT_DATA_HASH);
        assertThat(createCredentialRequest.alwaysSendAppInfoToProvider()).isTrue();
    }

    @Test
    @SmallTest
    public void testGetCreateCredentialRequest_mockDecorator_setsOriginAndBundleValues() {
        mHelper.getCreateCredentialRequest(mDecorator);

        verify(mDecorator).updateCreateCredentialRequestBuilder(any(), eq(mHelper));
        verify(mDecorator).updateCreateCredentialRequestBundle(any(), eq(mHelper));
    }
}
