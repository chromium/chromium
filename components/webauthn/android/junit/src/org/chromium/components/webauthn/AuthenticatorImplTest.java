// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.blink.mojom.WebAuthnClientCapability;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** Tests for {@link AuthenticatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
@SmallTest
public class AuthenticatorImplTest {
    private AuthenticatorImpl mAuthenticator;
    private Origin mOrigin;
    private Origin mTopOrigin;

    @Mock private WebContents mWebContents;
    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private FidoIntentSender mIntentSender;
    @Mock private WebauthnModeProvider mModeProviderMock;
    @Mock private Fido2CredentialRequest mFido2CredentialRequestMock;

    @Captor private ArgumentCaptor<IsUvpaaResponseCallback> mIsUvpaaCallbackCaptor;
    @Captor private ArgumentCaptor<WebAuthnClientCapability[]> mCapabilitiesCaptor;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private void invokeIsUvpaaCallback(boolean isUvpaaAvailable) {
        doAnswer(
                        invocation -> {
                            IsUvpaaResponseCallback cb = invocation.getArgument(0);
                            cb.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                                    isUvpaaAvailable);
                            return null;
                        })
                .when(mFido2CredentialRequestMock)
                .handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(any());
    }

    @Before
    public void setUp() {
        mOrigin = Origin.create(new GURL("https://example.com"));
        mTopOrigin = Origin.create(new GURL("https://example.com"));

        when(mRenderFrameHost.getLastCommittedOrigin()).thenReturn(mOrigin);

        WebauthnModeProvider.setInstanceForTesting(mModeProviderMock);
        when(mModeProviderMock.getWebauthnMode(any())).thenReturn(WebauthnMode.CHROME);
        when(mModeProviderMock.getGlobalWebauthnMode()).thenReturn(WebauthnMode.CHROME);
        AuthenticatorImpl.overrideFido2CredentialRequestForTesting(mFido2CredentialRequestMock);

        invokeIsUvpaaCallback(true);
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
    public void testGetClientCapabilities_CallsIsUvpaa() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        Authenticator.GetClientCapabilities_Response callback =
                mock(Authenticator.GetClientCapabilities_Response.class);

        mAuthenticator.getClientCapabilities(callback);

        verify(callback).call(mCapabilitiesCaptor.capture());
        assertEquals(5, mCapabilitiesCaptor.getValue().length);
    }

    @Test
    public void testGetClientCapabilities_RelatedOrigins_ShouldBeSupported() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        testCapability(AuthenticatorConstants.CAPABILITY_RELATED_ORIGINS, true);
    }

    @Test
    public void testGetClientCapabilities_HybridTransport_ShouldBeSupported() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        testCapability(AuthenticatorConstants.CAPABILITY_HYBRID_TRANSPORT, true);
    }

    @Test
    public void testGetClientCapabilities_Ppaa_ShouldBeSupported() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        testCapability(AuthenticatorConstants.CAPABILITY_PPAA, true);
    }

    @Test
    public void testGetClientCapabilities_ConditionalGet_Supported_WhenUvpaaAvailable() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        testCapability(AuthenticatorConstants.CAPABILITY_CONDITIONAL_GET, true);
    }

    @Test
    public void testGetClientCapabilities_Uvpaa_Supported() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        testCapability(AuthenticatorConstants.CAPABILITY_UVPAA, true);
    }

    @Test
    public void testGetClientCapabilities_Uvpaa_NotSupported() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION);
        invokeIsUvpaaCallback(false);
        testCapability(AuthenticatorConstants.CAPABILITY_UVPAA, false);
    }

    @Test
    public void testGetClientCapabilities_GmsCoreTooOld() {
        GmsCoreUtils.setGmsCoreVersionForTesting(GmsCoreUtils.GMSCORE_MIN_VERSION - 1);
        Authenticator.GetClientCapabilities_Response callback =
                mock(Authenticator.GetClientCapabilities_Response.class);

        mAuthenticator.getClientCapabilities(callback);

        verify(mFido2CredentialRequestMock, never())
                .handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(any());

        verify(callback).call(mCapabilitiesCaptor.capture());
        WebAuthnClientCapability[] capabilities = mCapabilitiesCaptor.getValue();
        assertEquals(5, capabilities.length);
        assertCapabilitySupported(
                capabilities, AuthenticatorConstants.CAPABILITY_CONDITIONAL_GET, false);
        assertCapabilitySupported(capabilities, AuthenticatorConstants.CAPABILITY_UVPAA, false);
        assertCapabilitySupported(
                capabilities, AuthenticatorConstants.CAPABILITY_RELATED_ORIGINS, true);
        assertCapabilitySupported(
                capabilities, AuthenticatorConstants.CAPABILITY_HYBRID_TRANSPORT, true);
        assertCapabilitySupported(capabilities, AuthenticatorConstants.CAPABILITY_PPAA, true);
    }

    private void testCapability(String capability, boolean expectedSupported) {
        Authenticator.GetClientCapabilities_Response callback =
                mock(Authenticator.GetClientCapabilities_Response.class);

        mAuthenticator.getClientCapabilities(callback);

        verify(callback).call(mCapabilitiesCaptor.capture());
        assertCapabilitySupported(mCapabilitiesCaptor.getValue(), capability, expectedSupported);
    }

    private void assertCapabilitySupported(
            WebAuthnClientCapability[] capabilities, String name, boolean expectedSupported) {
        for (WebAuthnClientCapability cap : capabilities) {
            if (cap.name.equals(name)) {
                assertEquals(expectedSupported, cap.supported);
                return;
            }
        }
        throw new AssertionError("Capability '" + name + "' not found.");
    }
}
