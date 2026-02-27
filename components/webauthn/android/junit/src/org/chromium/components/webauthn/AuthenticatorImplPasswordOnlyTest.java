// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.blink.mojom.GetCredentialOptions;
import org.chromium.blink.mojom.Mediation;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.DeviceFeatureList;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** Tests for {@link AuthenticatorImpl} with password-only requests. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
@SmallTest
@EnableFeatures({
    DeviceFeatureList.WEBAUTHN_AUTHENTICATOR_PASSWORDS_ONLY_IMMEDIATE_REQUESTS,
    DeviceFeatureList.WEBAUTHN_IMMEDIATE_GET
})
public class AuthenticatorImplPasswordOnlyTest {
    private AuthenticatorImpl mAuthenticator;
    private Origin mOrigin;
    private Origin mTopOrigin;

    @Mock private WebContents mWebContents;
    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private FidoIntentSender mIntentSender;
    @Mock private WebauthnModeProvider mModeProviderMock;
    @Mock private Fido2CredentialRequest mFido2CredentialRequestMock;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mOrigin = Origin.create(new GURL("https://example.com"));
        mTopOrigin = Origin.create(new GURL("https://example.com"));

        when(mRenderFrameHost.getLastCommittedOrigin()).thenReturn(mOrigin);

        WebauthnModeProvider.setInstanceForTesting(mModeProviderMock);
        when(mModeProviderMock.getWebauthnMode(any())).thenReturn(WebauthnMode.CHROME);
        when(mModeProviderMock.getGlobalWebauthnMode()).thenReturn(WebauthnMode.CHROME);
        AuthenticatorImpl.overrideFido2CredentialRequestForTesting(mFido2CredentialRequestMock);

        when(mWebContents.getVisibility()).thenReturn(Visibility.VISIBLE);

        mAuthenticator =
                new AuthenticatorImpl(
                        ApplicationProvider.getApplicationContext(),
                        mWebContents,
                        mIntentSender,
                        /* createConfirmationUiDelegate= */ null,
                        mRenderFrameHost,
                        mTopOrigin);
    }

    @After
    public void tearDown() {
        WebauthnModeProvider.setInstanceForTesting(null);
        AuthenticatorImpl.overrideFido2CredentialRequestForTesting(null);
    }

    @Test
    public void testGetCredential_passwordOnlyImmediate_callsFido2Request() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        GetCredentialOptions options = new GetCredentialOptions();
        options.password = true;
        options.publicKey = null;
        options.mediation = Mediation.IMMEDIATE;
        Authenticator.GetCredential_Response callback =
                mock(Authenticator.GetCredential_Response.class);

        mAuthenticator.getCredential(options, callback);

        verify(mFido2CredentialRequestMock)
                .handleGetCredentialRequest(eq(options), any(), any(), any());
    }

    @Test
    public void testGetCredential_passwordOnlyImmediate_webView_credManDisabled_fails() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        when(mModeProviderMock.getWebauthnMode(any())).thenReturn(WebauthnMode.BROWSER); // WebView
        when(mModeProviderMock.getGlobalWebauthnMode()).thenReturn(WebauthnMode.BROWSER);

        GetCredentialOptions options = new GetCredentialOptions();
        options.password = true;
        options.publicKey = null;
        options.mediation = Mediation.IMMEDIATE;
        Authenticator.GetCredential_Response callback =
                mock(Authenticator.GetCredential_Response.class);

        mAuthenticator.getCredential(options, callback);

        // Should NOT call handleGetCredentialRequest
        verify(mFido2CredentialRequestMock, org.mockito.Mockito.never())
                .handleGetCredentialRequest(any(), any(), any(), any());
    }

    @Test
    public void testGetCredential_combinedRequest_callsFido2Request() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        GetCredentialOptions options = new GetCredentialOptions();
        options.password = true;
        options.publicKey = new PublicKeyCredentialRequestOptions(); // Not null
        options.mediation = Mediation.IMMEDIATE;
        Authenticator.GetCredential_Response callback =
                mock(Authenticator.GetCredential_Response.class);

        mAuthenticator.getCredential(options, callback);

        verify(mFido2CredentialRequestMock)
                .handleGetCredentialRequest(eq(options), any(), any(), any());
    }
}
