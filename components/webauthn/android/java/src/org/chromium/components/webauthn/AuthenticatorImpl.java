// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.annotation.SuppressLint;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.PackageUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.mojo.system.MojoException;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Queue;
import java.util.Set;

/**
 * Android implementation of the authenticator.mojom interface.
 */
public final class AuthenticatorImpl implements Authenticator {
    private static final String GMSCORE_PACKAGE_NAME = "com.google.android.gms";
    public static final int GMSCORE_MIN_VERSION = 16890000;
    public static final int GMSCORE_MIN_VERSION_GET_MATCHING_CRED_IDS = 223300000;
    private final Context mContext;
    private final WebAuthenticationDelegate.IntentSender mIntentSender;
    private final RenderFrameHost mRenderFrameHost;

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

    /** Caches the GMS Core package version. */
    private int mGmsCorePackageVersion;

    private MakeCredential_Response mMakeCredentialCallback;
    private GetAssertion_Response mGetAssertionCallback;
    // A queue is used to store pending IsUserVerifyingPlatformAuthenticatorAvailable request
    // callbacks when there are multiple requests pending on the result from GMSCore. Noted that
    // the callbacks may not be invoked in the same order as the pending requests, which in this
    // situation does not matter because all pending requests will return the same value.
    private Queue<org.chromium.mojo.bindings.Callbacks.Callback1<Boolean>>
            mIsUserVerifyingPlatformAuthenticatorAvailableCallbackQueue = new LinkedList<>();
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
     *         Services.
     * @param renderFrameHost The host of the frame that has invoked the API.
     * @param topOrigin The origin of the main frame.
     */
    public AuthenticatorImpl(Context context, WebAuthenticationDelegate.IntentSender intentSender,
            RenderFrameHost renderFrameHost, Origin topOrigin) {
        assert renderFrameHost != null;

        mContext = context;
        mIntentSender = intentSender;
        mRenderFrameHost = renderFrameHost;
        mOrigin = mRenderFrameHost.getLastCommittedOrigin();
        mTopOrigin = topOrigin;

        mGmsCorePackageVersion = PackageUtils.getPackageVersion(GMSCORE_PACKAGE_NAME);
    }

    public static void overrideFido2CredentialRequestForTesting(Fido2CredentialRequest request) {
        sFido2CredentialRequestOverrideForTesting = request;
    }

    private Fido2CredentialRequest getFido2CredentialRequest() {
        if (sFido2CredentialRequestOverrideForTesting != null) {
            return sFido2CredentialRequestOverrideForTesting;
        }
        Fido2CredentialRequest request = new Fido2CredentialRequest(mIntentSender);
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
     * only if the user has confirmed the payment information that was displayed to the user.
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
        if (mGmsCorePackageVersion < GMSCORE_MIN_VERSION) {
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        mPendingFido2CredentialRequest = getFido2CredentialRequest();
        mPendingFido2CredentialRequest.handleMakeCredentialRequest(mContext, options,
                mRenderFrameHost, /*maybeClientDataHash=*/null, mOrigin,
                (status, response)
                        -> onRegisterResponse(status, response),
                status -> onError(status));
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

        if (mGmsCorePackageVersion < GMSCORE_MIN_VERSION) {
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        mPendingFido2CredentialRequest = getFido2CredentialRequest();
        mPendingFido2CredentialRequest.handleGetAssertionRequest(mContext, options,
                mRenderFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mTopOrigin, mPayment,
                (status, response) -> onSignResponse(status, response), status -> onError(status));
    }

    @Override
    public void isUserVerifyingPlatformAuthenticatorAvailable(
            final IsUserVerifyingPlatformAuthenticatorAvailable_Response callback) {
        IsUserVerifyingPlatformAuthenticatorAvailable_Response decoratedCallback = (isUvpaa) -> {
            RecordHistogram.recordBooleanHistogram(
                    "WebAuthentication.IsUVPlatformAuthenticatorAvailable2", isUvpaa);
            callback.call(isUvpaa);
        };

        if (mGmsCorePackageVersion < GMSCORE_MIN_VERSION) {
            decoratedCallback.call(false);
            return;
        }

        mIsUserVerifyingPlatformAuthenticatorAvailableCallbackQueue.add(decoratedCallback);
        getFido2CredentialRequest().handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
                mContext,
                isUvpaa -> onIsUserVerifyingPlatformAuthenticatorAvailableResponse(isUvpaa));
    }

    /**
     * Returns whether or not the getMatchingCredentialIds API is supported. As the API is
     * flag-guarded inside of GMSCore, we can only provide a best-effort guess based on the GMSCore
     * version.
     */
    public boolean isGetMatchingCredentialIdsSupported() {
        return mGmsCorePackageVersion >= GMSCORE_MIN_VERSION_GET_MATCHING_CRED_IDS;
    }

    /**
     * Retrieves the set of credentials for the given relying party, and filters them to match the
     * given input credential IDs. Optionally, may also filter the credentials to only return those
     * that are marked as third-party payment enabled.
     *
     * Because this functionality does not participate in the normal WebAuthn UI flow and is
     * idempotent at the Fido2 layer, it does not adhere to the 'one call at a time' logic used for
     * the create/get methods.
     */
    public void getMatchingCredentialIds(String relyingPartyId, byte[][] credentialIds,
            boolean requireThirdPartyPayment, GetMatchingCredentialIdsResponseCallback callback) {
        if (mGmsCorePackageVersion < GMSCORE_MIN_VERSION_GET_MATCHING_CRED_IDS) {
            callback.onResponse(new ArrayList<byte[]>());
            return;
        }

        getFido2CredentialRequest().handleGetMatchingCredentialIdsRequest(mRenderFrameHost,
                relyingPartyId, credentialIds, requireThirdPartyPayment, callback,
                status -> onError(status));
    }

    @Override
    public void isConditionalMediationAvailable(
            final IsConditionalMediationAvailable_Response callback) {
        if (mGmsCorePackageVersion < GMSCORE_MIN_VERSION
                || Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            callback.call(false);
            return;
        }

        // If the gmscore and chromium versions are out of sync for some reason, this method will
        // return true but chrome will ignore conditional requests. Android surfaces only platform
        // credentials on conditional requests, use IsUVPAA as a proxy for availability.
        mIsUserVerifyingPlatformAuthenticatorAvailableCallbackQueue.add(callback);
        getFido2CredentialRequest().handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
                mContext,
                isUvpaa -> onIsUserVerifyingPlatformAuthenticatorAvailableResponse(isUvpaa));
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

        mPendingFido2CredentialRequest.cancelConditionalGetAssertion(mRenderFrameHost);
    }

    /**
     * Callbacks for receiving responses from the internal handlers.
     */
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

    public void onIsUserVerifyingPlatformAuthenticatorAvailableResponse(boolean isUVPAA) {
        assert !mIsUserVerifyingPlatformAuthenticatorAvailableCallbackQueue.isEmpty();
        mIsUserVerifyingPlatformAuthenticatorAvailableCallbackQueue.poll().call(isUVPAA);
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

    /**
     * Implements {@link IntentSender} using a {@link WindowAndroid}.
     */
    public static class WindowIntentSender implements WebAuthenticationDelegate.IntentSender {
        private final WindowAndroid mWindow;

        WindowIntentSender(WindowAndroid window) {
            mWindow = window;
        }

        @Override
        public boolean showIntent(PendingIntent intent, Callback<Pair<Integer, Intent>> callback) {
            return mWindow != null && mWindow.getActivity().get() != null
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
