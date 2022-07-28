// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.os.Parcel;
import android.os.SystemClock;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.tasks.Task;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.AuthenticatorTransport;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.ClientDataJson;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.net.GURLUtils;
import org.chromium.url.Origin;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;

/**
 * Uses the Google Play Services Fido2 APIs.
 * Holds the logic of each request.
 */
public class Fido2CredentialRequest implements Callback<Pair<Integer, Intent>> {
    private static final String TAG = "Fido2Request";
    static final String NON_EMPTY_ALLOWLIST_ERROR_MSG =
            "Authentication request must have non-empty allowList";
    static final String NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG =
            "Request doesn't have a valid list of allowed credentials.";
    static final String NO_SCREENLOCK_ERROR_MSG = "The device is not secured with any screen lock";
    static final String CREDENTIAL_EXISTS_ERROR_MSG =
            "One of the excluded credentials exists on the local device";
    static final String LOW_LEVEL_ERROR_MSG = "Low level error 0x6a80";

    private final WebAuthenticationDelegate.IntentSender mIntentSender;
    private final @WebAuthenticationDelegate.Support int mSupportLevel;
    private GetAssertionResponseCallback mGetAssertionCallback;
    private MakeCredentialResponseCallback mMakeCredentialCallback;
    private FidoErrorResponseCallback mErrorCallback;
    private WebContents mWebContents;
    private boolean mAppIdExtensionUsed;
    private long mStartTimeMs;
    private WebAuthnBrowserBridge mBrowserBridge;

    // Not null when the GMSCore-created ClientDataJson needs to be overridden.
    @Nullable
    private String mClientDataJson;

    /**
     * Constructs the object.
     *
     * @param intentSender Interface for starting {@link Intent}s from Play Services.
     * @param supportLevel Whether this code should use the privileged or non-privileged Play
     *         Services API. (Note that a value of `NONE` is not allowed.)
     */
    public Fido2CredentialRequest(WebAuthenticationDelegate.IntentSender intentSender,
            @WebAuthenticationDelegate.Support int supportLevel) {
        assert supportLevel != WebAuthenticationDelegate.Support.NONE;

        mIntentSender = intentSender;
        mSupportLevel = supportLevel;
    }

    private void returnErrorAndResetCallback(int error) {
        assert mErrorCallback != null;
        if (mErrorCallback == null) return;
        mErrorCallback.onError(error);
        mErrorCallback = null;
        mGetAssertionCallback = null;
        mMakeCredentialCallback = null;
    }

    public void handleMakeCredentialRequest(PublicKeyCredentialCreationOptions options,
            RenderFrameHost frameHost, Origin origin, MakeCredentialResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        assert mMakeCredentialCallback == null && mErrorCallback == null;
        mMakeCredentialCallback = callback;
        mErrorCallback = errorCallback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }

        if (!apiAvailable()) {
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

        Fido2ApiCall call = new Fido2ApiCall(ContextUtils.getApplicationContext(), mSupportLevel);
        Parcel args = call.start();
        Fido2ApiCall.PendingIntentResult result = new Fido2ApiCall.PendingIntentResult(call);
        args.writeStrongBinder(result);
        args.writeInt(1); // This indicates that the following options are present.

        try {
            if (mSupportLevel == WebAuthenticationDelegate.Support.BROWSER) {
                Fido2Api.appendBrowserMakeCredentialOptionsToParcel(options,
                        Uri.parse(convertOriginToString(origin)), /* clientDataHash= */ null, args);
            } else {
                Fido2Api.appendMakeCredentialOptionsToParcel(options, args);
            }
        } catch (NoSuchAlgorithmException e) {
            returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
            return;
        }

        Task<PendingIntent> task = call.run(Fido2ApiCall.METHOD_BROWSER_REGISTER,
                Fido2ApiCall.TRANSACTION_REGISTER, args, result);
        task.addOnSuccessListener(this::onGotPendingIntent);
        task.addOnFailureListener(this::onBinderCallException);
    }

    private void onBinderCallException(Exception e) {
        Log.e(TAG, "FIDO2 API call failed", e);
        returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
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

        if (!apiAvailable()) {
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

        if (options.allowCredentials == null || options.allowCredentials.length == 0) {
            // No UVM support for discoverable credentials.
            options.userVerificationMethods = false;
        }

        if (options.appid != null) {
            mAppIdExtensionUsed = true;
        }

        String callerOriginString = convertOriginToString(callerOrigin);
        byte[] clientDataHash = null;

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
            clientDataHash = messageDigest.digest();
            if (clientDataHash == null) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }
        }

        if (options.isConditional) {
            // For use in the lambda expression.
            final byte[] finalClientDataHash = clientDataHash;
            Fido2ApiCallHelper.getInstance().invokeFido2GetCredentials(options.relyingPartyId,
                    mSupportLevel,
                    (credentials)
                            -> onWebAuthnCredentialDetailsListReceived(frameHost, options,
                                    callerOriginString, finalClientDataHash, credentials),
                    this::onBinderCallException);
            return;
        }

        dispatchGetAssertionRequest(options, callerOriginString, clientDataHash, null);
    }

    public void handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
            RenderFrameHost frameHost, IsUvpaaResponseCallback callback) {
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }
        if (!apiAvailable()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            // Note that |IsUserVerifyingPlatformAuthenticatorAvailable| only returns
            // true or false, making it unable to handle any error status.
            // So it callbacks with false if Fido2PrivilegedApi is not available.
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
            return;
        }

        Fido2ApiCall call = new Fido2ApiCall(ContextUtils.getApplicationContext(), mSupportLevel);
        Fido2ApiCall.BooleanResult result = new Fido2ApiCall.BooleanResult();
        Parcel args = call.start();
        args.writeStrongBinder(result);

        Task<Boolean> task = call.run(Fido2ApiCall.METHOD_BROWSER_ISUVPAA,
                Fido2ApiCall.TRANSACTION_ISUVPAA, args, result);
        task.addOnSuccessListener((isUVPAA) -> {
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(isUVPAA);
        });
        task.addOnFailureListener((e) -> { Log.e(TAG, "FIDO2 API call failed", e); });
    }

    @VisibleForTesting
    public void overrideBrowserBridgeForTesting(WebAuthnBrowserBridge bridge) {
        mBrowserBridge = bridge;
    }

    private boolean apiAvailable() {
        return ExternalAuthUtils.getInstance().canUseGooglePlayServices(
                new UserRecoverableErrorHandler.Silent());
    }

    private void onWebAuthnCredentialDetailsListReceived(RenderFrameHost frameHost,
            PublicKeyCredentialRequestOptions options, String callerOriginString,
            byte[] clientDataHash, List<WebAuthnCredentialDetails> credentials) {
        if (mBrowserBridge == null) {
            mBrowserBridge = new WebAuthnBrowserBridge();
        }
        List<WebAuthnCredentialDetails> discoverableCredentials = new ArrayList<>();
        for (WebAuthnCredentialDetails credential : credentials) {
            if (credential.mIsDiscoverable) {
                discoverableCredentials.add(credential);
            }
        }
        mBrowserBridge.onCredentialsDetailsListReceived(frameHost, discoverableCredentials,
                (selectedCredentialId)
                        -> dispatchGetAssertionRequest(
                                options, callerOriginString, clientDataHash, selectedCredentialId));
    }

    private void dispatchGetAssertionRequest(PublicKeyCredentialRequestOptions options,
            String callerOriginString, byte[] clientDataHash, byte[] credentialId) {
        if (credentialId != null) {
            if (credentialId.length == 0) {
                // An empty credential ID means an error from native code, which can happen if the
                // embedder does not support Conditional UI.
                Log.e(TAG, "Empty credential ID from account selection.");
                returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                return;
            }
            PublicKeyCredentialDescriptor selected_credential = new PublicKeyCredentialDescriptor();
            selected_credential.type = PublicKeyCredentialType.PUBLIC_KEY;
            selected_credential.id = credentialId;
            selected_credential.transports = new int[] {AuthenticatorTransport.INTERNAL};
            options.allowCredentials = new PublicKeyCredentialDescriptor[] {selected_credential};
        }
        Fido2ApiCall call = new Fido2ApiCall(ContextUtils.getApplicationContext(), mSupportLevel);
        Parcel args = call.start();
        Fido2ApiCall.PendingIntentResult result = new Fido2ApiCall.PendingIntentResult(call);
        args.writeStrongBinder(result);
        args.writeInt(1); // This indicates that the following options are present.

        if (mSupportLevel == WebAuthenticationDelegate.Support.BROWSER) {
            Fido2Api.appendBrowserGetAssertionOptionsToParcel(options,
                    Uri.parse(callerOriginString), clientDataHash, /*tunnelId=*/null, args);
        } else {
            Fido2Api.appendGetAssertionOptionsToParcel(options, /*tunnelId=*/null, args);
        }

        Task<PendingIntent> task = call.run(
                Fido2ApiCall.METHOD_BROWSER_SIGN, Fido2ApiCall.TRANSACTION_SIGN, args, result);
        task.addOnSuccessListener(this::onGotPendingIntent);
        task.addOnFailureListener(this::onBinderCallException);
    }

    // Handles a PendingIntent from the GMSCore FIDO library.
    private void onGotPendingIntent(PendingIntent pendingIntent) {
        if (pendingIntent == null) {
            Log.e(TAG, "Didn't receive a pending intent.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        // Record starting time that will be used to establish a timeout that will
        // be activated when we receive a response that cannot be returned to the
        // relying party prior to timeout.
        mStartTimeMs = SystemClock.elapsedRealtime();

        if (!mIntentSender.showIntent(pendingIntent, this)) {
            Log.e(TAG, "Failed to send intent to FIDO API");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }
    }

    // Handles the result.
    @Override
    public void onResult(Pair<Integer, Intent> result) {
        final int resultCode = result.first;
        final Intent data = result.second;
        int errorCode = AuthenticatorStatus.UNKNOWN_ERROR;
        Object response = null;

        switch (resultCode) {
            case Activity.RESULT_OK:
                if (data == null) {
                    errorCode = AuthenticatorStatus.NOT_ALLOWED_ERROR;
                } else {
                    try {
                        response = Fido2Api.parseIntentResponse(data);
                    } catch (IllegalArgumentException e) {
                        response = null;
                    }
                }
                break;

            case Activity.RESULT_CANCELED:
                errorCode = AuthenticatorStatus.NOT_ALLOWED_ERROR;
                break;

            default:
                Log.e(TAG, "FIDO2 PendingIntent resulted in code: " + resultCode);
                break;
        }

        if (response == null) {
            // Use the error already set.
        } else if (response instanceof Pair) {
            Pair<Integer, String> error = (Pair<Integer, String>) response;
            Log.e(TAG,
                    "FIDO2 API call resulted in error: " + error.first + " "
                            + (error.second != null ? error.second : ""));
            errorCode = convertError(error);
        } else if (mMakeCredentialCallback != null) {
            if (response instanceof org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse) {
                mMakeCredentialCallback.onRegisterResponse(AuthenticatorStatus.SUCCESS,
                        (org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse) response);
                mMakeCredentialCallback = null;
                return;
            }
        } else if (mGetAssertionCallback != null) {
            if (response instanceof org.chromium.blink.mojom.GetAssertionAuthenticatorResponse) {
                org.chromium.blink.mojom.GetAssertionAuthenticatorResponse r =
                        (org.chromium.blink.mojom.GetAssertionAuthenticatorResponse) response;
                if (mClientDataJson != null) {
                    r.info.clientDataJson = mClientDataJson.getBytes();
                }
                r.echoAppidExtension = mAppIdExtensionUsed;
                mGetAssertionCallback.onSignResponse(AuthenticatorStatus.SUCCESS, r);
                mGetAssertionCallback = null;
                return;
            }
        }

        returnErrorAndResetCallback(errorCode);
    }

    /**
     * Helper method to convert AuthenticatorErrorResponse errors.
     * @param errorCode
     * @return error code corresponding to an AuthenticatorStatus.
     */
    private static int convertError(Pair<Integer, String> error) {
        final int errorCode = error.first;
        @Nullable
        final String errorMsg = error.second;

        // TODO(b/113347251): Use specific error codes instead of strings when GmsCore Fido2
        // provides them.
        switch (errorCode) {
            case Fido2Api.SECURITY_ERR:
                // AppId or rpID fails validation.
                return AuthenticatorStatus.INVALID_DOMAIN;
            case Fido2Api.TIMEOUT_ERR:
                return AuthenticatorStatus.NOT_ALLOWED_ERROR;
            case Fido2Api.ENCODING_ERR:
                // Error encoding results (after user consent).
                return AuthenticatorStatus.UNKNOWN_ERROR;
            case Fido2Api.NOT_ALLOWED_ERR:
                // The implementation doesn't support resident keys.
                if (errorMsg != null
                        && (errorMsg.equals(NON_EMPTY_ALLOWLIST_ERROR_MSG)
                                || errorMsg.equals(NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG))) {
                    return AuthenticatorStatus.EMPTY_ALLOW_CREDENTIALS;
                }
                // The request is not allowed, possibly because the user denied permission.
                return AuthenticatorStatus.NOT_ALLOWED_ERROR;
            case Fido2Api.DATA_ERR:
            // Incoming requests were malformed/inadequate. Fallthrough.
            case Fido2Api.NOT_SUPPORTED_ERR:
                // Request parameters were not supported.
                return AuthenticatorStatus.ANDROID_NOT_SUPPORTED_ERROR;
            case Fido2Api.CONSTRAINT_ERR:
                if (errorMsg != null && errorMsg.equals(NO_SCREENLOCK_ERROR_MSG)) {
                    return AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED;
                } else {
                    // The user attempted to use a credential that was already registered.
                    return AuthenticatorStatus.CREDENTIAL_EXCLUDED;
                }
            case Fido2Api.INVALID_STATE_ERR:
                if (errorMsg != null && errorMsg.equals(CREDENTIAL_EXISTS_ERROR_MSG)) {
                    return AuthenticatorStatus.CREDENTIAL_EXCLUDED;
                }
            // else fallthrough.
            case Fido2Api.UNKNOWN_ERR:
                if (errorMsg != null && errorMsg.equals(LOW_LEVEL_ERROR_MSG)) {
                    // The error message returned from GmsCore when the user attempted to use a
                    // credential that is not registered with a U2F security key.
                    return AuthenticatorStatus.NOT_ALLOWED_ERROR;
                }
            // fall through
            default:
                return AuthenticatorStatus.UNKNOWN_ERROR;
        }
    }

    @VisibleForTesting
    public String convertOriginToString(Origin origin) {
        // Wrapping with GURLUtils.getOrigin() in order to trim default ports.
        return GURLUtils.getOrigin(
                origin.getScheme() + "://" + origin.getHost() + ":" + origin.getPort());
    }

    @VisibleForTesting
    public void setWebContentsForTesting(WebContents webContents) {
        mWebContents = webContents;
    }
}
