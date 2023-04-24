// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.OutcomeReceiver;
import android.os.Parcel;
import android.util.Base64;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.os.BuildCompat;

import com.google.android.gms.tasks.Task;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.AuthenticatorTransport;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.ClientDataJson;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.device.DeviceFeatureList;
import org.chromium.net.GURLUtils;
import org.chromium.url.Origin;

import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Uses the Google Play Services Fido2 APIs.
 * Holds the logic of each request.
 */
public class Fido2CredentialRequest implements Callback<Pair<Integer, Intent>> {
    private static final String TAG = "Fido2Request";
    private static final String CRED_MAN_PREFIX = "androidx.credentials.";
    static final String NON_EMPTY_ALLOWLIST_ERROR_MSG =
            "Authentication request must have non-empty allowList";
    static final String NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG =
            "Request doesn't have a valid list of allowed credentials.";
    static final String NO_SCREENLOCK_ERROR_MSG = "The device is not secured with any screen lock";
    static final String CREDENTIAL_EXISTS_ERROR_MSG =
            "One of the excluded credentials exists on the local device";
    static final String LOW_LEVEL_ERROR_MSG = "Low level error 0x6a80";

    private static Boolean sIsCredManEnabled;

    private final WebAuthenticationDelegate.IntentSender mIntentSender;
    private final @WebAuthenticationDelegate.Support int mSupportLevel;
    private GetAssertionResponseCallback mGetAssertionCallback;
    private MakeCredentialResponseCallback mMakeCredentialCallback;
    private FidoErrorResponseCallback mErrorCallback;
    private WebContents mWebContents;
    private boolean mAppIdExtensionUsed;
    private boolean mEchoCredProps;
    private WebAuthnBrowserBridge mBrowserBridge;
    private boolean mAttestationAcceptable;
    private boolean mIsCrossOrigin;

    private enum ConditionalUiState {
        NONE,
        WAITING_FOR_CREDENTIAL_LIST,
        WAITING_FOR_SELECTION,
        CANCEL_PENDING
    }

    private ConditionalUiState mConditionalUiState = ConditionalUiState.NONE;

    // Not null when the GMSCore-created ClientDataJson needs to be overridden or when using the
    // CredMan API.
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

    private boolean isCredManEnabled() {
        if (sIsCredManEnabled == null) {
            sIsCredManEnabled =
                    DeviceFeatureList.isEnabled(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN);
        }
        return sIsCredManEnabled;
    }

    @OptIn(markerClass = BuildCompat.PrereleaseSdkCheck.class)
    public void handleMakeCredentialRequest(PublicKeyCredentialCreationOptions options,
            RenderFrameHost frameHost, Origin origin, MakeCredentialResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        assert mMakeCredentialCallback == null && mErrorCallback == null;
        mMakeCredentialCallback = callback;
        mErrorCallback = errorCallback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }

        int securityCheck = frameHost.performMakeCredentialWebAuthSecurityChecks(
                options.relyingParty.id, origin, options.isPaymentCredentialCreation);
        if (securityCheck != AuthenticatorStatus.SUCCESS) {
            returnErrorAndResetCallback(securityCheck);
            return;
        }

        // Attestation is only for non-discoverable credentials in the Android
        // platform authenticator and discoverable credentials aren't supported
        // on security keys. There was a bug where discoverable credentials
        // accidentally included attestation, which was confusing, so that's
        // filtered here.
        mAttestationAcceptable = options.authenticatorSelection == null
                || options.authenticatorSelection.residentKey == ResidentKeyRequirement.DISCOURAGED;
        mEchoCredProps = options.credProps;

        if (isCredManEnabled() && BuildCompat.isAtLeastU()) {
            makeCredentialViaCredMan(options, origin, frameHost);
            return;
        }

        if (!apiAvailable()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
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

    @OptIn(markerClass = BuildCompat.PrereleaseSdkCheck.class)
    public void handleGetAssertionRequest(PublicKeyCredentialRequestOptions options,
            RenderFrameHost frameHost, Origin callerOrigin, PaymentOptions payment,
            GetAssertionResponseCallback callback, FidoErrorResponseCallback errorCallback) {
        assert mGetAssertionCallback == null && mErrorCallback == null;
        mGetAssertionCallback = callback;
        mErrorCallback = errorCallback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }

        WebAuthSecurityChecksResults webAuthSecurityChecksResults =
                frameHost.performGetAssertionWebAuthSecurityChecks(
                        options.relyingPartyId, callerOrigin, payment != null);
        if (webAuthSecurityChecksResults.securityCheckResult != AuthenticatorStatus.SUCCESS) {
            returnErrorAndResetCallback(webAuthSecurityChecksResults.securityCheckResult);
            return;
        }
        mIsCrossOrigin = webAuthSecurityChecksResults.isCrossOrigin;

        boolean hasAllowCredentials =
                options.allowCredentials != null && options.allowCredentials.length != 0;

        if (!hasAllowCredentials) {
            // No UVM support for discoverable credentials.
            options.userVerificationMethods = false;
        }

        if (options.appid != null) {
            mAppIdExtensionUsed = true;
        }

        String callerOriginString = convertOriginToString(callerOrigin);
        byte[] clientDataHash = null;

        // Payments should still go through Google Play Services.
        if (payment == null && isCredManEnabled() && BuildCompat.isAtLeastU()) {
            if (options.isConditional) {
                mConditionalUiState = ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST;
                prefetchCredentialsViaCredMan(
                        options, callerOrigin, frameHost, callerOriginString, clientDataHash);
            } else {
                getCredentialViaCredMan(options, callerOrigin, frameHost);
            }
            return;
        }

        if (!apiAvailable()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        if (payment != null
                && PaymentFeatureList.isEnabled(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION)) {
            assert options.challenge != null;
            clientDataHash = buildClientDataJsonAndComputeHash(ClientDataRequestType.PAYMENT_GET,
                    callerOriginString, options.challenge, mIsCrossOrigin, payment,
                    options.relyingPartyId, mWebContents.getMainFrame().getLastCommittedOrigin());
            if (clientDataHash == null) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }
        }

        if (options.isConditional
                || (ContentFeatureList.isEnabled(
                            ContentFeatures.WEB_AUTHN_TOUCH_TO_FILL_CREDENTIAL_SELECTION)
                        && !hasAllowCredentials)) {
            // For use in the lambda expression.
            final byte[] finalClientDataHash = clientDataHash;
            mConditionalUiState = ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST;
            Fido2ApiCallHelper.getInstance().invokeFido2GetCredentials(options.relyingPartyId,
                    mSupportLevel,
                    (credentials)
                            -> onWebAuthnCredentialDetailsListReceived(frameHost, options,
                                    callerOriginString, finalClientDataHash, credentials),
                    this::onBinderCallException);
            return;
        }

        maybeDispatchGetAssertionRequest(options, callerOriginString, clientDataHash, null);
    }

    public void cancelConditionalGetAssertion(RenderFrameHost frameHost) {
        if (mConditionalUiState == ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST) {
            mConditionalUiState = ConditionalUiState.CANCEL_PENDING;
            returnErrorAndResetCallback(AuthenticatorStatus.ABORT_ERROR);
            return;
        }

        if (mConditionalUiState == ConditionalUiState.WAITING_FOR_SELECTION) {
            mConditionalUiState = ConditionalUiState.CANCEL_PENDING;
            mBrowserBridge.cancelRequest(frameHost);
        }
    }

    public void handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
            RenderFrameHost frameHost, IsUvpaaResponseCallback callback) {
        if (isCredManEnabled()) {
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(true);
            return;
        }

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
        task.addOnFailureListener((e) -> {
            Log.e(TAG, "FIDO2 API call failed", e);
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
        });
    }

    public void handleGetMatchingCredentialIdsRequest(RenderFrameHost frameHost,
            String relyingPartyId, byte[][] allowCredentialIds, boolean requireThirdPartyPayment,
            GetMatchingCredentialIdsResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        assert mErrorCallback == null;
        mErrorCallback = errorCallback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }

        if (!apiAvailable()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        Fido2ApiCallHelper.getInstance().invokeFido2GetCredentials(relyingPartyId, mSupportLevel,
                (credentials)
                        -> onGetMatchingCredentialIdsListReceived(credentials, allowCredentialIds,
                                requireThirdPartyPayment, callback),
                this::onBinderCallException);
        return;
    }

    private void onGetMatchingCredentialIdsListReceived(
            List<WebAuthnCredentialDetails> retrievedCredentials, byte[][] allowCredentialIds,
            boolean requireThirdPartyPayment, GetMatchingCredentialIdsResponseCallback callback) {
        List<byte[]> matchingCredentialIds = new ArrayList<>();
        for (WebAuthnCredentialDetails credential : retrievedCredentials) {
            if (requireThirdPartyPayment && !credential.mIsPayment) continue;

            for (byte[] allowedId : allowCredentialIds) {
                if (Arrays.equals(allowedId, credential.mCredentialId)) {
                    matchingCredentialIds.add(credential.mCredentialId);
                    break;
                }
            }
        }
        callback.onResponse(matchingCredentialIds);
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
        assert mConditionalUiState == ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST
                || mConditionalUiState == ConditionalUiState.CANCEL_PENDING;

        boolean hasAllowCredentials =
                options.allowCredentials != null && options.allowCredentials.length != 0;
        boolean isConditionalRequest = options.isConditional;
        assert isConditionalRequest || !hasAllowCredentials;

        if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
            // The request was completed synchronously when the cancellation was received.
            return;
        }

        List<WebAuthnCredentialDetails> discoverableCredentials = new ArrayList<>();
        for (WebAuthnCredentialDetails credential : credentials) {
            if (!credential.mIsDiscoverable) continue;

            if (!hasAllowCredentials) {
                discoverableCredentials.add(credential);
                continue;
            }

            for (PublicKeyCredentialDescriptor descriptor : options.allowCredentials) {
                if (Arrays.equals(credential.mCredentialId, descriptor.id)) {
                    discoverableCredentials.add(credential);
                    break;
                }
            }
        }

        if (!isConditionalRequest && discoverableCredentials.isEmpty()) {
            mConditionalUiState = ConditionalUiState.NONE;
            // When no passkeys are present for a non-conditional request, pass the request
            // through to GMSCore. It will show an error message to the user. If at some point in
            // the future GMSCore supports passkeys from other devices, this will also allow it to
            // initiate a cross-device flow.
            maybeDispatchGetAssertionRequest(options, callerOriginString, clientDataHash, null);
            return;
        }

        if (mBrowserBridge == null) {
            mBrowserBridge = new WebAuthnBrowserBridge();
        }

        mConditionalUiState = ConditionalUiState.WAITING_FOR_SELECTION;
        mBrowserBridge.onCredentialsDetailsListReceived(frameHost, discoverableCredentials,
                isConditionalRequest,
                (selectedCredentialId)
                        -> maybeDispatchGetAssertionRequest(
                                options, callerOriginString, clientDataHash, selectedCredentialId));
    }

    private void maybeDispatchGetAssertionRequest(PublicKeyCredentialRequestOptions options,
            String callerOriginString, byte[] clientDataHash, byte[] credentialId) {
        // For Conditional UI requests, this is invoked by a callback, and might have been
        // cancelled before a credential was selected.
        if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
            mConditionalUiState = ConditionalUiState.NONE;
            returnErrorAndResetCallback(AuthenticatorStatus.ABORT_ERROR);
            return;
        }

        mConditionalUiState = ConditionalUiState.NONE;
        if (credentialId != null) {
            if (credentialId.length == 0) {
                if (options.isConditional) {
                    // An empty credential ID means an error from native code, which can happen if
                    // the embedder does not support Conditional UI.
                    Log.e(TAG, "Empty credential ID from account selection.");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                // For non-conditional requests, an empty credential ID means the user dismissed
                // the account selection dialog.
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
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
                        response = Fido2Api.parseIntentResponse(data, mAttestationAcceptable);
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
            if (response instanceof MakeCredentialAuthenticatorResponse) {
                MakeCredentialAuthenticatorResponse creationResponse =
                        (MakeCredentialAuthenticatorResponse) response;
                if (mEchoCredProps) {
                    // The other credProps fields will have been set by
                    // `parseIntentResponse` if Play Services provided credProps
                    // information.
                    creationResponse.echoCredProps = true;
                }
                mMakeCredentialCallback.onRegisterResponse(
                        AuthenticatorStatus.SUCCESS, creationResponse);
                mMakeCredentialCallback = null;
                return;
            }
        } else if (mGetAssertionCallback != null) {
            if (response instanceof GetAssertionAuthenticatorResponse) {
                GetAssertionAuthenticatorResponse r = (GetAssertionAuthenticatorResponse) response;
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
                }
                return AuthenticatorStatus.UNKNOWN_ERROR;
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

    private String getCredManExceptionType(Throwable exception) {
        try {
            return (String) exception.getClass().getMethod("getType").invoke(exception);
        } catch (ReflectiveOperationException e) {
            // This will map to UNKNOWN_ERROR.
            return "Exception details not available";
        }
    }

    /**
     * Create a credential using the Android 14 CredMan API.
     * TODO: update the version code to U when Chromium builds with Android 14 SDK.
     */
    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    @SuppressWarnings("WrongConstant")
    private void makeCredentialViaCredMan(
            PublicKeyCredentialCreationOptions options, Origin origin, RenderFrameHost frameHost) {
        final String requestAsJson =
                Fido2CredentialRequestJni.get().createOptionsToJson(options.serialize());
        final Context context = ContextUtils.getApplicationContext();

        final byte[] clientDataHash =
                buildClientDataJsonAndComputeHash(ClientDataRequestType.WEB_AUTHN_CREATE,
                        convertOriginToString(origin), options.challenge,
                        /*isCrossOrigin=*/false, /*paymentOptions=*/null, options.relyingParty.id,
                        /*topOrigin=*/null);
        if (clientDataHash == null) {
            returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
            return;
        }

        final Bundle requestBundle = new Bundle();
        requestBundle.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_SUBTYPE",
                CRED_MAN_PREFIX + "BUNDLE_VALUE_SUBTYPE_CREATE_PUBLIC_KEY_CREDENTIAL_REQUEST");
        requestBundle.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_JSON", requestAsJson);
        requestBundle.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_CLIENT_DATA_HASH",
                Base64.encodeToString(
                        clientDataHash, Base64.NO_PADDING | Base64.NO_WRAP | Base64.URL_SAFE));
        requestBundle.putBoolean(
                CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS", false);

        // The Android 14 APIs have to be called via reflection until Chromium
        // builds with the Android 14 SDK by default.
        OutcomeReceiver receiver = new OutcomeReceiver<Object, Throwable>() {
            @Override
            public void onError(Throwable e) {
                String errorType = getCredManExceptionType(e);
                Log.e(TAG, "CredMan CreateCredential call failed: %s",
                        errorType + " (" + e.getMessage() + ")");
                if (errorType.equals(
                            "android.credentials.CreateCredentialException.TYPE_USER_CANCELED")) {
                    returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                } else {
                    // Includes:
                    //  * CreateCredentialException.TYPE_UNKNOWN
                    //  * CreateCredentialException.TYPE_NO_CREATE_OPTIONS
                    //  * CreateCredentialException.TYPE_INTERRUPTED
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                }
            }

            @Override
            public void onResult(Object createCredentialResponse) {
                Bundle data;
                try {
                    data = (Bundle) createCredentialResponse.getClass().getMethod("getData").invoke(
                            createCredentialResponse);
                } catch (ReflectiveOperationException e) {
                    Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }

                String json =
                        data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_REGISTRATION_RESPONSE_JSON");
                byte[] responseSerialized =
                        Fido2CredentialRequestJni.get().makeCredentialResponseFromJson(json);
                if (responseSerialized == null) {
                    Log.e(TAG, "Failed to convert response from CredMan to Mojo object");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                MakeCredentialAuthenticatorResponse response =
                        MakeCredentialAuthenticatorResponse.deserialize(
                                ByteBuffer.wrap(responseSerialized));
                if (response == null) {
                    Log.e(TAG, "Failed to parse Mojo object");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                response.info.clientDataJson = mClientDataJson.getBytes();
                if (mEchoCredProps) {
                    response.echoCredProps = true;
                }
                mMakeCredentialCallback.onRegisterResponse(AuthenticatorStatus.SUCCESS, response);
                mMakeCredentialCallback = null;
            }
        };

        try {
            final Class createCredentialRequestBuilder =
                    Class.forName("android.credentials.CreateCredentialRequest$Builder");
            final Object builder =
                    createCredentialRequestBuilder
                            .getConstructor(String.class, Bundle.class, Bundle.class)
                            .newInstance(CRED_MAN_PREFIX + "TYPE_PUBLIC_KEY_CREDENTIAL",
                                    requestBundle, requestBundle);
            final Class builderClass = builder.getClass();
            builderClass.getMethod("setAlwaysSendAppInfoToProvider", boolean.class)
                    .invoke(builder, true);
            builderClass.getMethod("setOrigin", String.class)
                    .invoke(builder, convertOriginToString(origin));
            final Object request = builderClass.getMethod("build").invoke(builder);
            // TODO: switch "credential" to `Context.CREDENTIAL_SERVICE` and remove the
            // `@SuppressWarnings` when the Android U SDK is available.
            final Object manager = context.getSystemService("credential");
            try {
                manager.getClass()
                        .getMethod("createCredential", Context.class, request.getClass(),
                                android.os.CancellationSignal.class,
                                java.util.concurrent.Executor.class, OutcomeReceiver.class)
                        .invoke(manager, context, request, null, context.getMainExecutor(),
                                receiver);
            } catch (NoSuchMethodException e) {
                // In order to be compatible with Android 14 Beta 1, the older
                // form of the call is also tried.
                final Activity activity = WebContentsStatics.fromRenderFrameHost(frameHost)
                                                  .getTopLevelNativeWindow()
                                                  .getActivity()
                                                  .get();
                if (activity == null) {
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                manager.getClass()
                        .getMethod("createCredential", request.getClass(), Activity.class,
                                android.os.CancellationSignal.class,
                                java.util.concurrent.Executor.class, OutcomeReceiver.class)
                        .invoke(manager, request, activity, null, context.getMainExecutor(),
                                receiver);
            }
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }
    }

    /**
     * Gets the credential using the Android 14 CredMan API.
     * TODO: update the version code to U when Chromium builds with Android 14 SDK.
     */
    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    @SuppressWarnings("WrongConstant")
    private void getCredentialViaCredMan(
            PublicKeyCredentialRequestOptions options, Origin origin, RenderFrameHost frameHost) {
        final Context context = ContextUtils.getApplicationContext();

        // The Android 14 APIs have to be called via reflection until Chromium
        // builds with the Android 14 SDK by default.
        OutcomeReceiver<Object, Throwable> receiver = new OutcomeReceiver<>() {
            @Override
            public void onError(Throwable getCredentialException) {
                String errorType = getCredManExceptionType(getCredentialException);
                Log.e(TAG, "CredMan getCredential call failed: %s",
                        errorType + " (" + getCredentialException.getMessage() + ")");
                if (errorType.equals(
                            "android.credentials.GetCredentialException.TYPE_USER_CANCELED")) {
                    returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                } else {
                    // Includes:
                    //  * GetCredentialException.TYPE_UNKNOWN
                    //  * GetCredentialException.TYPE_NO_CREATE_OPTIONS
                    //  * GetCredentialException.TYPE_INTERRUPTED
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                }
            }

            @Override
            public void onResult(Object getCredentialResponse) {
                Bundle data;
                try {
                    Object credential = getCredentialResponse.getClass()
                                                .getMethod("getCredential")
                                                .invoke(getCredentialResponse);
                    data = (Bundle) credential.getClass().getMethod("getData").invoke(credential);

                } catch (ReflectiveOperationException e) {
                    Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }

                String json =
                        data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_AUTHENTICATION_RESPONSE_JSON");
                byte[] responseSerialized =
                        Fido2CredentialRequestJni.get().getCredentialResponseFromJson(json);
                if (responseSerialized == null) {
                    Log.e(TAG, "Failed to convert response from CredMan to Mojo object");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }

                GetAssertionAuthenticatorResponse response =
                        GetAssertionAuthenticatorResponse.deserialize(
                                ByteBuffer.wrap(responseSerialized));
                if (response == null) {
                    Log.e(TAG, "Failed to parse Mojo object");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                response.info.clientDataJson = mClientDataJson.getBytes();
                if (mAppIdExtensionUsed) {
                    response.echoAppidExtension = mAppIdExtensionUsed;
                }
                mGetAssertionCallback.onSignResponse(AuthenticatorStatus.SUCCESS, response);
                mGetAssertionCallback = null;
            }
        };

        try {
            final Object getCredentialRequest = buildGetCredentialRequest(options, origin);
            if (getCredentialRequest == null) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }

            // TODO: switch "credential" to `Context.CREDENTIAL_SERVICE` and remove the
            // `@SuppressWarnings` when the Android U SDK is available.
            final Object manager = context.getSystemService("credential");
            try {
                manager.getClass()
                        .getMethod("getCredential", Context.class, getCredentialRequest.getClass(),
                                android.os.CancellationSignal.class,
                                java.util.concurrent.Executor.class, OutcomeReceiver.class)
                        .invoke(manager, context, getCredentialRequest, null,
                                context.getMainExecutor(), receiver);
            } catch (NoSuchMethodException e) {
                // In order to be compatible with Android 14 Beta 1, the older
                // form of the call is also tried.
                final Activity activity = WebContentsStatics.fromRenderFrameHost(frameHost)
                                                  .getTopLevelNativeWindow()
                                                  .getActivity()
                                                  .get();
                if (activity == null) {
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                manager.getClass()
                        .getMethod("getCredential", getCredentialRequest.getClass(), Activity.class,
                                android.os.CancellationSignal.class,
                                java.util.concurrent.Executor.class, OutcomeReceiver.class)
                        .invoke(manager, getCredentialRequest, activity, null,
                                context.getMainExecutor(), receiver);
            }
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }
    }

    /**
     * Queries credential availability using the Android 14 CredMan API.
     * TODO: update the version code to U when Chromium builds with Android 14 SDK.
     */
    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    @SuppressWarnings("WrongConstant")
    private void prefetchCredentialsViaCredMan(PublicKeyCredentialRequestOptions options,
            Origin origin, RenderFrameHost frameHost, String callerOriginString,
            byte[] clientDataHash) {
        final Context context = ContextUtils.getApplicationContext();

        // The Android 14 APIs have to be called via reflection until Chromium
        // builds with the Android 14 SDK by default.
        OutcomeReceiver<Object, Throwable> receiver = new OutcomeReceiver<>() {
            @Override
            public void onError(Throwable e) {
                // prepareGetCredential uses getCredentialException, but it cannot be user
                // cancelled so all errors map to UNKNOWN_ERROR.
                Log.e(TAG, "CredMan prepareGetCredential call failed: %s",
                        getCredManExceptionType(e) + " (" + e.getMessage() + ")");
                returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            }

            @Override
            public void onResult(Object prepareGetCredentialResponse) {
                if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                    // The request was completed synchronously when the cancellation was received.
                    return;
                }
                boolean hasPublicKeyCredentials;
                boolean hasPasswordCredentials;
                boolean hasRemoteResults;
                Object pendingGetCredentialHandle;
                try {
                    Method hasCredentialResultsMethod =
                            prepareGetCredentialResponse.getClass().getMethod(
                                    "hasCredentialResults", String.class);
                    hasPublicKeyCredentials = (Boolean) hasCredentialResultsMethod.invoke(
                            prepareGetCredentialResponse,
                            "androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
                    hasPasswordCredentials = (Boolean) hasCredentialResultsMethod.invoke(
                            prepareGetCredentialResponse,
                            "android.credentials.TYPE_PASSWORD_CREDENTIAL");
                    hasRemoteResults = (Boolean) prepareGetCredentialResponse.getClass()
                                               .getMethod("hasRemoteResults")
                                               .invoke(prepareGetCredentialResponse);
                    pendingGetCredentialHandle = prepareGetCredentialResponse.getClass()
                                                         .getMethod("getPendingGetCredentialHandle")
                                                         .invoke(prepareGetCredentialResponse);

                } catch (ReflectiveOperationException e) {
                    Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                if (pendingGetCredentialHandle == null) {
                    Log.e(TAG, "prepareGetCredentialResponse is unusable.");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }

                if (mBrowserBridge == null) {
                    mBrowserBridge = new WebAuthnBrowserBridge();
                }
                mConditionalUiState = ConditionalUiState.WAITING_FOR_SELECTION;
                // TODO: Wire `hasPublicKeyCredentials`, `hasPublicKeyCredentials`,
                // `hasRemoteResults` and `pendingGetCredentialHandle` instead of
                // calling `onCredentialsDetailsListReceived`.
                mBrowserBridge.onCredentialsDetailsListReceived(frameHost,
                        new ArrayList<WebAuthnCredentialDetails>(), options.isConditional,
                        (selectedCredentialId)
                                -> maybeDispatchGetAssertionRequest(options, callerOriginString,
                                        clientDataHash, selectedCredentialId));
            }
        };

        try {
            final Object getCredentialRequest = buildGetCredentialRequest(options, origin);
            if (getCredentialRequest == null) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }

            // TODO: switch "credential" to `Context.CREDENTIAL_SERVICE` and remove the
            // `@SuppressWarnings` when the Android U SDK is available.
            final Object manager = context.getSystemService("credential");
            manager.getClass()
                    .getMethod("prepareGetCredential", getCredentialRequest.getClass(),
                            android.os.CancellationSignal.class,
                            java.util.concurrent.Executor.class, OutcomeReceiver.class)
                    .invoke(manager, getCredentialRequest, null, context.getMainExecutor(),
                            receiver);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }
    }

    private Object buildGetCredentialRequest(PublicKeyCredentialRequestOptions options,
            Origin origin) throws ReflectiveOperationException {
        final String requestAsJson =
                Fido2CredentialRequestJni.get().getOptionsToJson(options.serialize());

        final byte[] clientDataHash =
                buildClientDataJsonAndComputeHash(ClientDataRequestType.WEB_AUTHN_GET,
                        convertOriginToString(origin), options.challenge, mIsCrossOrigin,
                        /*paymentOptions=*/null, options.relyingPartyId, /*topOrigin=*/null);
        if (clientDataHash == null) {
            Log.e(TAG, "ClientDataJson generation failed.");
            return null;
        }

        Bundle publicKeyCredentialOptionBundle = buildPublicKeyCredentialOptionBundle(requestAsJson,
                Base64.encodeToString(
                        clientDataHash, Base64.NO_PADDING | Base64.NO_WRAP | Base64.URL_SAFE));

        // Build the CredentialOption:
        Object credentialOption;
        try {
            final Class<?> credentialOptionBuilderClass =
                    Class.forName("android.credentials.CredentialOption$Builder");
            final Object credentialOptionBuilder =
                    credentialOptionBuilderClass
                            .getConstructor(String.class, Bundle.class, Bundle.class)
                            .newInstance(CRED_MAN_PREFIX + "TYPE_PUBLIC_KEY_CREDENTIAL",
                                    publicKeyCredentialOptionBundle,
                                    publicKeyCredentialOptionBundle);
            credentialOption =
                    credentialOptionBuilderClass.getMethod("build").invoke(credentialOptionBuilder);
        } catch (ClassNotFoundException e) {
            // In order to be compatible with Android 14 Beta 1, the older
            // form of the call is also tried.
            credentialOption =
                    Class.forName("android.credentials.CredentialOption")
                            .getConstructor(String.class, Bundle.class, Bundle.class, Boolean.TYPE)
                            .newInstance(CRED_MAN_PREFIX + "TYPE_PUBLIC_KEY_CREDENTIAL",
                                    publicKeyCredentialOptionBundle,
                                    publicKeyCredentialOptionBundle, false);
        }

        // Build the GetCredentialRequest:
        final Class<?> getCredentialRequestBuilderClass =
                Class.forName("android.credentials.GetCredentialRequest$Builder");
        final Object getCredentialRequestBuilderObject =
                getCredentialRequestBuilderClass.getConstructor(Bundle.class)
                        .newInstance(new Bundle());
        getCredentialRequestBuilderClass
                .getMethod("addCredentialOption", credentialOption.getClass())
                .invoke(getCredentialRequestBuilderObject, credentialOption);
        getCredentialRequestBuilderClass.getMethod("setOrigin", String.class)
                .invoke(getCredentialRequestBuilderObject, convertOriginToString(origin));
        return getCredentialRequestBuilderClass.getMethod("build").invoke(
                getCredentialRequestBuilderObject);
    }

    private Bundle buildPublicKeyCredentialOptionBundle(
            String requestAsJson, String encodedClientDataHash) {
        final Bundle publicKeyCredentialOptionBundle = new Bundle();
        publicKeyCredentialOptionBundle.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_SUBTYPE",
                CRED_MAN_PREFIX + "BUNDLE_VALUE_SUBTYPE_GET_PUBLIC_KEY_CREDENTIAL_OPTION");
        publicKeyCredentialOptionBundle.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_JSON", requestAsJson);
        publicKeyCredentialOptionBundle.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_CLIENT_DATA_HASH", encodedClientDataHash);
        publicKeyCredentialOptionBundle.putBoolean(
                CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS", false);
        return publicKeyCredentialOptionBundle;
    }

    private byte[] buildClientDataJsonAndComputeHash(
            @ClientDataRequestType int clientDataRequestType, String callerOrigin, byte[] challenge,
            boolean isCrossOrigin, PaymentOptions paymentOptions, String relyingPartyId,
            Origin topOrigin) {
        mClientDataJson = ClientDataJson.buildClientDataJson(clientDataRequestType, callerOrigin,
                challenge, isCrossOrigin, paymentOptions, relyingPartyId, topOrigin);
        if (mClientDataJson == null) {
            return null;
        }
        MessageDigest messageDigest;
        try {
            messageDigest = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            return null;
        }
        messageDigest.update(mClientDataJson.getBytes());
        return messageDigest.digest();
    }

    @NativeMethods
    interface Natives {
        String createOptionsToJson(ByteBuffer serializedOptions);
        byte[] makeCredentialResponseFromJson(String json);
        String getOptionsToJson(ByteBuffer serializedOptions);
        byte[] getCredentialResponseFromJson(String json);
    }
}
