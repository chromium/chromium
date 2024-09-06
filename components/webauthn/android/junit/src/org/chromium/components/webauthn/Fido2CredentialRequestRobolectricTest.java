// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.credentials.CredentialManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ResultReceiver;

import androidx.test.filters.SmallTest;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.components.webauthn.cred_man.CredManHelper;
import org.chromium.components.webauthn.cred_man.CredManSupportProvider;
import org.chromium.components.webauthn.cred_man.ShadowCredentialManager;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.device.DeviceFeatureList;
import org.chromium.net.GURLUtils;
import org.chromium.net.GURLUtilsJni;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            ShadowCredentialManager.class,
        })
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
public class Fido2CredentialRequestRobolectricTest {
    private static final String TEST_CHANNEL_EXTRA = "stable";
    private static final Boolean TEST_INCOGNITO_EXTRA = true;
    private static final String TEST_CLIENT_DATA_JSON = "{ClientDataJSON}";

    private Fido2CredentialRequest mRequest;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private PublicKeyCredentialRequestOptions mRequestOptions;
    private Fido2ApiTestHelper.AuthenticatorCallback mCallback;
    private Origin mOrigin;
    private Bundle mBrowserOptions;
    private FakeFido2ApiCallHelper mFido2ApiCallHelper;

    @Mock private RenderFrameHost mFrameHost;
    @Mock GURLUtils.Natives mGURLUtilsJniMock;
    @Mock Activity mActivity;
    @Mock WebauthnBrowserBridge mBrowserBridgeMock;
    @Mock CredManHelper mCredManHelperMock;
    @Mock Barrier mBarrierMock;
    @Mock WebauthnModeProvider mModeProviderMock;
    @Mock AuthenticationContextProvider mAuthenticationContextProviderMock;

    @Rule public JniMocker mMocker = new JniMocker();

    @Before
    public void setUp() throws Exception {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, true);
        testValues.addFieldTrialParamOverride(
                DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, "gpm_in_cred_man", "true");
        FeatureList.setTestValues(testValues);

        MockitoAnnotations.initMocks(this);

        ShadowApplication shadowApp = ShadowApplication.getInstance();
        shadowApp.setSystemService(
                Context.CREDENTIAL_SERVICE, Shadow.newInstanceOf(CredentialManager.class));

        GURL gurl =
                new GURL(
                        "https://subdomain.example.test:443/content/test/data/android/authenticator.html");
        mOrigin = Origin.create(gurl);

        mBrowserOptions = new Bundle();
        mBrowserOptions.putString("com.android.chrome.CHANNEL", TEST_CHANNEL_EXTRA);
        mBrowserOptions.putBoolean("com.android.chrome.INCOGNITO", TEST_INCOGNITO_EXTRA);

        mMocker.mock(GURLUtilsJni.TEST_HOOKS, mGURLUtilsJniMock);
        Mockito.when(mGURLUtilsJniMock.getOrigin(any(String.class)))
                .thenReturn("https://subdomain.example.test:443");

        mFido2ApiCallHelper = new FakeFido2ApiCallHelper();
        mFido2ApiCallHelper.setArePlayServicesAvailable(true);
        Fido2ApiCallHelper.overrideInstanceForTesting(mFido2ApiCallHelper);

        mCreationOptions = Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        // Set rk=required and empty allowlist on the assumption that most test cases care about
        // exercising the passkeys case.
        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.REQUIRED;
        mRequestOptions = Fido2ApiTestHelper.createDefaultGetAssertionOptions();
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        WebauthnModeProvider.setInstanceForTesting(mModeProviderMock);
        Mockito.when(mModeProviderMock.getWebauthnMode(any())).thenReturn(WebauthnMode.CHROME);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode()).thenReturn(WebauthnMode.CHROME);
        Mockito.when(mAuthenticationContextProviderMock.getIntentSender()).thenReturn(null);
        Mockito.when(mAuthenticationContextProviderMock.getContext()).thenReturn(mActivity);
        Mockito.when(mAuthenticationContextProviderMock.getRenderFrameHost())
                .thenReturn(mFrameHost);
        mRequest = new Fido2CredentialRequest(mAuthenticationContextProviderMock);

        Fido2ApiTestHelper.mockFido2CredentialRequestJni(mMocker);
        Fido2ApiTestHelper.mockClientDataJson(mMocker, TEST_CLIENT_DATA_JSON);

        mCallback = Fido2ApiTestHelper.getAuthenticatorCallback();

        Mockito.when(mFrameHost.getLastCommittedURL()).thenReturn(gurl);
        Mockito.when(mFrameHost.getLastCommittedOrigin()).thenReturn(mOrigin);
        Mockito.doAnswer(
                        (invocation) -> {
                            ((Callback<WebAuthSecurityChecksResults>) invocation.getArguments()[3])
                                    .onResult(
                                            new WebAuthSecurityChecksResults(
                                                    AuthenticatorStatus.SUCCESS, false));
                            return null;
                        })
                .when(mFrameHost)
                .performMakeCredentialWebAuthSecurityChecks(
                        any(String.class), any(Origin.class), anyBoolean(), any(Callback.class));
        Mockito.doAnswer(
                        (invocation) -> {
                            ((Callback<WebAuthSecurityChecksResults>) invocation.getArguments()[3])
                                    .onResult(
                                            new WebAuthSecurityChecksResults(
                                                    AuthenticatorStatus.SUCCESS, false));
                            return null;
                        })
                .when(mFrameHost)
                .performGetAssertionWebAuthSecurityChecks(
                        any(String.class), any(Origin.class), anyBoolean(), any(Callback.class));

        // Reset any cached evaluation of whether CredMan should be supported.
        CredManSupportProvider.setupForTesting(true);
        mRequest.overrideBrowserBridgeForTesting(mBrowserBridgeMock);
        mRequest.setCredManHelperForTesting(mCredManHelperMock);
        mRequest.setBarrierForTesting(mBarrierMock);
    }

    @After
    public void tearDown() {
        WebauthnModeProvider.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManEnabled() {

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                /* maybeBrowserOptions= */ null,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);

        verify(mCredManHelperMock, times(1))
                .startMakeRequest(any(), any(), any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManDisabled_notUsed() {

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        FeatureList.setTestValues(testValues);
        Mockito.when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(null);

        final byte[] clientDataHash = new byte[] {1, 2, 3};
        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                clientDataHash,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);

        verify(mCredManHelperMock, times(0))
                .startMakeRequest(any(), any(), any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManDisabled_stillUsedForHybrid() {

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        FeatureList.setTestValues(testValues);
        Mockito.when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(null);

        final byte[] clientDataHash = new byte[] {1, 2, 3};
        mRequest.setIsHybridRequest(true);
        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                clientDataHash,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);

        verify(mCredManHelperMock, times(1))
                .startMakeRequest(any(), any(), any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testMakeCredential_rkDisabledWithExplicitHash_success() {

        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.DISCOURAGED;
        final byte[] clientDataHash = new byte[] {1, 2, 3};
        Mockito.when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(null);

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                clientDataHash,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);

        verify(mCredManHelperMock, times(0))
                .startMakeRequest(any(), any(), any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testMakeCredential_rkDiscouraged_goesToPlayServices() {

        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.DISCOURAGED;

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);

        assertThat(mFido2ApiCallHelper.mMakeCredentialCalled).isTrue();
        assertThat(mFido2ApiCallHelper.getChannelExtraOrNull()).isEqualTo(TEST_CHANNEL_EXTRA);
        assertThat(mFido2ApiCallHelper.getIncognitoExtraOrNull()).isTrue();
        verify(mCredManHelperMock, times(0))
                .startMakeRequest(any(), any(), any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testMakeCredential_paymentsEnabled_goesToPlayServices() {

        mCreationOptions.isPaymentCredentialCreation = true;

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);

        assertThat(mFido2ApiCallHelper.mMakeCredentialCalled).isTrue();
        verifyNoInteractions(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void testMakeCredential_webauthnModeAppAndBelowAndroid14_goesToPlayServices() {
        Mockito.when(mModeProviderMock.getWebauthnMode(any())).thenReturn(WebauthnMode.APP);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode()).thenReturn(WebauthnMode.NONE);
        CredManSupportProvider.setupForTesting(/* override= */ false);

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);

        assertThat(mFido2ApiCallHelper.mMakeCredentialCalled).isTrue();
        verifyNoInteractions(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void testMakeCredential_webauthnModeAppAndAboveAndroid14_goesToCredMan() {
        Mockito.when(mModeProviderMock.getWebauthnMode(any())).thenReturn(WebauthnMode.APP);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode()).thenReturn(WebauthnMode.NONE);
        CredManSupportProvider.setupForTesting(/* override= */ true);

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);

        assertThat(mFido2ApiCallHelper.mMakeCredentialCalled).isFalse();
        verify(mCredManHelperMock)
                .startMakeRequest(
                        any(),
                        any(),
                        /* clientDataJson= */ eq(null),
                        /* clientDataHash= */ eq(null),
                        any(),
                        any());
    }

    @Test
    @SmallTest
    public void testGetAssertion_prfRequestedOverHybrid_goesToPlayServices() {

        mCreationOptions.prfEnable = true;
        Mockito.when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(null);

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ new byte[] {0},
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);

        assertThat(mFido2ApiCallHelper.mMakeCredentialCalled).isTrue();
        verifyNoInteractions(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabledWithGpmInCredManFlag_success() {

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        String originString = Fido2CredentialRequest.convertOriginToString(mOrigin);
        verify(mCredManHelperMock)
                .startGetRequest(
                        eq(mRequestOptions),
                        eq(originString),
                        eq(TEST_CLIENT_DATA_JSON.getBytes()),
                        /* clientDataHash= */ notNull(),
                        /* getCallback= */ any(),
                        /* errorCallback= */ any(),
                        /* ignoreGpm= */ eq(false));
    }

    @Test
    @SmallTest
    public void
            testGetAssertion_credManEnabledWithGpmNotInCredManFlag_doesNotCallGetAssertionImmediately() {

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, true);
        testValues.addFieldTrialParamOverride(
                DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, "gpm_in_cred_man", "false");
        FeatureList.setTestValues(testValues);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        ArgumentCaptor<Runnable> fido2ApiCallSuccessfulRunback =
                ArgumentCaptor.forClass(Runnable.class);
        verify(mBarrierMock).onFido2ApiSuccessful(fido2ApiCallSuccessfulRunback.capture());
        fido2ApiCallSuccessfulRunback.getValue().run();

        String originString = Fido2CredentialRequest.convertOriginToString(mOrigin);
        verify(mCredManHelperMock)
                .startPrefetchRequest(
                        eq(mRequestOptions),
                        eq(originString),
                        eq(TEST_CLIENT_DATA_JSON.getBytes()),
                        /* clientDataHash= */ any(),
                        /* getCallback= */ any(),
                        /* errorCallback= */ any(),
                        /* barrier= */ any(),
                        /* ignoreGpm= */ eq(true));
        verify(mBrowserBridgeMock)
                .onCredentialsDetailsListReceived(
                        eq(mFrameHost), eq(Collections.emptyList()), eq(false), any(), any());
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isFalse();
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManDisabled_notUsed() {

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        FeatureList.setTestValues(testValues);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                (responseStatus, response) -> mCallback.onSignResponse(responseStatus, response),
                mCallback::onError);

        verifyNoInteractions(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManDisabled_stillUsedForHybrid() {

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        FeatureList.setTestValues(testValues);

        mRequest.setIsHybridRequest(true);
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        verify(mCredManHelperMock)
                .startGetRequest(
                        eq(mRequestOptions),
                        /* originString= */ any(),
                        eq(TEST_CLIENT_DATA_JSON.getBytes()),
                        /* clientDataHash= */ notNull(),
                        /* getCallback= */ any(),
                        /* errorCallback= */ any(),
                        /* ignoreGpm= */ eq(false));
    }

    @Test
    @SmallTest
    public void testGetAssertion_allowListMatchWithExplicitHash_goesToPlayServices() {

        mFido2ApiCallHelper.mCredentials = new ArrayList<>();
        mFido2ApiCallHelper.mCredentials.add(createWebauthnCredential());
        Mockito.when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(null);

        final byte[] clientDataHash = new byte[] {1, 2, 3, 4};
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                clientDataHash,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        verifyNoInteractions(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void testGetAssertion_prfInputsHashed_goesToPlayServices() {

        final byte[] clientDataHash = new byte[] {1, 2, 3, 4};
        mRequestOptions.extensions.prfInputsHashed = true;
        Mockito.when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(null);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                clientDataHash,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        verifyNoInteractions(mCredManHelperMock);
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
        assertThat(mFido2ApiCallHelper.mClientDataHash).isEqualTo(clientDataHash);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManNoCredentialsWithGpmInCredManFlag_fallbackToPlayServices() {

        mFido2ApiCallHelper.mCredentialsError = new IllegalStateException("injected error");
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        ArgumentCaptor<Runnable> setNoCredentialsParamCaptor =
                ArgumentCaptor.forClass(Runnable.class);
        verify(mCredManHelperMock).setNoCredentialsFallback(setNoCredentialsParamCaptor.capture());
        verify(mCredManHelperMock)
                .startGetRequest(
                        any(), any(), any(), any(), any(), any(), /* ignoreGpm= */ eq(false));

        // Now run the no credentials fallback action:
        setNoCredentialsParamCaptor.getValue().run();

        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testGetAssertion_allowListNoMatch_goesToCredMan() {

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {1, 2, 3, 4};
        descriptor.transports = new int[] {0};
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        verify(mCredManHelperMock)
                .startGetRequest(
                        any(), any(), any(), any(), any(), any(), /* ignoreGpm= */ eq(false));
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isFalse();
    }

    @Test
    @SmallTest
    public void testGetAssertion_allowListEnumerationFails_goesToCredMan() {

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {1, 2, 3, 4};
        descriptor.transports = new int[] {0};
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        mFido2ApiCallHelper.mCredentialsError = new IllegalStateException("injected error");

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        verify(mCredManHelperMock)
                .startGetRequest(
                        any(), any(), any(), any(), any(), any(), /* ignoreGpm= */ eq(false));
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isFalse();
    }

    @Test
    @SmallTest
    public void testGetAssertion_allowListMatch_goesToPlayServices() {

        mFido2ApiCallHelper.mCredentials = new ArrayList<>();
        mFido2ApiCallHelper.mCredentials.add(createWebauthnCredential());

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                (responseStatus, response) -> mCallback.onSignResponse(responseStatus, response),
                mCallback::onError);

        verifyNoInteractions(mCredManHelperMock);
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
    }

    @Test
    @SmallTest
    public void testGetAssertion_allowListNoMatchWhenGpmNotInCredMan_goesToCredMan() {

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, true);
        testValues.addFieldTrialParamOverride(
                DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, "gpm_in_cred_man", "false");
        FeatureList.setTestValues(testValues);

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {1, 2, 3, 4};
        descriptor.transports = new int[] {0};
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                (responseStatus, response) -> mCallback.onSignResponse(responseStatus, response),
                mCallback::onError);

        verify(mCredManHelperMock)
                .startGetRequest(
                        any(), any(), any(), any(), any(), any(), /* ignoreGpm= */ eq(true));
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isFalse();
    }

    @Test
    @SmallTest
    public void testGetAssertion_WebAuthnModeApp_GoesToPlayServices() {
        CredManSupportProvider.setupForTesting(/* override= */ false);

        Mockito.when(mModeProviderMock.getWebauthnMode(any())).thenReturn(WebauthnMode.APP);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode()).thenReturn(WebauthnMode.NONE);
        final byte[] clientDataHash = new byte[] {1, 2, 3, 4};
        Mockito.when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(null);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                clientDataHash,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        verifyNoInteractions(mCredManHelperMock);
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
        assertThat(mFido2ApiCallHelper.mClientDataHash).isEqualTo(clientDataHash);
    }

    @Test
    @SmallTest
    public void testGetAssertion_WebAuthnModeApp_failsIfGmscoreNotAvailable() {
        CredManSupportProvider.setupForTesting(/* override= */ false);

        Mockito.when(mModeProviderMock.getWebauthnMode(any())).thenReturn(WebauthnMode.APP);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode()).thenReturn(WebauthnMode.NONE);
        final byte[] clientDataHash = new byte[] {1, 2, 3, 4};
        mFido2ApiCallHelper.setArePlayServicesAvailable(false);
        Mockito.when(mAuthenticationContextProviderMock.getRenderFrameHost()).thenReturn(null);
        Fido2CredentialRequest request =
                new Fido2CredentialRequest(mAuthenticationContextProviderMock);

        request.handleGetAssertionRequest(
                mRequestOptions,
                clientDataHash,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        verifyNoInteractions(mCredManHelperMock);
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isFalse();
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledSuccessWithGpmInCredManFlag_success() {
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        String originString = Fido2CredentialRequest.convertOriginToString(mOrigin);
        verify(mCredManHelperMock, times(1))
                .startPrefetchRequest(
                        eq(mRequestOptions),
                        eq(originString),
                        eq(TEST_CLIENT_DATA_JSON.getBytes()),
                        /* clientDataHash= */ notNull(),
                        /* getCallback= */ any(),
                        /* errorCallback= */ any(),
                        /* barrier= */ any(),
                        /* ignoreGpm= */ eq(false));
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledSuccessWithGpmNotInCredManFlag_success() {
        mRequestOptions.isConditional = true;

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, true);
        testValues.addFieldTrialParamOverride(
                DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, "gpm_in_cred_man", "false");
        FeatureList.setTestValues(testValues);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        ArgumentCaptor<Runnable> fido2ApiCallSuccessfulRunback =
                ArgumentCaptor.forClass(Runnable.class);
        verify(mBarrierMock).onFido2ApiSuccessful(fido2ApiCallSuccessfulRunback.capture());
        fido2ApiCallSuccessfulRunback.getValue().run();

        String originString = Fido2CredentialRequest.convertOriginToString(mOrigin);
        verify(mCredManHelperMock, times(1))
                .startPrefetchRequest(
                        eq(mRequestOptions),
                        eq(originString),
                        eq(TEST_CLIENT_DATA_JSON.getBytes()),
                        /* clientDataHash= */ notNull(),
                        /* getCallback= */ any(),
                        /* errorCallback= */ any(),
                        /* barrier= */ any(),
                        /* ignoreGpm= */ eq(true));
        verify(mBrowserBridgeMock, times(1))
                .onCredentialsDetailsListReceived(any(), any(), eq(true), any(), any());
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_webauthnModeNotChrome_notImplemented() {
        mRequestOptions.isConditional = true;
        Mockito.when(mModeProviderMock.getWebauthnMode(any())).thenReturn(WebauthnMode.APP);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode()).thenReturn(WebauthnMode.NONE);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        assertThat(mCallback.getStatus()).isEqualTo(AuthenticatorStatus.NOT_IMPLEMENTED);
        verifyNoInteractions(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void
            testConditionalGetAssertion_credManEnabledRpCancelWhileIdleWithGpmInCredManFlag_notAllowedError() {
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        mRequest.cancelConditionalGetAssertion();

        // CredManHelper class is responsible to return the status.
        assertThat(mCallback.getStatus()).isEqualTo(null);
        verify(mCredManHelperMock).cancelConditionalGetAssertion();
        verify(mBrowserBridgeMock, never()).cleanupRequest(any());
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void
            testConditionalGetAssertion_credManEnabledRpCancelWhileIdleWithGpmNotInCredManFlag_notAllowedError() {
        mRequestOptions.isConditional = true;

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, true);
        testValues.addFieldTrialParamOverride(
                DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, "gpm_in_cred_man", "false");
        FeatureList.setTestValues(testValues);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        ArgumentCaptor<Runnable> fido2ApiCallSuccessfulRunback =
                ArgumentCaptor.forClass(Runnable.class);
        verify(mBarrierMock).onFido2ApiSuccessful(fido2ApiCallSuccessfulRunback.capture());
        fido2ApiCallSuccessfulRunback.getValue().run();

        mRequest.cancelConditionalGetAssertion();

        verify(mBarrierMock).onFido2ApiCancelled();
        verify(mCredManHelperMock).cancelConditionalGetAssertion();
        verify(mBrowserBridgeMock).cleanupRequest(any());
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_abortedWhileWaitingForRpIdValidation_aborted() {

        // Capture the RP ID validation callback and let the request sit
        // waiting for it.
        var rpIdValidationCallback = new Callback[1];
        Mockito.doAnswer(
                        (invocation) -> {
                            rpIdValidationCallback[0] = (Callback) invocation.getArguments()[3];
                            return null;
                        })
                .when(mFrameHost)
                .performGetAssertionWebAuthSecurityChecks(
                        any(String.class), any(Origin.class), anyBoolean(), any(Callback.class));

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        // The request should have requested RP ID validation.
        assertThat(rpIdValidationCallback[0]).isNotNull();
        // Aborting the request shouldn't do anything yet because it's waiting
        // for RP ID validation.
        mRequest.cancelConditionalGetAssertion();
        assertThat(mCallback.getStatus()).isEqualTo(null);
        // When the RP ID validation completes, the overall request should then
        // be canceled. Any RP ID validation error should be ignored in favour
        // of `ABORT_ERROR`.
        rpIdValidationCallback[0].onResult(
                new WebAuthSecurityChecksResults(AuthenticatorStatus.NOT_ALLOWED_ERROR, false));
        assertThat(mCallback.getStatus()).isEqualTo(AuthenticatorStatus.ABORT_ERROR);
    }

    @Test
    @SmallTest
    public void
            testConditionalGetAssertion_webauthnModeChrome3ppAndCredManDisabled_notImplemented() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        FeatureList.setTestValues(testValues);

        mRequestOptions.isConditional = true;
        Mockito.when(mModeProviderMock.getWebauthnMode(any()))
                .thenReturn(WebauthnMode.CHROME_3PP_ENABLED);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode())
                .thenReturn(WebauthnMode.CHROME_3PP_ENABLED);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        assertThat(mCallback.getStatus()).isEqualTo(AuthenticatorStatus.NOT_IMPLEMENTED);
        verifyNoInteractions(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void testGetAssertion_webauthnModeChrome3ppAndCredManDisabled_goesToPlayServices() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        FeatureList.setTestValues(testValues);

        Mockito.when(mModeProviderMock.getWebauthnMode(any()))
                .thenReturn(WebauthnMode.CHROME_3PP_ENABLED);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode())
                .thenReturn(WebauthnMode.CHROME_3PP_ENABLED);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        verifyNoInteractions(mCredManHelperMock);
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_webauthnModeChrome3ppAndCredManEnabled_goesToCredMan() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, true);
        FeatureList.setTestValues(testValues);

        mRequestOptions.isConditional = true;
        Mockito.when(mModeProviderMock.getWebauthnMode(any()))
                .thenReturn(WebauthnMode.CHROME_3PP_ENABLED);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode())
                .thenReturn(WebauthnMode.CHROME_3PP_ENABLED);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        assertThat(mCallback.getStatus()).isNull();
        verify(mCredManHelperMock, times(1))
                .startPrefetchRequest(
                        any(), any(), any(), any(), any(), any(), any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testGetAssertion_webauthnModeChrome3ppAndCredManEnabled_goesToCredMan() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, true);
        FeatureList.setTestValues(testValues);

        Mockito.when(mModeProviderMock.getWebauthnMode(any()))
                .thenReturn(WebauthnMode.CHROME_3PP_ENABLED);
        Mockito.when(mModeProviderMock.getGlobalWebauthnMode())
                .thenReturn(WebauthnMode.CHROME_3PP_ENABLED);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);

        verify(mCredManHelperMock, times(1))
                .startGetRequest(any(), any(), any(), any(), any(), any(), anyBoolean());
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isFalse();
    }

    private WebauthnCredentialDetails createWebauthnCredential() {
        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {1, 2, 3, 4};
        descriptor.transports = new int[] {0};
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        WebauthnCredentialDetails details = new WebauthnCredentialDetails();
        details.mCredentialId = descriptor.id;
        return details;
    }

    static class FakeFido2ApiCallHelper extends Fido2ApiCallHelper {
        public boolean mMakeCredentialCalled;
        public boolean mGetAssertionCalled;
        public List<WebauthnCredentialDetails> mCredentials;
        public Exception mCredentialsError;
        public byte[] mClientDataHash;
        public Bundle mBrowserOptions;

        private boolean mArePlayServicesAvailable = true;

        @Override
        public boolean arePlayServicesAvailable() {
            return mArePlayServicesAvailable;
        }

        public void setArePlayServicesAvailable(boolean arePlayServicesAvailable) {
            mArePlayServicesAvailable = arePlayServicesAvailable;
        }

        String getChannelExtraOrNull() {
            return mBrowserOptions == null
                    ? null
                    : mBrowserOptions.getString("com.android.chrome.CHANNEL");
        }

        Boolean getIncognitoExtraOrNull() {
            return mBrowserOptions == null
                    ? null
                    : mBrowserOptions.getBoolean("com.android.chrome.INCOGNITO");
        }

        @Override
        public void invokeFido2GetCredentials(
                AuthenticationContextProvider authenticationContextProvider,
                String relyingPartyId,
                OnSuccessListener<List<WebauthnCredentialDetails>> successCallback,
                OnFailureListener failureCallback) {
            if (mCredentialsError != null) {
                failureCallback.onFailure(mCredentialsError);
                return;
            }

            List<WebauthnCredentialDetails> credentials;
            if (mCredentials == null) {
                credentials = new ArrayList();
            } else {
                credentials = mCredentials;
                mCredentials = null;
            }

            successCallback.onSuccess(credentials);
        }

        @Override
        public void invokeFido2MakeCredential(
                AuthenticationContextProvider authenticationContextProvider,
                PublicKeyCredentialCreationOptions options,
                Uri uri,
                byte[] clientDataHash,
                Bundle browserOptions,
                ResultReceiver resultReceiver,
                OnSuccessListener<PendingIntent> successCallback,
                OnFailureListener failureCallback)
                throws NoSuchAlgorithmException {
            mMakeCredentialCalled = true;
            mClientDataHash = clientDataHash;
            mBrowserOptions = browserOptions;

            if (mCredentialsError != null) {
                failureCallback.onFailure(mCredentialsError);
                return;
            }
            // Don't make any actual calls to Play Services.
        }

        @Override
        public void invokeFido2GetAssertion(
                AuthenticationContextProvider authenticationContextProvider,
                PublicKeyCredentialRequestOptions options,
                Uri uri,
                byte[] clientDataHash,
                ResultReceiver resultReceiver,
                OnSuccessListener<PendingIntent> successCallback,
                OnFailureListener failureCallback) {
            mGetAssertionCalled = true;
            mClientDataHash = clientDataHash;

            if (mCredentialsError != null) {
                failureCallback.onFailure(mCredentialsError);
                return;
            }
            // Don't make any actual calls to Play Services.
        }
    }
}
