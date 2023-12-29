// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.credentials.CreateCredentialRequest;
import android.os.Bundle;
import android.util.Base64;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

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
public class GpmCredManRequestDecoratorRobolectricTest {
    private static final String ORIGIN = "www.coolwebsite.com";
    private static final byte[] USER_ID = new byte[] {3, 5, 8};

    private CreateCredentialRequest.Builder mBuilder =
            Shadow.newInstanceOf(CreateCredentialRequest.Builder.class);
    @Mock private CredManCreateCredentialRequestHelper mHelper;

    private GpmCredManRequestDecorator mDecorator = GpmCredManRequestDecorator.getInstance();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.openMocks(this);
    }

    @Test
    @SmallTest
    public void testUpdateCreateCredentialRequestBundle() {
        when(mHelper.getUserId()).thenReturn(USER_ID);
        Bundle bundle = new Bundle();

        mDecorator.updateCreateCredentialRequestBundle(bundle, mHelper);

        verify(mHelper).getUserId();
        Bundle displayInfoBundle =
                bundle.getBundle("androidx.credentials.BUNDLE_KEY_REQUEST_DISPLAY_INFO");
        assertThat(displayInfoBundle).isNotNull();
        assertThat(displayInfoBundle.getCharSequence("androidx.credentials.BUNDLE_KEY_USER_ID"))
                .isEqualTo(
                        Base64.encodeToString(
                                USER_ID, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP));
        assertThat(displayInfoBundle.getString("androidx.credentials.BUNDLE_KEY_DEFAULT_PROVIDER"))
                .contains("com.google.android.gms");
        assertThat(bundle.containsKey("com.android.chrome.CHANNEL")).isTrue();
    }

    @Test
    @SmallTest
    public void testUpdateCreateCredentialRequestBuilder() {
        when(mHelper.getOrigin()).thenReturn(ORIGIN);

        mDecorator.updateCreateCredentialRequestBuilder(mBuilder, mHelper);

        ShadowCreateCredentialRequest.ShadowBuilder shadowBuilder = Shadow.extract(mBuilder);
        assertThat(shadowBuilder.getOrigin()).isEqualTo(ORIGIN);
    }
}
