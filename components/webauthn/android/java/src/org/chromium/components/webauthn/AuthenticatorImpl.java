// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.webauthn.WebauthnLogger.log;
import static org.chromium.components.webauthn.WebauthnLogger.updateLogState;
import static org.chromium.components.webauthn.WebauthnModeProvider.isChrome;

import android.annotation.SuppressLint;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.CredentialInfo;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.GetAssertionResponse;
import org.chromium.blink.mojom.GetCredentialOptions;
import org.chromium.blink.mojom.GetCredentialResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.Mediation;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialReportOptions;
import org.chromium.blink.mojom.WebAuthnClientCapability;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.DeviceFeatureList;
import org.chromium.device.DeviceFeatureMap;
import org.chromium.mojo.system.MojoException;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

/** Android implementation of the authenticator.mojom interface. */
@NullMarked
public final class AuthenticatorImpl implements Authenticator, AuthenticationContextProvider {
    private static final String TAG = "AuthenticatorImpl";

    private final @Nullable Context mContext;
    private final @Nullable WebContents mWebContents;
    private final @Nullable FidoIntentSender mIntentSender;
    private final @Nullable RenderFrameHost mRenderFrameHost;
    private final @Nullable CreateConfirmationUiDelegate mCreateConfirmationUiDelegate;

    /** Ensures only one request is processed at a time. */
    private boolean mIsOperationPending;

    /**
     * The origin of the request. This may be overridden by an internal request from the browser
     * process. <code>mOrigin</code> will be set when a RenderFrameHost is provided at construction
     * and null otherwise.
     */
    private @Nullable Origin mOrigin;

    /** The origin of the main frame. */
    private final @Nullable Origin mTopOrigin;

    /** The payment information to be added to the "clientDataJson". */
    private @Nullable PaymentOptions mPayment;

    private @Nullable MakeCredential_Response mMakeCredentialCallback;
    private @Nullable GetCredential_Response mGetCredentialCallback;
    private @Nullable Report_Response mReportCallback;
    private @Nullable Fido2CredentialRequest mPendingFido2CredentialRequest;
    private final Set<Fido2CredentialRequest> mUnclosedFido2CredentialRequests = new HashSet<>();

    // Information about the request cached here for metric reporting purposes.
    private boolean mIsConditionalRequest;
    private boolean mIsPaymentRequest;
    private boolean mIsImmediateRequest;

    // StaticFieldLeak complains that this is a memory leak because
    // `Fido2CredentialRequest` contains a `Context`. But this field is only
    // used in tests so a memory leak is irrelevent.
    @SuppressLint("StaticFieldLeak")
    private static @Nullable Fido2CredentialRequest sFido2CredentialRequestOverrideForTesting;

    /**
     * Builds the Authenticator service implementation.
     *
     * @param context The context of the AndroidWindow that triggered this operation.
     * @param intentSender The interface that will be used to start {@link Intent}s from Play
     *     Services. May only be null for calls that do not go to Play Services such as {@link
     *     #getMatchingCredentialIds()}.
     * @param createConfirmationUiDelegate If not null, is an object that will be called before
     *     creating a credential to show a confirmation UI.
     * @param renderFrameHost The host of the frame that has invoked the API. Null if created
     *     unrelated to a renderer context, and when renderFrameHost is null {@link
     *     #makeCredential()} and {@link #getCredential()} will fail, do not call them.
     * @param topOrigin The origin of the main frame.
     */
    public AuthenticatorImpl(
            @Nullable Context context,
            @Nullable WebContents webContents,
            @Nullable FidoIntentSender intentSender,
            @Nullable CreateConfirmationUiDelegate createConfirmationUiDelegate,
            @Nullable RenderFrameHost renderFrameHost,
            @Nullable Origin topOrigin) {
        assert WebauthnModeProvider.getInstance().getWebauthnMode(webContents) != WebauthnMode.NONE;
        updateLogState();
        log(TAG, "AuthenticatorImpl created");

        mContext = context;
        mWebContents = webContents;
        mIntentSender = intentSender;
        mRenderFrameHost = renderFrameHost;
        mOrigin = mRenderFrameHost == null ? null : mRenderFrameHost.getLastCommittedOrigin();
        mTopOrigin = topOrigin;
        mCreateConfirmationUiDelegate = createConfirmationUiDelegate;
    }

    public static void overrideFido2CredentialRequestForTesting(Fido2CredentialRequest request) {
        sFido2CredentialRequestOverrideForTesting = request;
    }

    private Fido2CredentialRequest getFido2CredentialRequest() {
        if (sFido2CredentialRequestOverrideForTesting != null) {
            return sFido2CredentialRequestOverrideForTesting;
        }
        Fido2CredentialRequest request = new Fido2CredentialRequest(this);
        mUnclosedFido2CredentialRequests.add(request);
        return request;
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process. Since the request is from the browser process, the
     * Relying Party ID may not correspond with the origin of the renderer.
     */
    public void setEffectiveOrigin(Origin origin) {
        mOrigin = origin;
    }

    /**
     * @param payment The payment information to be added to the "clientDataJson". Should be used
     *     only if the user has confirmed the payment information that was displayed to the user.
     */
    public void setPaymentOptions(PaymentOptions payment) {
        mPayment = payment;
    }

    @Override
    public void makeCredential(
            PublicKeyCredentialCreationOptions options, MakeCredential_Response callback) {
        assert mIntentSender != null;
        assert mRenderFrameHost != null;
        if (mIsOperationPending) {
            callback.call(AuthenticatorStatus.PENDING_REQUEST, null, null);
            return;
        }

        log(TAG, "makeCredential");

        mIsPaymentRequest = options.isPaymentCredentialCreation;
        mMakeCredentialCallback = callback;
        mIsOperationPending = true;
        if (!GmsCoreUtils.isWebauthnSupported()
                || (!isChrome(mWebContents) && !GmsCoreUtils.isResultReceiverSupported())) {
            recordOutcomeEvent(MakeCredentialOutcome.OTHER_FAILURE);
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        boolean isConditionalCreate =
                options.isConditional
                        && DeviceFeatureMap.isEnabled(DeviceFeatureList.WEBAUTHN_PASSKEY_UPGRADE);

        if (mCreateConfirmationUiDelegate != null && !isConditionalCreate) {
            if (!mCreateConfirmationUiDelegate.show(
                    () -> continueMakeCredential(options),
                    () -> {
                        recordOutcomeEvent(MakeCredentialOutcome.USER_CANCELLATION);
                        onError(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                    })) {
                continueMakeCredential(options);
            }
        } else {
            continueMakeCredential(options);
        }
    }

    private void continueMakeCredential(PublicKeyCredentialCreationOptions options) {
        log(TAG, "continueMakeCredential");
        mPendingFido2CredentialRequest = getFido2CredentialRequest();
        mPendingFido2CredentialRequest.handleMakeCredentialRequest(
                options,
                maybeCreateBrowserOptions(),
                assertNonNull(mOrigin),
                mTopOrigin,
                mPayment,
                this::onRegisterResponse,
                this::onError,
                this::recordOutcomeEvent);
    }

    private @Nullable Bundle maybeCreateBrowserOptions() {
        if (!isChrome(mWebContents)) {
            return null;
        }
        Bundle browserOptions = GpmBrowserOptionsHelper.createDefaultBrowserOptions();
        GpmBrowserOptionsHelper.addIncognitoExtraToOptions(browserOptions, mRenderFrameHost);
        return browserOptions;
    }

    @Override
    public void getCredential(GetCredentialOptions options, GetCredential_Response callback) {
        assert mIntentSender != null;
        assert mRenderFrameHost != null;
        if (mIsOperationPending) {
            callback.call(
                    getCredentialResponseForAssertion(AuthenticatorStatus.PENDING_REQUEST, null));
            return;
        }
        log(TAG, "getCredential");

        mGetCredentialCallback = callback;
        mIsOperationPending = true;
        mIsPaymentRequest = mPayment != null;
        mIsConditionalRequest = options.mediation == Mediation.CONDITIONAL;
        mIsImmediateRequest = options.mediation == Mediation.IMMEDIATE;

        if (!GmsCoreUtils.isWebauthnSupported()
                || (!isChrome(mWebContents) && !GmsCoreUtils.isResultReceiverSupported())
                || options.publicKey == null) {
            recordOutcomeEvent(GetAssertionOutcome.OTHER_FAILURE);
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }
        assumeNonNull(options.publicKey);

        mPendingFido2CredentialRequest = getFido2CredentialRequest();
        mPendingFido2CredentialRequest.handleGetCredentialRequest(
                options,
                assertNonNull(mOrigin),
                mTopOrigin,
                mPayment,
                this::onCredentialResponse,
                this::onError,
                this::recordOutcomeEvent);
    }

    @Override
    public void report(PublicKeyCredentialReportOptions options, Report_Response callback) {
        log(TAG, "report");
        if (!DeviceFeatureMap.isEnabled(DeviceFeatureList.WEBAUTHN_ANDROID_SIGNAL)) {
            callback.call(AuthenticatorStatus.NOT_IMPLEMENTED, null);
            return;
        }

        if (mIsOperationPending) {
            callback.call(AuthenticatorStatus.PENDING_REQUEST, null);
            return;
        }

        mReportCallback = callback;
        mIsOperationPending = true;

        mPendingFido2CredentialRequest = getFido2CredentialRequest();
        mPendingFido2CredentialRequest.handleReportRequest(
                options, assertNonNull(mOrigin), this::onReportComplete);
    }

    private boolean couldSupportConditionalMediation() {
        return GmsCoreUtils.isWebauthnSupported() && isChrome(mWebContents);
    }

    private boolean couldSupportUvpaa() {
        return GmsCoreUtils.isWebauthnSupported()
                && (isChrome(mWebContents) || GmsCoreUtils.isResultReceiverSupported());
    }

    @Override
    public void isUserVerifyingPlatformAuthenticatorAvailable(
            final IsUserVerifyingPlatformAuthenticatorAvailable_Response callback) {
        log(TAG, "isUserVerifyingPlatformAuthenticatorAvailable");
        IsUserVerifyingPlatformAuthenticatorAvailable_Response decoratedCallback =
                (isUvpaa) -> {
                    RecordHistogram.recordBooleanHistogram(
                            "WebAuthentication.IsUVPlatformAuthenticatorAvailable2", isUvpaa);
                    callback.call(isUvpaa);
                };

        if (!couldSupportUvpaa()) {
            decoratedCallback.call(false);
            return;
        }

        getFido2CredentialRequest()
                .handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
                        isUvpaa -> decoratedCallback.call(isUvpaa));
    }

    @Override
    public void getClientCapabilities(final GetClientCapabilities_Response callback) {
        log(TAG, "getClientCapabilities");
        ArrayList<WebAuthnClientCapability> capabilities = new ArrayList<>();
        capabilities.add(
                createWebAuthnClientCapability(
                        AuthenticatorConstants.CAPABILITY_RELATED_ORIGINS, true));
        // This assumes GMSCore is available and up-to-date, hence this should report "true". This
        // assumption should be revisited if it proves insufficient.
        capabilities.add(
                createWebAuthnClientCapability(
                        AuthenticatorConstants.CAPABILITY_HYBRID_TRANSPORT, true));
        // passkeyPlatformAuthenticator is supported if (IsUVPAA OR hybridTransport) is supported.
        // Since we assume that hybridTransport is always true on Android,
        // passkeyPlatformAuthenticator is also always true.
        capabilities.add(
                createWebAuthnClientCapability(AuthenticatorConstants.CAPABILITY_PPAA, true));

        if (!couldSupportUvpaa()) {
            capabilities.add(
                    createWebAuthnClientCapability(
                            AuthenticatorConstants.CAPABILITY_CONDITIONAL_GET, false));
            capabilities.add(
                    createWebAuthnClientCapability(
                            AuthenticatorConstants.CAPABILITY_CONDITIONAL_CREATE, false));
            capabilities.add(
                    createWebAuthnClientCapability(AuthenticatorConstants.CAPABILITY_UVPAA, false));
            capabilities.add(
                    createWebAuthnClientCapability(
                            AuthenticatorConstants.CAPABILITY_IMMEDIATE_GET, false));
            capabilities.add(
                    createWebAuthnClientCapability(
                            AuthenticatorConstants.CAPABILITY_SIGNAL_ALL_ACCEPTED_CREDENTIALS,
                            false));
            capabilities.add(
                    createWebAuthnClientCapability(
                            AuthenticatorConstants.CAPABILITY_SIGNAL_CURRENT_USER_DETAILS, false));
            capabilities.add(
                    createWebAuthnClientCapability(
                            AuthenticatorConstants.CAPABILITY_SIGNAL_UNKNOWN_CREDENTIAL, false));
            callback.call(capabilities.toArray(new WebAuthnClientCapability[0]));
            return;
        }

        getFido2CredentialRequest()
                .handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
                        isUvpaa -> {
                            capabilities.add(
                                    createWebAuthnClientCapability(
                                            AuthenticatorConstants.CAPABILITY_CONDITIONAL_GET,
                                            couldSupportConditionalMediation() && isUvpaa));
                            capabilities.add(
                                    createWebAuthnClientCapability(
                                            AuthenticatorConstants.CAPABILITY_UVPAA,
                                            couldSupportUvpaa() && isUvpaa));
                            boolean conditionalCreateEnabled =
                                    couldSupportConditionalMediation()
                                            && DeviceFeatureMap.isEnabled(
                                                    DeviceFeatureList.WEBAUTHN_PASSKEY_UPGRADE);
                            capabilities.add(
                                    createWebAuthnClientCapability(
                                            AuthenticatorConstants.CAPABILITY_CONDITIONAL_CREATE,
                                            isUvpaa && conditionalCreateEnabled));
                            capabilities.add(
                                    createWebAuthnClientCapability(
                                            AuthenticatorConstants.CAPABILITY_IMMEDIATE_GET,
                                            DeviceFeatureMap.isEnabled(
                                                            DeviceFeatureList
                                                                    .WEBAUTHN_IMMEDIATE_GET)
                                                    && isUvpaa));
                            boolean signalSupported =
                                    isUvpaa
                                            && DeviceFeatureMap.isEnabled(
                                                    DeviceFeatureList.WEBAUTHN_ANDROID_SIGNAL);
                            capabilities.add(
                                    createWebAuthnClientCapability(
                                            AuthenticatorConstants
                                                    .CAPABILITY_SIGNAL_ALL_ACCEPTED_CREDENTIALS,
                                            signalSupported));
                            capabilities.add(
                                    createWebAuthnClientCapability(
                                            AuthenticatorConstants
                                                    .CAPABILITY_SIGNAL_CURRENT_USER_DETAILS,
                                            signalSupported));
                            capabilities.add(
                                    createWebAuthnClientCapability(
                                            AuthenticatorConstants
                                                    .CAPABILITY_SIGNAL_UNKNOWN_CREDENTIAL,
                                            signalSupported));
                            callback.call(capabilities.toArray(new WebAuthnClientCapability[0]));
                        });
    }

    // Helper function to create WebAuthnClientCapability instances
    private WebAuthnClientCapability createWebAuthnClientCapability(
            String name, boolean supported) {
        WebAuthnClientCapability capability = new WebAuthnClientCapability();
        capability.name = name;
        capability.supported = supported;
        return capability;
    }

    /**
     * Retrieves the set of credentials for the given relying party, and filters them to match the
     * given input credential IDs. Optionally, may also filter the credentials to only return those
     * that are marked as third-party payment enabled.
     *
     * <p>Because this functionality does not participate in the normal WebAuthn UI flow and is
     * idempotent at the Fido2 layer, it does not adhere to the 'one call at a time' logic used for
     * the create/get methods.
     */
    public void getMatchingCredentialIds(
            String relyingPartyId,
            byte[][] credentialIds,
            boolean requireThirdPartyPayment,
            GetMatchingCredentialIdsResponseCallback callback) {
        log(TAG, "getMatchingCredentialIds");
        if (!GmsCoreUtils.isGetMatchingCredentialIdsSupported()) {
            callback.onResponse(new ArrayList<byte[]>());
            return;
        }

        getFido2CredentialRequest()
                .handleGetMatchingCredentialIdsRequest(
                        relyingPartyId,
                        credentialIds,
                        requireThirdPartyPayment,
                        callback,
                        this::onError);
    }

    @Override
    public void isConditionalMediationAvailable(
            final IsConditionalMediationAvailable_Response callback) {
        log(TAG, "isConditionalMediationAvailable");
        if (!couldSupportConditionalMediation()) {
            callback.call(false);
            return;
        }

        // If the gmscore and chromium versions are out of sync for some reason, this method will
        // return true but chrome will ignore conditional requests. Android surfaces only platform
        // credentials on conditional requests, use IsUVPAA as a proxy for availability.
        getFido2CredentialRequest()
                .handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
                        isUvpaa -> callback.call(isUvpaa));
    }

    @Override
    public void cancel() {
        log(TAG, "cancel");
        // This is not implemented for anything other than getAssertion requests, since there is
        // no way to cancel a request that has already triggered GMSCore or CredMan UI. Get
        // requests can be cancelled in situations such as while pending credential enumeration.
        if (!mIsOperationPending || mGetCredentialCallback == null) {
            return;
        }

        assumeNonNull(mPendingFido2CredentialRequest);
        mPendingFido2CredentialRequest.cancelGetAssertion();
    }

    /** Callbacks for receiving responses from the internal handlers. */
    public void onRegisterResponse(int status, MakeCredentialAuthenticatorResponse response) {
        log(TAG, "makeCredential completed with status: " + status);
        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending) return;

        assert mMakeCredentialCallback != null;
        assert status == AuthenticatorStatus.SUCCESS;
        mMakeCredentialCallback.call(AuthenticatorStatus.SUCCESS, response, null);
        cleanupRequest();
    }

    public void onCredentialResponse(
            @Nullable GetAssertionAuthenticatorResponse assertionResponse,
            @Nullable CredentialInfo passwordCredential) {
        log(TAG, "getCredential completed.");
        assert assertionResponse == null ^ passwordCredential == null;

        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending) return;

        assert mGetCredentialCallback != null;
        if (assertionResponse != null) {
            mGetCredentialCallback.call(
                    getCredentialResponseForAssertion(
                            AuthenticatorStatus.SUCCESS, assertionResponse));
        } else {
            assumeNonNull(passwordCredential);
            mGetCredentialCallback.call(getCredentialResponseForPassword(passwordCredential));
        }
        cleanupRequest();
    }

    public void onReportComplete(int status) {
        log(TAG, "onReportComplete completed with status: " + status);
        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending || mReportCallback == null) return;

        mReportCallback.call(status, null);
        cleanupRequest();
    }

    public void onError(Integer status) {
        log(TAG, "Request completed with error: " + status);
        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending) return;

        assert ((mMakeCredentialCallback != null && mGetCredentialCallback == null)
                || (mMakeCredentialCallback == null && mGetCredentialCallback != null));
        assert status != AuthenticatorStatus.ERROR_WITH_DOM_EXCEPTION_DETAILS;
        if (mMakeCredentialCallback != null) {
            mMakeCredentialCallback.call(status, null, null);
        } else if (mGetCredentialCallback != null) {
            mGetCredentialCallback.call(getCredentialResponseForAssertion(status, null));
        }
        if (mPendingFido2CredentialRequest != null) mPendingFido2CredentialRequest.destroyBridge();
        cleanupRequest();
    }

    /** Record outcome UKM at the request's completion time. */
    private void recordOutcomeEvent(int resultMetricValue) {
        // mWebContents can be null in tests.
        if (mWebContents == null || !isChrome(mWebContents)) {
            return;
        }
        String event;
        String resultMetricName;
        if (mGetCredentialCallback != null) {
            event = "WebAuthn.SignCompletion";
            resultMetricName = "SignCompletionResult";
        } else if (mMakeCredentialCallback != null) {
            event = "WebAuthn.RegisterCompletion";
            resultMetricName = "RegisterCompletionResult";
        } else {
            return;
        }

        @AuthenticationRequestMode int mode = AuthenticationRequestMode.MODAL_WEB_AUTHN;
        if (mIsConditionalRequest) {
            mode = AuthenticationRequestMode.CONDITIONAL;
        } else if (mIsPaymentRequest) {
            mode = AuthenticationRequestMode.PAYMENT;
        } else if (mIsImmediateRequest) {
            mode = AuthenticationRequestMode.IMMEDIATE;
        }
        new UkmRecorder(mWebContents, event)
                .addMetric(resultMetricName, resultMetricValue)
                .addMetric("RequestMode", mode)
                .record();
    }

    private void cleanupRequest() {
        mIsOperationPending = false;
        mMakeCredentialCallback = null;
        mGetCredentialCallback = null;
        mPendingFido2CredentialRequest = null;
    }

    @Override
    public void close() {
        log(TAG, "close");
        mUnclosedFido2CredentialRequests.forEach(Fido2CredentialRequest::destroyBridge);
        mUnclosedFido2CredentialRequests.clear();
        cleanupRequest();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    @Override
    public @Nullable Context getContext() {
        return mContext;
    }

    @Override
    public @Nullable RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    @Override
    public FidoIntentSender getIntentSender() {
        assert mIntentSender != null;
        return mIntentSender;
    }

    @Override
    public @Nullable WebContents getWebContents() {
        return mWebContents;
    }

    /** Implements {@link IntentSender} using a {@link WindowAndroid}. */
    public static class WindowIntentSender implements FidoIntentSender {
        private final @Nullable WindowAndroid mWindow;

        WindowIntentSender(@Nullable WindowAndroid window) {
            mWindow = window;
        }

        @Override
        public boolean showIntent(
                PendingIntent intent, Callback<Pair<Integer, @Nullable Intent>> callback) {
            return mWindow != null
                    && mWindow.getActivity().get() != null
                    && mWindow.showCancelableIntent(intent, new CallbackWrapper(callback), null)
                            != WindowAndroid.START_INTENT_FAILURE;
        }

        private static class CallbackWrapper implements WindowAndroid.IntentCallback {
            private final Callback<Pair<Integer, @Nullable Intent>> mCallback;

            CallbackWrapper(Callback<Pair<Integer, @Nullable Intent>> callback) {
                mCallback = callback;
            }

            @Override
            public void onIntentCompleted(int resultCode, @Nullable Intent data) {
                mCallback.onResult(new Pair(resultCode, data));
            }
        }
    }

    private GetCredentialResponse getCredentialResponseForAssertion(
            int status, @Nullable GetAssertionAuthenticatorResponse response) {
        GetCredentialResponse finalResponse = new GetCredentialResponse();
        GetAssertionResponse assertionResponse = new GetAssertionResponse();
        assertionResponse.credential = response;
        assertionResponse.status = status;
        finalResponse.setGetAssertionResponse(assertionResponse);
        return finalResponse;
    }

    private GetCredentialResponse getCredentialResponseForPassword(
            CredentialInfo passwordCredential) {
        GetCredentialResponse response = new GetCredentialResponse();
        response.setPasswordResponse(passwordCredential);
        return response;
    }
}
