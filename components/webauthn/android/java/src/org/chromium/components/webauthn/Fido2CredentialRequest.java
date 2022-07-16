// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.fido.Fido;
import com.google.android.gms.fido.fido2.Fido2PrivilegedApiClient;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorErrorResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorResponse;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialRequestOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredential;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialCreationOptions;
import com.google.android.gms.tasks.Task;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.ClientDataJson;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.net.GURLUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/**
 * Uses the Google Play Services Fido2 APIs.
 * Holds the logic of each request.
 */
public class Fido2CredentialRequest implements WindowAndroid.IntentCallback {
    private static final String TAG = "Fido2Request";
    private GetAssertionResponseCallback mGetAssertionCallback;
    private MakeCredentialResponseCallback mMakeCredentialCallback;
    private FidoErrorResponseCallback mErrorCallback;
    private Fido2PrivilegedApiClient mFido2ApiClient;
    private WebContents mWebContents;
    private WindowAndroid mWindow;
    private @RequestStatus int mRequestStatus;
    private boolean mAppIdExtensionUsed;
    private long mStartTimeMs;

    // Not null when the GMSCore-created ClientDataJson needs to be overridden.
    @Nullable
    private String mClientDataJson;

    @IntDef({
            REGISTER_REQUEST,
            SIGN_REQUEST,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface RequestStatus {}

    /** Request status: processing a register request. */
    public static final int REGISTER_REQUEST = 1;

    /** Request status: processing a sign request. */
    public static final int SIGN_REQUEST = 2;

    /** The key used to retrieve PublicKeyCredential. */
    public static final String FIDO2_KEY_CREDENTIAL_EXTRA = "FIDO2_CREDENTIAL_EXTRA";

    private void returnErrorAndResetCallback(int error) {
        assert mErrorCallback != null;
        if (mErrorCallback == null) return;
        mErrorCallback.onError(error);
        mErrorCallback = null;
        mGetAssertionCallback = null;
        mMakeCredentialCallback = null;
    }

    public void handleMakeCredentialRequest(
            org.chromium.blink.mojom.PublicKeyCredentialCreationOptions options,
            RenderFrameHost frameHost, Origin origin, MakeCredentialResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        assert mMakeCredentialCallback == null && mErrorCallback == null;
        mMakeCredentialCallback = callback;
        mErrorCallback = errorCallback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }

        mRequestStatus = REGISTER_REQUEST;

        if (!initFido2ApiClient()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        int securityCheck = frameHost.performMakeCredentialWebAuthSecurityChecks(
                options.relyingParty.id, origin, options.isPaymentCredentialCreation);
        if (securityCheck != AuthenticatorStatus.SUCCESS) {
            returnErrorAndResetCallback(securityCheck);
            return;
        }

        PublicKeyCredentialCreationOptions credentialCreationOptions;
        try {
            credentialCreationOptions = Fido2Helper.toMakeCredentialOptions(options);
        } catch (NoSuchAlgorithmException e) {
            returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
            return;
        }

        BrowserPublicKeyCredentialCreationOptions browserRequestOptions =
                new BrowserPublicKeyCredentialCreationOptions.Builder()
                        .setPublicKeyCredentialCreationOptions(credentialCreationOptions)
                        .setOrigin(Uri.parse(convertOriginToString(origin)))
                        .build();

        Task<PendingIntent> result =
                mFido2ApiClient.getRegisterPendingIntent(browserRequestOptions);
        result.addOnSuccessListener(this::onGotPendingIntent);
    }

    public void handleGetAssertionRequest(PublicKeyCredentialRequestOptions options,
            RenderFrameHost frameHost, Origin callerOrigin, PaymentOptions payment,
            GetAssertionResponseCallback callback, FidoErrorResponseCallback errorCallback) {
        assert mGetAssertionCallback == null && mErrorCallback == null;
        mGetAssertionCallback = callback;
        mErrorCallback = errorCallback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }

        mRequestStatus = SIGN_REQUEST;

        if (!initFido2ApiClient()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        WebAuthSecurityChecksResults webAuthSecurityChecksResults =
                frameHost.performGetAssertionWebAuthSecurityChecks(
                        options.relyingPartyId, callerOrigin, payment != null);
        if (webAuthSecurityChecksResults.securityCheckResult != AuthenticatorStatus.SUCCESS) {
            returnErrorAndResetCallback(webAuthSecurityChecksResults.securityCheckResult);
            return;
        }

        if (options.appid != null) {
            mAppIdExtensionUsed = true;
        }

        com.google.android.gms.fido.fido2.api.common
                .PublicKeyCredentialRequestOptions getAssertionOptions;
        getAssertionOptions = Fido2Helper.toGetAssertionOptions(options);

        String callerOriginString = convertOriginToString(callerOrigin);
        BrowserPublicKeyCredentialRequestOptions.Builder browserRequestOptionsBuilder =
                new BrowserPublicKeyCredentialRequestOptions.Builder()
                        .setPublicKeyCredentialRequestOptions(getAssertionOptions)
                        .setOrigin(Uri.parse(callerOriginString));

        if (payment != null
                && PaymentFeatureList.isEnabled(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION)) {
            assert options.challenge != null;
            mClientDataJson = ClientDataJson.buildClientDataJson(ClientDataRequestType.PAYMENT_GET,
                    callerOriginString, options.challenge,
                    webAuthSecurityChecksResults.isCrossOrigin, payment, options.relyingPartyId,
                    mWebContents.getLastCommittedUrl().getOrigin().getSpec());
            if (mClientDataJson == null) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }
            MessageDigest messageDigest;
            try {
                messageDigest = MessageDigest.getInstance("SHA-256");
            } catch (NoSuchAlgorithmException e) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }
            messageDigest.update(mClientDataJson.getBytes());
            byte[] clientDataHash = messageDigest.digest();
            if (clientDataHash == null) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }
            browserRequestOptionsBuilder.setClientDataHash(clientDataHash);
        }

        Task<PendingIntent> result =
                mFido2ApiClient.getSignPendingIntent(browserRequestOptionsBuilder.build());
        result.addOnSuccessListener(this::onGotPendingIntent);
    }

    public void handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
            RenderFrameHost frameHost, IsUvpaaResponseCallback callback) {
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }
        if (!initFido2ApiClient()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            // Note that |IsUserVerifyingPlatformAuthenticatorAvailable| only returns
            // true or false, making it unable to handle any error status.
            // So it callbacks with false if Fido2PrivilegedApi is not available.
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
            return;
        }

        Task<Boolean> result =
                mFido2ApiClient.isUserVerifyingPlatformAuthenticatorAvailable()
                        .addOnSuccessListener((isUVPAA) -> {
                            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                                    isUVPAA);
                        });
    }

    /* Initialize the FIDO2 browser API client. */
    private boolean initFido2ApiClient() {
        if (mFido2ApiClient != null) {
            return true;
        }

        if (!ExternalAuthUtils.getInstance().canUseGooglePlayServices(
                    new UserRecoverableErrorHandler.Silent())) {
            return false;
        }

        mFido2ApiClient = Fido.getFido2PrivilegedApiClient(ContextUtils.getApplicationContext());
        if (mFido2ApiClient == null) {
            return false;
        }
        return true;
    }

    // Handles a PendingIntent from the GMSCore Fido library.
    private void onGotPendingIntent(PendingIntent pendingIntent) {
        if (pendingIntent == null) {
            Log.e(TAG, "Didn't receive a pending intent.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        if (mWindow == null) {
            mWindow = mWebContents.getTopLevelNativeWindow();
            if (mWindow == null) {
                Log.e(TAG, "Couldn't get ActivityWindowAndroid.");
                returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                return;
            }
        }

        final Activity activity = mWindow.getActivity().get();
        if (activity == null) {
            Log.e(TAG, "Null activity.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        // Record starting time that will be used to establish a timeout that will
        // be activated when we receive a response that cannot be returned to the
        // relying party prior to timeout.
        mStartTimeMs = SystemClock.elapsedRealtime();
        int requestCode = mWindow.showCancelableIntent(pendingIntent, this, null);

        if (requestCode == WindowAndroid.START_INTENT_FAILURE) {
            Log.e(TAG, "Failed to send Fido2 request to Google Play Services.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
        } else {
            Log.e(TAG, "Sent a Fido2 request to Google Play Services.");
        }
    }

    // Handles the result.
    @Override
    public void onIntentCompleted(int resultCode, Intent data) {
        if (data == null) {
            Log.e(TAG, "Received a null intent.");
            // The user canceled the request.
            returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
            return;
        }

        switch (resultCode) {
            case Activity.RESULT_CANCELED:
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                break;
            case Activity.RESULT_OK:
                processIntentResult(data);
                break;
            default:
                // Something went wrong.
                Log.e(TAG, "Failed with result code" + resultCode);
                returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                break;
        }
    }

    private void processPublicKeyCredential(Intent data) {
        PublicKeyCredential publicKeyCredential = PublicKeyCredential.deserializeFromBytes(
                data.getByteArrayExtra(FIDO2_KEY_CREDENTIAL_EXTRA));
        AuthenticatorResponse response = publicKeyCredential.getResponse();
        if (response instanceof AuthenticatorErrorResponse) {
            processErrorResponse((AuthenticatorErrorResponse) response);
        } else if (response instanceof AuthenticatorAttestationResponse) {
            try {
                mMakeCredentialCallback.onRegisterResponse(AuthenticatorStatus.SUCCESS,
                        Fido2Helper.toMakeCredentialResponse(publicKeyCredential));
                mMakeCredentialCallback = null;
            } catch (NoSuchAlgorithmException e) {
                returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
            }
        } else if (response instanceof AuthenticatorAssertionResponse) {
            mGetAssertionCallback.onSignResponse(AuthenticatorStatus.SUCCESS,
                    Fido2Helper.toGetAssertionResponse(
                            publicKeyCredential, mAppIdExtensionUsed, mClientDataJson));
            mClientDataJson = null;
            mGetAssertionCallback = null;
        }
    }

    private void processErrorResponse(AuthenticatorErrorResponse errorResponse) {
        Log.e(TAG,
                "Google Play Services FIDO2 API returned an error: "
                        + errorResponse.getErrorMessage());
        int authenticatorStatus = Fido2Helper.convertError(
                errorResponse.getErrorCode(), errorResponse.getErrorMessage());
        returnErrorAndResetCallback(authenticatorStatus);
    }

    private void processKeyResponse(Intent data) {
        switch (mRequestStatus) {
            case REGISTER_REQUEST:
                Log.e(TAG, "Received a register response from Google Play Services FIDO2 API");
                try {
                    mMakeCredentialCallback.onRegisterResponse(AuthenticatorStatus.SUCCESS,
                            Fido2Helper.toMakeCredentialResponse(
                                    AuthenticatorAttestationResponse.deserializeFromBytes(
                                            data.getByteArrayExtra(
                                                    Fido.FIDO2_KEY_RESPONSE_EXTRA))));
                } catch (NoSuchAlgorithmException e) {
                    returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
                }
                break;
            case SIGN_REQUEST:
                Log.e(TAG, "Received a sign response from Google Play Services FIDO2 API");
                mGetAssertionCallback.onSignResponse(AuthenticatorStatus.SUCCESS,
                        Fido2Helper.toGetAssertionResponse(
                                AuthenticatorAssertionResponse.deserializeFromBytes(
                                        data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)),
                                mAppIdExtensionUsed));
                break;
        }
        mMakeCredentialCallback = null;
        mGetAssertionCallback = null;
    }

    private void processIntentResult(Intent data) {
        // If returned PublicKeyCredential, use PublicKeyCredential to retrieve
        // [Attestation/Assertion/Error] Response, else directly retrieve
        // [Attestation/Assertion/Error] Response.
        if (data.hasExtra(FIDO2_KEY_CREDENTIAL_EXTRA)) {
            processPublicKeyCredential(data);
        } else if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            processErrorResponse(AuthenticatorErrorResponse.deserializeFromBytes(
                    data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA)));
        } else if (data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)) {
            processKeyResponse(data);
        } else {
            // Something went wrong.
            Log.e(TAG,
                    "The response is missing FIDO2_KEY_RESPONSE_EXTRA "
                            + "and FIDO2_KEY_CREDENTIAL_EXTRA.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
        }
    }

    @VisibleForTesting
    public String convertOriginToString(Origin origin) {
        // Wrapping with GURLUtils.getOrigin() in order to trim default ports.
        return GURLUtils.getOrigin(
                origin.getScheme() + "://" + origin.getHost() + ":" + origin.getPort());
    }

    @VisibleForTesting
    public void setWindowForTesting(WindowAndroid window) {
        mWindow = window;
    }

    @VisibleForTesting
    public void setWebContentsForTesting(WebContents webContents) {
        mWebContents = webContents;
    }
}
