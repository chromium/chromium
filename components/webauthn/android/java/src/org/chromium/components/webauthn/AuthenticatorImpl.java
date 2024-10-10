// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.components.webauthn.WebauthnModeProvider.isChrome;

import android.annotation.SuppressLint;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialReportOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

/** Android implementation of the authenticator.mojom interface. */
public final class AuthenticatorImpl implements Authenticator, AuthenticationContextProvider {
    private final Context mContext;
    private final WebContents mWebContents;
    private final FidoIntentSender mIntentSender;
    private final RenderFrameHost mRenderFrameHost;
    private final CreateConfirmationUiDelegate mCreateConfirmationUiDelegate;

    /** Ensures only one request is processed at a time. */
    private boolean mIsOperationPending;

    /**
     * The origin of the request. This may be overridden by an internal request from the browser
     * process.
     */
    private Origin mOrigin;

    /** The origin of the main frame. */
    private Origin mTopOrigin;

    /** The payment information to be added to the "clientDataJson". */
    private PaymentOptions mPayment;

    private MakeCredential_Response mMakeCredentialCallback;
    private GetAssertion_Response mGetAssertionCallback;
    private Fido2CredentialRequest mPendingFido2CredentialRequest;
    private Set<Fido2CredentialRequest> mUnclosedFido2CredentialRequests = new HashSet<>();

    // StaticFieldLeak complains that this is a memory leak because
    // `Fido2CredentialRequest` contains a `Context`. But this field is only
    // used in tests so a memory leak is irrelevent.
    @SuppressLint("StaticFieldLeak")
    private static Fido2CredentialRequest sFido2CredentialRequestOverrideForTesting;

    /**
     * Builds the Authenticator service implementation.
     *
     * @param context The context of the AndroidWindow that triggered this operation.
     * @param intentSender The interface that will be used to start {@link Intent}s from Play
     *     Services.
     * @param createConfirmationUiDelegate If not null, is an object that will be called before
     *     creating a credential to show a confirmation UI.
     * @param renderFrameHost The host of the frame that has invoked the API.
     * @param topOrigin The origin of the main frame.
     */
    public AuthenticatorImpl(
            Context context,
            WebContents webContents,
            FidoIntentSender intentSender,
            @Nullable CreateConfirmationUiDelegate createConfirmationUiDelegate,
            RenderFrameHost renderFrameHost,
            Origin topOrigin) {
        assert renderFrameHost != null;
        assert WebauthnModeProvider.getInstance().getWebauthnMode(webContents) != WebauthnMode.NONE;

        mContext = context;
        mWebContents = webContents;
        mIntentSender = intentSender;
        mRenderFrameHost = renderFrameHost;
        mOrigin = mRenderFrameHost.getLastCommittedOrigin();
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
        if (mIsOperationPending) {
            callback.call(AuthenticatorStatus.PENDING_REQUEST, null, null);
            return;
        }

        mMakeCredentialCallback = callback;
        mIsOperationPending = true;
        if (!GmsCoreUtils.isWebauthnSupported()
                || (!isChrome(mWebContents) && !GmsCoreUtils.isResultReceiverSupported())) {
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        if (mCreateConfirmationUiDelegate != null) {
            if (!mCreateConfirmationUiDelegate.show(
                    () -> continueMakeCredential(options),
                    () -> onError(AuthenticatorStatus.NOT_ALLOWED_ERROR))) {
                continueMakeCredential(options);
            }
        } else {
            continueMakeCredential(options);
        }
    }

    private void continueMakeCredential(PublicKeyCredentialCreationOptions options) {
        mPendingFido2CredentialRequest = getFido2CredentialRequest();
        mPendingFido2CredentialRequest.handleMakeCredentialRequest(
                options,
                /* maybeClientDataHash= */ null,
                maybeCreateBrowserOptions(),
                mOrigin,
                mTopOrigin,
                this::onRegisterResponse,
                this::onError);
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
    public void getAssertion(
            PublicKeyCredentialRequestOptions options, GetAssertion_Response callback) {
        if (mIsOperationPending) {
            callback.call(AuthenticatorStatus.PENDING_REQUEST, null, null);
            return;
        }

        mGetAssertionCallback = callback;
        mIsOperationPending = true;

        if (!GmsCoreUtils.isWebauthnSupported()
                || (!isChrome(mWebContents) && !GmsCoreUtils.isResultReceiverSupported())) {
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        mPendingFido2CredentialRequest = getFido2CredentialRequest();
        mPendingFido2CredentialRequest.handleGetAssertionRequest(
                options,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mTopOrigin,
                mPayment,
                this::onSignResponse,
                this::onError);
    }

    @Override
    public void report(PublicKeyCredentialReportOptions options, Report_Response callback) {
        callback.call(AuthenticatorStatus.NOT_IMPLEMENTED, null);
    }

    @Override
    public void isUserVerifyingPlatformAuthenticatorAvailable(
            final IsUserVerifyingPlatformAuthenticatorAvailable_Response callback) {
        IsUserVerifyingPlatformAuthenticatorAvailable_Response decoratedCallback =
                (isUvpaa) -> {
                    RecordHistogram.recordBooleanHistogram(
                            "WebAuthentication.IsUVPlatformAuthenticatorAvailable2", isUvpaa);
                    callback.call(isUvpaa);
                };

        if (!GmsCoreUtils.isWebauthnSupported()
                || (!isChrome(mWebContents) && !GmsCoreUtils.isResultReceiverSupported())) {
            decoratedCallback.call(false);
            return;
        }

        getFido2CredentialRequest()
                .handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
                        isUvpaa -> decoratedCallback.call(isUvpaa));
    }

    @Override
    public void getClientCapabilities(final GetClientCapabilities_Response callback) {
        // TODO(crbug.com/360327828): Implement.
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
        if (!GmsCoreUtils.isWebauthnSupported()
                || Build.VERSION.SDK_INT < Build.VERSION_CODES.P
                || !isChrome(mWebContents)) {
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
        // This is not implemented for anything other than getAssertion requests, since there is
        // no way to cancel a request that has already triggered gmscore UI. Get requests can be
        // cancelled if they are pending conditional UI requests, or if they are discoverable
        // credential requests with the account selector being shown to the user.
        if (!mIsOperationPending || mGetAssertionCallback == null) {
            return;
        }

        mPendingFido2CredentialRequest.cancelConditionalGetAssertion();
    }

    /** Callbacks for receiving responses from the internal handlers. */
    public void onRegisterResponse(int status, MakeCredentialAuthenticatorResponse response) {
        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending) return;

        assert mMakeCredentialCallback != null;
        assert status == AuthenticatorStatus.SUCCESS;
        mMakeCredentialCallback.call(status, response, null);
        cleanupRequest();
    }

    public void onSignResponse(int status, GetAssertionAuthenticatorResponse response) {
        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending) return;

        assert mGetAssertionCallback != null;
        mGetAssertionCallback.call(status, response, null);
        cleanupRequest();
    }

    public void onError(Integer status) {
        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending) return;

        assert ((mMakeCredentialCallback != null && mGetAssertionCallback == null)
                || (mMakeCredentialCallback == null && mGetAssertionCallback != null));
        assert status != AuthenticatorStatus.ERROR_WITH_DOM_EXCEPTION_DETAILS;
        if (mMakeCredentialCallback != null) {
            mMakeCredentialCallback.call(status, null, null);
        } else if (mGetAssertionCallback != null) {
            mGetAssertionCallback.call(status, null, null);
        }
        if (mPendingFido2CredentialRequest != null) mPendingFido2CredentialRequest.destroyBridge();
        cleanupRequest();
    }

    private void cleanupRequest() {
        mIsOperationPending = false;
        mMakeCredentialCallback = null;
        mGetAssertionCallback = null;
        mPendingFido2CredentialRequest = null;
    }

    @Override
    public void close() {
        mUnclosedFido2CredentialRequests.forEach(Fido2CredentialRequest::destroyBridge);
        mUnclosedFido2CredentialRequests.clear();
        cleanupRequest();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    @Override
    public FidoIntentSender getIntentSender() {
        return mIntentSender;
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    /** Implements {@link IntentSender} using a {@link WindowAndroid}. */
    public static class WindowIntentSender implements FidoIntentSender {
        private final WindowAndroid mWindow;

        WindowIntentSender(WindowAndroid window) {
            mWindow = window;
        }

        @Override
        public boolean showIntent(PendingIntent intent, Callback<Pair<Integer, Intent>> callback) {
            return mWindow != null
                    && mWindow.getActivity().get() != null
                    && mWindow.showCancelableIntent(intent, new CallbackWrapper(callback), null)
                            != WindowAndroid.START_INTENT_FAILURE;
        }

        private static class CallbackWrapper implements WindowAndroid.IntentCallback {
            private final Callback<Pair<Integer, Intent>> mCallback;

            CallbackWrapper(Callback<Pair<Integer, Intent>> callback) {
                mCallback = callback;
            }

            @Override
            public void onIntentCompleted(int resultCode, Intent data) {
                mCallback.onResult(new Pair(resultCode, data));
            }
        }
    }
}
