// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ComponentName;
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
import org.chromium.base.PackageUtils;
import org.chromium.base.annotations.JNINamespace;
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
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.browser.ClientDataJson;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.device.DeviceFeatureList;
import org.chromium.device.DeviceFeatureMap;
import org.chromium.net.GURLUtils;
import org.chromium.url.Origin;

import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/**
 * Uses the Google Play Services Fido2 APIs.
 * Holds the logic of each request.
 */
@JNINamespace("webauthn")
public class Fido2CredentialRequest implements Callback<Pair<Integer, Intent>> {
    private static final String TAG = "Fido2Request";
    private static final String CRED_MAN_PREFIX = "androidx.credentials.";
    private static final ComponentName GPM_COMPONENT_NAME =
            ComponentName.createRelative("com.google.android.gms",
                    ".auth.api.credentials.credman.service.PasswordAndPasskeyService");
    private static final String CHANNEL_KEY = "com.android.chrome.CHANNEL";
    private static final String TYPE_PASSKEY = CRED_MAN_PREFIX + "TYPE_PUBLIC_KEY_CREDENTIAL";
    static final String NON_EMPTY_ALLOWLIST_ERROR_MSG =
            "Authentication request must have non-empty allowList";
    static final String NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG =
            "Request doesn't have a valid list of allowed credentials.";
    static final String NO_SCREENLOCK_ERROR_MSG = "The device is not secured with any screen lock";
    static final String CREDENTIAL_EXISTS_ERROR_MSG =
            "One of the excluded credentials exists on the local device";
    static final String LOW_LEVEL_ERROR_MSG = "Low level error 0x6a80";
    static final String CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_USER_CANCEL =
            "android.credentials.CreateCredentialException.TYPE_USER_CANCELED";
    static final String CRED_MAN_EXCEPTION_GET_CREDENTIAL_TYPE_USER_CANCEL =
            "android.credentials.GetCredentialException.TYPE_USER_CANCELED";
    public static final int GMSCORE_MIN_VERSION_HYBRID_API = 231206000;

    private static Boolean sIsCredManEnabled;

    private final WebAuthenticationDelegate.IntentSender mIntentSender;
    private GetAssertionResponseCallback mGetAssertionCallback;
    private MakeCredentialResponseCallback mMakeCredentialCallback;
    private FidoErrorResponseCallback mErrorCallback;
    private WebContents mWebContents;
    private RenderFrameHost mFrameHost;
    private boolean mAppIdExtensionUsed;
    private boolean mEchoCredProps;
    private WebAuthnBrowserBridge mBrowserBridge;
    private Object mCredentialManagerServiceForTesting;
    private Class mCredManCreateRequestBuilderClassForTesting;
    private Class mCredManGetRequestBuilderClassForTesting;
    private Class mCredManCredentialOptionBuilderClassForTesting;
    private boolean mAttestationAcceptable;
    private boolean mIsCrossOrigin;
    private boolean mOverrideVersionCheckForTesting;

    private enum ConditionalUiState {
        NONE,
        WAITING_FOR_CREDENTIAL_LIST,
        WAITING_FOR_SELECTION,
        REQUEST_SENT_TO_PLATFORM,
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
    public Fido2CredentialRequest(WebAuthenticationDelegate.IntentSender intentSender) {
        mIntentSender = intentSender;
    }

    private void returnErrorAndResetCallback(int error) {
        assert mErrorCallback != null;
        if (mErrorCallback == null) return;
        mErrorCallback.onError(error);
        mErrorCallback = null;
        mGetAssertionCallback = null;
        mMakeCredentialCallback = null;
    }

    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    private boolean isCredManEnabled() {
        if (sIsCredManEnabled == null) {
            sIsCredManEnabled =
                    DeviceFeatureMap.isEnabled(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN)
                    && (BuildCompat.isAtLeastU() || mOverrideVersionCheckForTesting);
        }
        return sIsCredManEnabled;
    }

    @SuppressWarnings("NewApi")
    public void handleMakeCredentialRequest(PublicKeyCredentialCreationOptions options,
            RenderFrameHost frameHost, Origin origin, MakeCredentialResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        assert mMakeCredentialCallback == null && mErrorCallback == null;
        mMakeCredentialCallback = callback;
        mErrorCallback = errorCallback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }
        mFrameHost = frameHost;

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
        final boolean rkDiscouraged = options.authenticatorSelection == null
                || options.authenticatorSelection.residentKey == ResidentKeyRequirement.DISCOURAGED;
        mAttestationAcceptable = rkDiscouraged;
        mEchoCredProps = options.credProps;

        // residentKey=discouraged requests are often for the traditional,
        // non-syncing platform authenticator on Android. A number of sites use
        // this and, so as not to disrupt them with Android 14, these requests
        // continue to be sent directly to Play Services.
        //
        // Otherwise these requests are for security keys, and Play Services is
        // currently the best answer for those requests too.
        if (!rkDiscouraged && isCredManEnabled()) {
            makeCredentialViaCredMan(options, origin);
            return;
        }

        if (!apiAvailable()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        try {
            Fido2ApiCallHelper.getInstance().invokeFido2MakeCredential(options,
                    Uri.parse(convertOriginToString(origin)), /* clientDataHash= */ null,
                    this::onGotPendingIntent, this::onBinderCallException);
        } catch (NoSuchAlgorithmException e) {
            returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
            return;
        }
    }

    private void onBinderCallException(Exception e) {
        Log.e(TAG, "FIDO2 API call failed", e);
        returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
    }

    @SuppressWarnings("NewApi")
    public void handleGetAssertionRequest(PublicKeyCredentialRequestOptions options,
            RenderFrameHost frameHost, Origin callerOrigin, PaymentOptions payment,
            GetAssertionResponseCallback callback, FidoErrorResponseCallback errorCallback) {
        assert mGetAssertionCallback == null && mErrorCallback == null;
        mGetAssertionCallback = callback;
        mErrorCallback = errorCallback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }
        mFrameHost = frameHost;

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
            options.extensions.userVerificationMethods = false;
        }

        if (options.extensions.appid != null) {
            mAppIdExtensionUsed = true;
        }

        String callerOriginString = convertOriginToString(callerOrigin);
        byte[] clientDataHash = null;

        // Payments should still go through Google Play Services.
        if (payment == null && isCredManEnabled()) {
            if (options.isConditional) {
                prefetchCredentialsViaCredMan(
                        options, callerOrigin, callerOriginString, clientDataHash);
            } else {
                getCredentialViaCredMan(options, callerOrigin, /*requestPasswords=*/false);
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
                || (ContentFeatureMap.isEnabled(
                            ContentFeatures.WEB_AUTHN_TOUCH_TO_FILL_CREDENTIAL_SELECTION)
                        && !hasAllowCredentials)) {
            // For use in the lambda expression.
            final byte[] finalClientDataHash = clientDataHash;
            mConditionalUiState = ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST;
            Fido2ApiCallHelper.getInstance().invokeFido2GetCredentials(options.relyingPartyId,
                    (credentials)
                            -> onWebAuthnCredentialDetailsListReceived(
                                    options, callerOriginString, finalClientDataHash, credentials),
                    this::onBinderCallException);
            return;
        }

        maybeDispatchGetAssertionRequest(options, callerOriginString, clientDataHash, null);
    }

    public void cancelConditionalGetAssertion(RenderFrameHost frameHost) {
        switch (mConditionalUiState) {
            case WAITING_FOR_CREDENTIAL_LIST:
                mConditionalUiState = ConditionalUiState.CANCEL_PENDING;
                returnErrorAndResetCallback(AuthenticatorStatus.ABORT_ERROR);
                break;
            case WAITING_FOR_SELECTION:
                mBrowserBridge.cleanupRequest(frameHost);
                mConditionalUiState = ConditionalUiState.NONE;
                returnErrorAndResetCallback(AuthenticatorStatus.ABORT_ERROR);
                break;
            case REQUEST_SENT_TO_PLATFORM:
                // If the platform successfully completes the getAssertion then cancelation is
                // ignored, but if it returns an error then CANCEL_PENDING removes the option to
                // try again.
                mConditionalUiState = ConditionalUiState.CANCEL_PENDING;
                break;
            default:
                // No action
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

        Fido2ApiCall call = new Fido2ApiCall(ContextUtils.getApplicationContext());
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

        Fido2ApiCallHelper.getInstance().invokeFido2GetCredentials(relyingPartyId,
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

    @VisibleForTesting
    public void setOverrideVersionCheckForTesting(boolean override) {
        mOverrideVersionCheckForTesting = override;
    }

    @VisibleForTesting
    public void setCredManClassesForTesting(Object credentialManager, Class createRequestBuilder,
            Class getRequestBuilder, Class credentialOptionBuilder) {
        mCredentialManagerServiceForTesting = credentialManager;
        mCredManCreateRequestBuilderClassForTesting = createRequestBuilder;
        mCredManGetRequestBuilderClassForTesting = getRequestBuilder;
        mCredManCredentialOptionBuilderClassForTesting = credentialOptionBuilder;
    }

    private boolean apiAvailable() {
        return Fido2ApiCallHelper.getInstance().arePlayServicesAvailable();
    }

    private void onWebAuthnCredentialDetailsListReceived(PublicKeyCredentialRequestOptions options,
            String callerOriginString, byte[] clientDataHash,
            List<WebAuthnCredentialDetails> credentials) {
        assert mConditionalUiState == ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST
                || mConditionalUiState == ConditionalUiState.CANCEL_PENDING;

        boolean hasAllowCredentials =
                options.allowCredentials != null && options.allowCredentials.length != 0;
        boolean isConditionalRequest = options.isConditional;
        assert isConditionalRequest || !hasAllowCredentials;

        if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
            // The request was completed synchronously when the cancellation was received,
            // so no need to return an error to the renderer.
            mConditionalUiState = ConditionalUiState.NONE;
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
            // through to GMSCore. It will show an error message to the user, but can offer the
            // user alternatives to use external passkeys.
            maybeDispatchGetAssertionRequest(options, callerOriginString, clientDataHash, null);
            return;
        }

        if (mBrowserBridge == null) {
            mBrowserBridge = new WebAuthnBrowserBridge();
        }

        Runnable hybridCallback = null;
        if (isHybridClientApiAvailable()) {
            hybridCallback = ()
                    -> dispatchHybridGetAssertionRequest(
                            options, callerOriginString, clientDataHash);
        }

        mConditionalUiState = ConditionalUiState.WAITING_FOR_SELECTION;
        mBrowserBridge.onCredentialsDetailsListReceived(mFrameHost, discoverableCredentials,
                isConditionalRequest,
                (selectedCredentialId)
                        -> maybeDispatchGetAssertionRequest(
                                options, callerOriginString, clientDataHash, selectedCredentialId),
                hybridCallback);
    }

    private void maybeDispatchGetAssertionRequest(PublicKeyCredentialRequestOptions options,
            String callerOriginString, byte[] clientDataHash, byte[] credentialId) {
        assert mConditionalUiState == ConditionalUiState.NONE
                || mConditionalUiState == ConditionalUiState.REQUEST_SENT_TO_PLATFORM
                || mConditionalUiState == ConditionalUiState.WAITING_FOR_SELECTION;

        // If this is called a second time while the first sign-in attempt is still outstanding,
        // ignore the second call.
        if (mConditionalUiState == ConditionalUiState.REQUEST_SENT_TO_PLATFORM) {
            Log.e(TAG, "Received a second credential selection while the first still in progress.");
            return;
        }

        mConditionalUiState = ConditionalUiState.NONE;
        if (credentialId != null) {
            if (credentialId.length == 0) {
                if (options.isConditional) {
                    // An empty credential ID means an error from native code, which can happen if
                    // the embedder does not support Conditional UI.
                    Log.e(TAG, "Empty credential ID from account selection.");
                    mBrowserBridge.cleanupRequest(mFrameHost);
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

        if (options.isConditional) {
            mConditionalUiState = ConditionalUiState.REQUEST_SENT_TO_PLATFORM;
        }

        Fido2ApiCallHelper.getInstance().invokeFido2GetAssertion(options,
                Uri.parse(callerOriginString), clientDataHash, this::onGotPendingIntent,
                this::onBinderCallException);
    }

    private void dispatchHybridGetAssertionRequest(PublicKeyCredentialRequestOptions options,
            String callerOriginString, byte[] clientDataHash) {
        assert mConditionalUiState == ConditionalUiState.NONE
                || mConditionalUiState == ConditionalUiState.REQUEST_SENT_TO_PLATFORM
                || mConditionalUiState == ConditionalUiState.WAITING_FOR_SELECTION;

        if (mConditionalUiState == ConditionalUiState.REQUEST_SENT_TO_PLATFORM) {
            Log.e(TAG, "Received a second credential selection while the first still in progress.");
            return;
        }
        mConditionalUiState = ConditionalUiState.REQUEST_SENT_TO_PLATFORM;

        Fido2ApiCall call = new Fido2ApiCall(ContextUtils.getApplicationContext());
        Parcel args = call.start();
        Fido2ApiCall.PendingIntentResult result = new Fido2ApiCall.PendingIntentResult();
        args.writeStrongBinder(result);
        args.writeInt(1); // This indicates that the following options are present.
        Fido2Api.appendBrowserGetAssertionOptionsToParcel(
                options, Uri.parse(callerOriginString), clientDataHash, /*tunnelId=*/null, args);
        Task<PendingIntent> task = call.run(Fido2ApiCall.METHOD_BROWSER_HYBRID_SIGN,
                Fido2ApiCall.TRANSACTION_HYBRID_SIGN, args, result);
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

        assert mConditionalUiState == ConditionalUiState.NONE
                || mConditionalUiState == ConditionalUiState.REQUEST_SENT_TO_PLATFORM
                || mConditionalUiState == ConditionalUiState.CANCEL_PENDING;

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

        if (mConditionalUiState != ConditionalUiState.NONE) {
            if (response == null || response instanceof Pair) {
                if (response != null) {
                    Pair<Integer, String> error = (Pair<Integer, String>) response;
                    Log.e(TAG,
                            "FIDO2 API call resulted in error: " + error.first + " "
                                    + (error.second != null ? error.second : ""));
                    errorCode = convertError(error);
                }

                if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                    mConditionalUiState = ConditionalUiState.NONE;
                    mBrowserBridge.cleanupRequest(mFrameHost);
                    returnErrorAndResetCallback(AuthenticatorStatus.ABORT_ERROR);
                } else {
                    // The user can try again by selecting another conditional UI credential.
                    mConditionalUiState = ConditionalUiState.WAITING_FOR_SELECTION;
                }
                return;
            }
            mConditionalUiState = ConditionalUiState.NONE;
            mBrowserBridge.cleanupRequest(mFrameHost);
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
    public static String convertOriginToString(Origin origin) {
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

    @SuppressWarnings("WrongConstant")
    Object credentialManagerService(Context context) {
        if (mCredentialManagerServiceForTesting != null) {
            return mCredentialManagerServiceForTesting;
        }
        // TODO: switch "credential" to `Context.CREDENTIAL_SERVICE` and remove the
        // `@SuppressWarnings` when the Android U SDK is available.
        return context.getSystemService("credential");
    }

    Class credManCreateRequestBuilderClass() throws ClassNotFoundException {
        if (mCredManCreateRequestBuilderClassForTesting != null) {
            return mCredManCreateRequestBuilderClassForTesting;
        }
        return Class.forName("android.credentials.CreateCredentialRequest$Builder");
    }

    Class credManGetRequestBuilderClass() throws ClassNotFoundException {
        if (mCredManGetRequestBuilderClassForTesting != null) {
            return mCredManGetRequestBuilderClassForTesting;
        }
        return Class.forName("android.credentials.GetCredentialRequest$Builder");
    }

    Class credManCredentialOptionBuilderClass() throws ClassNotFoundException {
        if (mCredManCredentialOptionBuilderClassForTesting != null) {
            return mCredManCredentialOptionBuilderClassForTesting;
        }
        return Class.forName("android.credentials.CredentialOption$Builder");
    }

    /**
     * Create a credential using the Android 14 CredMan API.
     * TODO: update the version code to U when Chromium builds with Android 14 SDK.
     */
    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    private void makeCredentialViaCredMan(
            PublicKeyCredentialCreationOptions options, Origin origin) {
        final String requestAsJson =
                Fido2CredentialRequestJni.get().createOptionsToJson(options.serialize());
        final Context context = getContext();

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
        requestBundle.putByteArray(CRED_MAN_PREFIX + "BUNDLE_KEY_CLIENT_DATA_HASH", clientDataHash);
        requestBundle.putBoolean(
                CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS", false);

        final Bundle displayInfoBundle = new Bundle();
        displayInfoBundle.putCharSequence(CRED_MAN_PREFIX + "BUNDLE_KEY_USER_ID",
                Base64.encodeToString(
                        options.user.id, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP));
        displayInfoBundle.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_DEFAULT_PROVIDER",
                GPM_COMPONENT_NAME.flattenToString());

        requestBundle.putBundle(
                CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_DISPLAY_INFO", displayInfoBundle);
        requestBundle.putString(CHANNEL_KEY, getChannel());

        // The Android 14 APIs have to be called via reflection until Chromium
        // builds with the Android 14 SDK by default.
        OutcomeReceiver receiver = new OutcomeReceiver<Object, Throwable>() {
            @Override
            public void onError(Throwable e) {
                String errorType = getCredManExceptionType(e);
                Log.e(TAG, "CredMan CreateCredential call failed: %s",
                        errorType + " (" + e.getMessage() + ")");
                if (errorType.equals(CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_USER_CANCEL)) {
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
                    Log.e(TAG, "Failed to convert response from CredMan to Mojo object: %s", json);
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
            final Class createCredentialRequestBuilder = credManCreateRequestBuilderClass();
            final Object builder = createCredentialRequestBuilder
                                           .getConstructor(String.class, Bundle.class, Bundle.class)
                                           .newInstance(TYPE_PASSKEY, requestBundle, requestBundle);
            final Class builderClass = builder.getClass();
            builderClass.getMethod("setAlwaysSendAppInfoToProvider", boolean.class)
                    .invoke(builder, true);
            builderClass.getMethod("setOrigin", String.class)
                    .invoke(builder, convertOriginToString(origin));
            final Object request = builderClass.getMethod("build").invoke(builder);
            final Object manager = credentialManagerService(context);
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
                final Activity activity = WebContentsStatics.fromRenderFrameHost(mFrameHost)
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
    private void getCredentialViaCredMan(
            PublicKeyCredentialRequestOptions options, Origin origin, boolean requestPasswords) {
        final Context context = getContext();

        // The Android 14 APIs have to be called via reflection until Chromium
        // builds with the Android 14 SDK by default.
        OutcomeReceiver<Object, Throwable> receiver = new OutcomeReceiver<>() {
            @Override
            public void onError(Throwable getCredentialException) {
                String errorType = getCredManExceptionType(getCredentialException);
                Log.e(TAG, "CredMan getCredential call failed: %s",
                        errorType + " (" + getCredentialException.getMessage() + ")");
                notifyBrowserOnCredManClosed(false);
                if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                    mConditionalUiState = ConditionalUiState.NONE;
                    mBrowserBridge.cleanupRequest(mFrameHost);
                    returnErrorAndResetCallback(AuthenticatorStatus.ABORT_ERROR);
                    return;
                }
                if (errorType.equals(CRED_MAN_EXCEPTION_GET_CREDENTIAL_TYPE_USER_CANCEL)) {
                    if (mConditionalUiState == ConditionalUiState.NONE) {
                        returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                    }
                } else {
                    // Includes:
                    //  * GetCredentialException.TYPE_UNKNOWN
                    //  * GetCredentialException.TYPE_NO_CREATE_OPTIONS
                    //  * GetCredentialException.TYPE_INTERRUPTED
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                }
                mConditionalUiState = options.isConditional
                        ? ConditionalUiState.WAITING_FOR_SELECTION
                        : ConditionalUiState.NONE;
            }

            @Override
            public void onResult(Object getCredentialResponse) {
                if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                    notifyBrowserOnCredManClosed(false);
                    mConditionalUiState = ConditionalUiState.NONE;
                    mBrowserBridge.cleanupRequest(mFrameHost);
                    returnErrorAndResetCallback(AuthenticatorStatus.ABORT_ERROR);
                    return;
                }
                Bundle data;
                String type;
                try {
                    Object credential = getCredentialResponse.getClass()
                                                .getMethod("getCredential")
                                                .invoke(getCredentialResponse);
                    data = (Bundle) credential.getClass().getMethod("getData").invoke(credential);
                    type = (String) credential.getClass().getMethod("getType").invoke(credential);

                } catch (ReflectiveOperationException e) {
                    Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
                    mConditionalUiState = options.isConditional
                            ? ConditionalUiState.WAITING_FOR_SELECTION
                            : ConditionalUiState.NONE;
                    notifyBrowserOnCredManClosed(false);
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }

                if (!TYPE_PASSKEY.equals(type)) {
                    mBrowserBridge.onPasswordCredentialReceived(mFrameHost,
                            data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_ID"),
                            data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_PASSWORD"));
                    return;
                }

                String json =
                        data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_AUTHENTICATION_RESPONSE_JSON");
                byte[] responseSerialized =
                        Fido2CredentialRequestJni.get().getCredentialResponseFromJson(json);
                if (responseSerialized == null) {
                    Log.e(TAG, "Failed to convert response from CredMan to Mojo object: %s", json);
                    mConditionalUiState = options.isConditional
                            ? ConditionalUiState.WAITING_FOR_SELECTION
                            : ConditionalUiState.NONE;
                    notifyBrowserOnCredManClosed(false);
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }

                GetAssertionAuthenticatorResponse response =
                        GetAssertionAuthenticatorResponse.deserialize(
                                ByteBuffer.wrap(responseSerialized));
                if (response == null) {
                    Log.e(TAG, "Failed to parse Mojo object");
                    mConditionalUiState = options.isConditional
                            ? ConditionalUiState.WAITING_FOR_SELECTION
                            : ConditionalUiState.NONE;
                    notifyBrowserOnCredManClosed(false);
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                response.info.clientDataJson = mClientDataJson.getBytes();
                if (mAppIdExtensionUsed) {
                    response.echoAppidExtension = mAppIdExtensionUsed;
                }
                mConditionalUiState = options.isConditional
                        ? ConditionalUiState.WAITING_FOR_SELECTION
                        : ConditionalUiState.NONE;
                notifyBrowserOnCredManClosed(true);
                mGetAssertionCallback.onSignResponse(AuthenticatorStatus.SUCCESS, response);
                mGetAssertionCallback = null;
            }
        };

        if (mConditionalUiState == ConditionalUiState.REQUEST_SENT_TO_PLATFORM) {
            Log.e(TAG, "Received a second credential selection while the first still in progress.");
            return;
        }
        mConditionalUiState = options.isConditional ? ConditionalUiState.REQUEST_SENT_TO_PLATFORM
                                                    : ConditionalUiState.NONE;
        try {
            final Object getCredentialRequest =
                    buildGetCredentialRequest(options, origin, requestPasswords);
            if (getCredentialRequest == null) {
                mConditionalUiState = options.isConditional
                        ? ConditionalUiState.WAITING_FOR_SELECTION
                        : ConditionalUiState.NONE;
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }
            final Object manager = credentialManagerService(context);
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
                final Activity activity = WebContentsStatics.fromRenderFrameHost(mFrameHost)
                                                  .getTopLevelNativeWindow()
                                                  .getActivity()
                                                  .get();
                if (activity == null) {
                    mConditionalUiState = options.isConditional
                            ? ConditionalUiState.WAITING_FOR_SELECTION
                            : ConditionalUiState.NONE;
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
            mConditionalUiState = options.isConditional ? ConditionalUiState.WAITING_FOR_SELECTION
                                                        : ConditionalUiState.NONE;
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }
    }

    /**
     * Queries credential availability using the Android 14 CredMan API.
     * TODO: update the version code to U when Chromium builds with Android 14 SDK.
     */
    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    private void prefetchCredentialsViaCredMan(PublicKeyCredentialRequestOptions options,
            Origin origin, String callerOriginString, byte[] clientDataHash) {
        final Context context = getContext();

        // The Android 14 APIs have to be called via reflection until Chromium
        // builds with the Android 14 SDK by default.
        OutcomeReceiver<Object, Throwable> receiver = new OutcomeReceiver<>() {
            @Override
            public void onError(Throwable e) {
                assert mConditionalUiState == ConditionalUiState.NONE
                        || mConditionalUiState == ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST
                        || mConditionalUiState == ConditionalUiState.CANCEL_PENDING;
                // prepareGetCredential uses getCredentialException, but it cannot be user
                // cancelled so all errors map to UNKNOWN_ERROR.
                Log.e(TAG, "CredMan prepareGetCredential call failed: %s",
                        getCredManExceptionType(e) + " (" + e.getMessage() + ")");
                mConditionalUiState = ConditionalUiState.NONE;
                returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            }

            @Override
            public void onResult(Object prepareGetCredentialResponse) {
                if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                    // The request was completed synchronously when the cancellation was received.
                    mConditionalUiState = ConditionalUiState.NONE;
                    mBrowserBridge.cleanupRequest(mFrameHost);
                    return;
                }
                assert mConditionalUiState == ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST;
                boolean hasPublicKeyCredentials;
                try {
                    Method hasCredentialResultsMethod =
                            prepareGetCredentialResponse.getClass().getMethod(
                                    "hasCredentialResults", String.class);
                    hasPublicKeyCredentials = (Boolean) hasCredentialResultsMethod.invoke(
                            prepareGetCredentialResponse, TYPE_PASSKEY);
                } catch (ReflectiveOperationException e) {
                    Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
                    mConditionalUiState = ConditionalUiState.NONE;
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }

                if (mBrowserBridge == null) {
                    mBrowserBridge = new WebAuthnBrowserBridge();
                };
                mConditionalUiState = ConditionalUiState.WAITING_FOR_SELECTION;
                mBrowserBridge.onCredManConditionalRequestPending(mFrameHost,
                        hasPublicKeyCredentials,
                        (requestPasswords)
                                -> getCredentialViaCredMan(options, origin, requestPasswords));
            }
        };

        try {
            mConditionalUiState = ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST;
            final Object getCredentialRequest =
                    buildGetCredentialRequest(options, origin, /*requestPasswords=*/false);
            if (getCredentialRequest == null) {
                mConditionalUiState = ConditionalUiState.NONE;
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }

            final Object manager = credentialManagerService(context);
            manager.getClass()
                    .getMethod("prepareGetCredential", getCredentialRequest.getClass(),
                            android.os.CancellationSignal.class,
                            java.util.concurrent.Executor.class, OutcomeReceiver.class)
                    .invoke(manager, getCredentialRequest, null, context.getMainExecutor(),
                            receiver);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
            mConditionalUiState = ConditionalUiState.NONE;
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }
    }

    private Object buildGetCredentialRequest(PublicKeyCredentialRequestOptions options,
            Origin origin, boolean requestPasswords) throws ReflectiveOperationException {
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

        Bundle publicKeyCredentialOptionBundle =
                buildPublicKeyCredentialOptionBundle(requestAsJson, clientDataHash);

        // Build the CredentialOption for passkeys:
        Object credentialOption;
        try {
            final Class<?> credentialOptionBuilderClass = credManCredentialOptionBuilderClass();
            final Object credentialOptionBuilder =
                    credentialOptionBuilderClass
                            .getConstructor(String.class, Bundle.class, Bundle.class)
                            .newInstance(TYPE_PASSKEY, publicKeyCredentialOptionBundle,
                                    publicKeyCredentialOptionBundle);
            credentialOption =
                    credentialOptionBuilderClass.getMethod("build").invoke(credentialOptionBuilder);
        } catch (ClassNotFoundException e) {
            // In order to be compatible with Android 14 Beta 1, the older
            // form of the call is also tried.
            credentialOption =
                    Class.forName("android.credentials.CredentialOption")
                            .getConstructor(String.class, Bundle.class, Bundle.class, Boolean.TYPE)
                            .newInstance(TYPE_PASSKEY, publicKeyCredentialOptionBundle,
                                    publicKeyCredentialOptionBundle, false);
        }

        // Build the GetCredentialRequest:
        final Class<?> getCredentialRequestBuilderClass = credManGetRequestBuilderClass();
        Bundle getCredentialRequestBundle = new Bundle();
        getCredentialRequestBundle.putParcelable(
                CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_UI_BRANDING_COMPONENT_NAME",
                GPM_COMPONENT_NAME);
        final Object getCredentialRequestBuilderObject =
                getCredentialRequestBuilderClass.getConstructor(Bundle.class)
                        .newInstance(getCredentialRequestBundle);
        getCredentialRequestBuilderClass
                .getMethod("addCredentialOption", credentialOption.getClass())
                .invoke(getCredentialRequestBuilderObject, credentialOption);
        if (requestPasswords) {
            Object passwordCredentialOption = buildPasswordOption();
            if (passwordCredentialOption != null) {
                getCredentialRequestBuilderClass
                        .getMethod("addCredentialOption", passwordCredentialOption.getClass())
                        .invoke(getCredentialRequestBuilderObject, passwordCredentialOption);
            }
        }
        getCredentialRequestBuilderClass.getMethod("setOrigin", String.class)
                .invoke(getCredentialRequestBuilderObject, convertOriginToString(origin));
        return getCredentialRequestBuilderClass.getMethod("build").invoke(
                getCredentialRequestBuilderObject);
    }

    private Bundle buildPublicKeyCredentialOptionBundle(
            String requestAsJson, byte[] clientDataHash) {
        final Bundle publicKeyCredentialOptionBundle = new Bundle();
        publicKeyCredentialOptionBundle.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_SUBTYPE",
                CRED_MAN_PREFIX + "BUNDLE_VALUE_SUBTYPE_GET_PUBLIC_KEY_CREDENTIAL_OPTION");
        publicKeyCredentialOptionBundle.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_JSON", requestAsJson);
        publicKeyCredentialOptionBundle.putByteArray(
                CRED_MAN_PREFIX + "BUNDLE_KEY_CLIENT_DATA_HASH", clientDataHash);
        publicKeyCredentialOptionBundle.putBoolean(
                CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS", false);
        publicKeyCredentialOptionBundle.putString(CHANNEL_KEY, getChannel());
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

    private void notifyBrowserOnCredManClosed(boolean success) {
        if (mConditionalUiState == ConditionalUiState.NONE) return;
        mBrowserBridge.onCredManUiClosed(mFrameHost, success);
    }

    private boolean isHybridClientApiAvailable() {
        return PackageUtils.getPackageVersion("com.google.android.gms")
                >= GMSCORE_MIN_VERSION_HYBRID_API
                && DeviceFeatureMap.isEnabled(DeviceFeatureList.WEBAUTHN_ANDROID_HYBRID_CLIENT_UI);
    }

    private static final String getChannel() {
        if (VersionInfo.isCanaryBuild()) {
            return "canary";
        }
        if (VersionInfo.isDevBuild()) {
            return "dev";
        }
        if (VersionInfo.isBetaBuild()) {
            return "beta";
        }
        if (VersionInfo.isStableBuild()) {
            return "stable";
        }
        if (VersionInfo.isLocalBuild()) {
            return "built_locally";
        }
        assert false : "Channel must be canary, dev, beta, stable or chrome must be built locally.";
        return null;
    }

    private Object buildPasswordOption() throws ReflectiveOperationException {
        Object passwordCredentialOption;
        Bundle passwordOptionBundle = new Bundle();
        passwordOptionBundle.putString(CHANNEL_KEY, getChannel());

        try {
            final Class<?> credentialOptionBuilderClass = credManCredentialOptionBuilderClass();
            final Object credentialOptionBuilder =
                    credentialOptionBuilderClass
                            .getConstructor(String.class, Bundle.class, Bundle.class)
                            .newInstance("android.credentials.TYPE_PASSWORD_CREDENTIAL",
                                    passwordOptionBundle, passwordOptionBundle);
            credentialOptionBuilderClass.getMethod("setAllowedProviders", Set.class)
                    .invoke(credentialOptionBuilder, Set.of(GPM_COMPONENT_NAME));
            passwordCredentialOption =
                    credentialOptionBuilderClass.getMethod("build").invoke(credentialOptionBuilder);

        } catch (ClassNotFoundException e) {
            // In order to be compatible with Android 14 Beta 1, the older
            // form of the call is also tried.
            passwordCredentialOption =
                    Class.forName("android.credentials.CredentialOption")
                            .getConstructor(String.class, Bundle.class, Bundle.class, Boolean.TYPE)
                            .newInstance("android.credentials.TYPE_PASSWORD_CREDENTIAL",
                                    passwordOptionBundle, passwordOptionBundle, false);
        }
        return passwordCredentialOption;
    }

    private Context getContext() {
        if (mWebContents == null) {
            return ContextUtils.getApplicationContext();
        }
        final Activity activity = mWebContents.getTopLevelNativeWindow().getActivity().get();
        return activity == null ? ContextUtils.getApplicationContext() : activity;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        String createOptionsToJson(ByteBuffer serializedOptions);
        byte[] makeCredentialResponseFromJson(String json);
        String getOptionsToJson(ByteBuffer serializedOptions);
        byte[] getCredentialResponseFromJson(String json);
    }
}
