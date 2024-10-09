// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.components.webauthn.WebauthnModeProvider.is;
import static org.chromium.components.webauthn.WebauthnModeProvider.isChrome;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Parcel;
import android.os.ResultReceiver;
import android.os.SystemClock;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.tasks.Task;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
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
import org.chromium.components.webauthn.Fido2ApiCall.Fido2ApiCallParams;
import org.chromium.components.webauthn.cred_man.CredManHelper;
import org.chromium.components.webauthn.cred_man.CredManSupportProvider;
import org.chromium.content_public.browser.ClientDataJson;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.GURLUtils;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Uses the Google Play Services Fido2 APIs. Holds the logic of each request. */
@JNINamespace("webauthn")
public class Fido2CredentialRequest
        implements Callback<Pair<Integer, Intent>>, WebauthnBrowserBridge.Provider {
    private static final String TAG = "Fido2Request";
    static final String NON_EMPTY_ALLOWLIST_ERROR_MSG =
            "Authentication request must have non-empty allowList";
    static final String NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG =
            "Request doesn't have a valid list of allowed credentials.";
    static final String NO_SCREENLOCK_ERROR_MSG = "The device is not secured with any screen lock";
    static final String CREDENTIAL_EXISTS_ERROR_MSG =
            "One of the excluded credentials exists on the local device";
    static final String LOW_LEVEL_ERROR_MSG = "Low level error 0x6a80";

    // mPlayServicesAvailable caches whether the Play Services FIDO API is
    // available.
    private final boolean mPlayServicesAvailable;
    private final AuthenticationContextProvider mAuthenticationContextProvider;
    private GetAssertionResponseCallback mGetAssertionCallback;
    private MakeCredentialResponseCallback mMakeCredentialCallback;
    private FidoErrorResponseCallback mErrorCallback;
    private CredManHelper mCredManHelper;
    private Barrier mBarrier;
    // mFrameHost is null in makeCredential requests. For getAssertion requests
    // it's non-null for conditional requests and may be non-null in other
    // requests.
    private boolean mAppIdExtensionUsed;
    private boolean mEchoCredProps;
    private WebauthnBrowserBridge mBrowserBridge;
    // mIsHybridRequest is true if this request comes from a hybrid (i.e. cross-device) flow rather
    // than a WebContents. Handling the hybrid protocol can be delegated to Chrome (by Play
    // Services).
    private boolean mIsHybridRequest;

    public enum ConditionalUiState {
        NONE,
        WAITING_FOR_RP_ID_VALIDATION,
        WAITING_FOR_CREDENTIAL_LIST,
        WAITING_FOR_SELECTION,
        REQUEST_SENT_TO_PLATFORM,
        CANCEL_PENDING,
        CANCEL_PENDING_RP_ID_VALIDATION_COMPLETE,
    }

    private ConditionalUiState mConditionalUiState = ConditionalUiState.NONE;

    // Not null when the GMSCore-created ClientDataJson needs to be overridden or when using the
    // CredMan API.
    @Nullable private byte[] mClientDataJson;

    /**
     * Constructs the object.
     *
     * @param intentSender Interface for starting {@link Intent}s from Play Services.
     */
    public Fido2CredentialRequest(AuthenticationContextProvider authenticationContextProvider) {
        mAuthenticationContextProvider = authenticationContextProvider;
        boolean playServicesAvailable;
        try {
            playServicesAvailable = Fido2ApiCallHelper.getInstance().arePlayServicesAvailable();
        } catch (Exception e) {
            playServicesAvailable = false;
        }
        mPlayServicesAvailable = playServicesAvailable;
        mCredManHelper =
                new CredManHelper(mAuthenticationContextProvider, this, mPlayServicesAvailable);
        mBarrier = new Barrier(this::returnErrorAndResetCallback);
    }

    private void returnErrorAndResetCallback(int error) {
        assert mErrorCallback != null;
        if (mErrorCallback == null) return;
        mErrorCallback.onError(error);
        mErrorCallback = null;
        mGetAssertionCallback = null;
        mMakeCredentialCallback = null;
    }

    private Barrier.Mode getBarrierMode() {
        @CredManSupport int support = CredManSupportProvider.getCredManSupport();
        if (support != CredManSupport.DISABLED && mIsHybridRequest) {
            return Barrier.Mode.ONLY_CRED_MAN;
        }
        switch (support) {
            case CredManSupport.DISABLED:
                return Barrier.Mode.ONLY_FIDO_2_API;
            case CredManSupport.IF_REQUIRED:
                if (mIsHybridRequest) {
                    return Barrier.Mode.ONLY_CRED_MAN;
                }
                return Barrier.Mode.ONLY_FIDO_2_API;
            case CredManSupport.FULL_UNLESS_INAPPLICABLE:
                return Barrier.Mode.ONLY_CRED_MAN;
            case CredManSupport.PARALLEL_WITH_FIDO_2:
                return Barrier.Mode.BOTH;
        }
        assert support == CredManSupport.NOT_EVALUATED : "All `CredManMode`s must be handled!";
        return Barrier.Mode.ONLY_FIDO_2_API;
    }

    /**
     * Process a WebAuthn create() request.
     *
     * @param context The context used for both Play Services and CredMan calls.
     * @param options The arguments to create()
     * @param maybeClientDataHash The SHA-256 of the ClientDataJSON. Must be non-null iff frameHost
     *     from mAuthenticationContextProvider.frameHost is null.
     * @param maybeBrowserOptions Optional set of browser-specific data, like channel or incognito.
     * @param origin The origin that made the WebAuthn call.
     * @param topOrigin The origin of the main frame.
     * @param callback Success callback.
     * @param errorCallback Failure callback.
     */
    @SuppressWarnings("NewApi")
    public void handleMakeCredentialRequest(
            PublicKeyCredentialCreationOptions options,
            byte[] maybeClientDataHash,
            Bundle maybeBrowserOptions,
            Origin origin,
            Origin topOrigin,
            MakeCredentialResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        assert (frameHost != null) ^ (maybeClientDataHash != null);
        assert mMakeCredentialCallback == null && mErrorCallback == null;
        mMakeCredentialCallback = callback;
        mErrorCallback = errorCallback;

        if (frameHost != null) {
            frameHost.performMakeCredentialWebAuthSecurityChecks(
                    options.relyingParty.id,
                    origin,
                    options.isPaymentCredentialCreation,
                    (result) -> {
                        if (result.securityCheckResult != AuthenticatorStatus.SUCCESS) {
                            returnErrorAndResetCallback(result.securityCheckResult);
                            return;
                        }
                        continueMakeCredentialRequestAfterRpIdValidation(
                                options,
                                maybeClientDataHash,
                                maybeBrowserOptions,
                                origin,
                                topOrigin,
                                result.isCrossOrigin);
                    });
        } else {
            continueMakeCredentialRequestAfterRpIdValidation(
                    options,
                    maybeClientDataHash,
                    maybeBrowserOptions,
                    origin,
                    topOrigin,
                    /* isCrossOrigin= */ false);
        }
    }

    @SuppressWarnings("NewApi")
    private void continueMakeCredentialRequestAfterRpIdValidation(
            PublicKeyCredentialCreationOptions options,
            byte[] maybeClientDataHash,
            Bundle maybeBrowserOptions,
            Origin origin,
            Origin topOrigin,
            boolean isCrossOrigin) {
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        final boolean rkDiscouraged =
                options.authenticatorSelection == null
                        || options.authenticatorSelection.residentKey
                                == ResidentKeyRequirement.DISCOURAGED;
        mEchoCredProps = options.credProps;

        byte[] clientDataHash = maybeClientDataHash;
        if (clientDataHash == null
                && !is(mAuthenticationContextProvider.getWebContents(), WebauthnMode.APP)) {
            assert options.challenge != null;
            final String callerOriginString = convertOriginToString(origin);
            clientDataHash =
                    buildClientDataJsonAndComputeHash(
                            ClientDataRequestType.WEB_AUTHN_CREATE,
                            callerOriginString,
                            options.challenge,
                            isCrossOrigin,
                            /* paymentOptions= */ null,
                            options.relyingParty.name,
                            topOrigin);
            if (clientDataHash == null) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }
        }

        if (!isChrome(mAuthenticationContextProvider.getWebContents())) {
            if (CredManSupportProvider.getCredManSupportForWebView() == CredManSupport.DISABLED) {
                if (!mPlayServicesAvailable) {
                    Log.e(TAG, "Google Play Services' Fido2 API is not available.");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                try {
                    Fido2ApiCallHelper.getInstance()
                            .invokeFido2MakeCredential(
                                    mAuthenticationContextProvider,
                                    options,
                                    Uri.parse(convertOriginToString(origin)),
                                    clientDataHash,
                                    maybeBrowserOptions,
                                    getMaybeResultReceiver(),
                                    this::onGotPendingIntent,
                                    this::onBinderCallException);
                } catch (NoSuchAlgorithmException e) {
                    returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
                    return;
                }
                return;
            }
            int result =
                    mCredManHelper.startMakeRequest(
                            options,
                            convertOriginToString(origin),
                            mClientDataJson,
                            clientDataHash,
                            mMakeCredentialCallback,
                            this::returnErrorAndResetCallback);
            if (result != AuthenticatorStatus.SUCCESS) returnErrorAndResetCallback(result);
            return;
        }

        // If the PRF option is requested over hybrid then send the request
        // directly to Play Services. PRF evaluation points come over hybrid
        // pre-hashed, so it's not possible to send them via CredMan because
        // the JSON form of the PRF extension needs them unhashed. Thus, for
        // getAssertion, PRF requests go directly to Play Services. Because of
        // that, makeCredential requests with PRF are also sent directly to
        // Play Services so that users don't create a credential in a 3rd-party
        // password manager that they cannot then assert.
        final boolean prfOverHybrid = frameHost == null && options.prfEnable;

        // Send requests to Android 14+ CredMan if CredMan is enabled and
        // `gpm_in_cred_man` parameter is enabled.
        //
        // residentKey=discouraged requests are often for the traditional,
        // non-syncing platform authenticator on Android. A number of sites use
        // this and, so as not to disrupt them with Android 14, these requests
        // continue to be sent directly to Play Services.
        //
        // Otherwise these requests are for security keys, and Play Services is
        // currently the best answer for those requests too.
        //
        // Payments requests are also routed to Play Services since we haven't
        // defined how SPC works in CredMan yet.
        if (!rkDiscouraged
                && !options.isPaymentCredentialCreation
                && !prfOverHybrid
                && getBarrierMode() == Barrier.Mode.ONLY_CRED_MAN) {
            int result =
                    mCredManHelper.startMakeRequest(
                            options,
                            convertOriginToString(origin),
                            mClientDataJson,
                            clientDataHash,
                            mMakeCredentialCallback,
                            this::returnErrorAndResetCallback);
            if (result != AuthenticatorStatus.SUCCESS) returnErrorAndResetCallback(result);
            return;
        }

        if (!mPlayServicesAvailable) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        try {
            Fido2ApiCallHelper.getInstance()
                    .invokeFido2MakeCredential(
                            mAuthenticationContextProvider,
                            options,
                            Uri.parse(convertOriginToString(origin)),
                            clientDataHash,
                            maybeBrowserOptions,
                            getMaybeResultReceiver(),
                            this::onGotPendingIntent,
                            this::onBinderCallException);
        } catch (NoSuchAlgorithmException e) {
            returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
            return;
        }
    }

    private void onBinderCallException(Exception e) {
        Log.e(TAG, "FIDO2 API call failed", e);
        returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
    }

    /**
     * Process a WebAuthn get() request.
     *
     * @param options The arguments to get(). If `isConditional` is true then `frameHost` must be
     *     non-null.
     * @param maybeClientDataHash The SHA-256 of the ClientDataJSON. Must be non-null iff frameHost
     *     from mAuthenticationContextProvider.frameHost is null.
     * @param origin The origin that made the WebAuthn call.
     * @param topOrigin The origin of the main frame.
     * @param payment Options for Secure Payment Confirmation. May only be non-null if `frameHost`
     *     is non-null.
     * @param callback Success callback.
     * @param errorCallback Failure callback.
     */
    @SuppressWarnings("NewApi")
    public void handleGetAssertionRequest(
            PublicKeyCredentialRequestOptions options,
            byte[] maybeClientDataHash,
            Origin origin,
            Origin topOrigin,
            PaymentOptions payment,
            GetAssertionResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        assert (frameHost != null) ^ (maybeClientDataHash != null);
        assert payment == null || frameHost != null;
        assert !options.isConditional || frameHost != null;
        assert mGetAssertionCallback == null && mErrorCallback == null;
        mGetAssertionCallback = callback;
        mErrorCallback = errorCallback;

        if (frameHost != null) {
            mConditionalUiState = ConditionalUiState.WAITING_FOR_RP_ID_VALIDATION;
            frameHost.performGetAssertionWebAuthSecurityChecks(
                    options.relyingPartyId,
                    origin,
                    payment != null,
                    (results) -> {
                        if (mConditionalUiState
                                == ConditionalUiState.CANCEL_PENDING_RP_ID_VALIDATION_COMPLETE) {
                            // This request was canceled while waiting for RP ID validation to
                            // complete.
                            returnErrorAndResetCallback(AuthenticatorStatus.ABORT_ERROR);
                            return;
                        }
                        mConditionalUiState = ConditionalUiState.NONE;
                        if (results.securityCheckResult != AuthenticatorStatus.SUCCESS) {
                            returnErrorAndResetCallback(results.securityCheckResult);
                            return;
                        }
                        continueGetAssertionRequestAfterRpIdValidation(
                                options,
                                maybeClientDataHash,
                                origin,
                                topOrigin,
                                payment,
                                results.isCrossOrigin);
                    });
            return;
        }

        continueGetAssertionRequestAfterRpIdValidation(
                options,
                maybeClientDataHash,
                origin,
                topOrigin,
                payment,
                /* isCrossOrigin= */ false);
    }

    @SuppressWarnings("NewApi")
    private void continueGetAssertionRequestAfterRpIdValidation(
            PublicKeyCredentialRequestOptions options,
            byte[] maybeClientDataHash,
            Origin origin,
            Origin topOrigin,
            PaymentOptions payment,
            boolean isCrossOrigin) {
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        boolean hasAllowCredentials =
                options.allowCredentials != null && options.allowCredentials.length != 0;

        if (!hasAllowCredentials) {
            // No UVM support for discoverable credentials.
            options.extensions.userVerificationMethods = false;
        }

        if (options.extensions.appid != null) {
            mAppIdExtensionUsed = true;
        }

        final String callerOriginString = convertOriginToString(origin);
        byte[] clientDataHash = maybeClientDataHash;
        if (clientDataHash == null
                && !is(mAuthenticationContextProvider.getWebContents(), WebauthnMode.APP)) {
            assert options.challenge != null;
            clientDataHash =
                    buildClientDataJsonAndComputeHash(
                            (payment != null)
                                    ? ClientDataRequestType.PAYMENT_GET
                                    : ClientDataRequestType.WEB_AUTHN_GET,
                            callerOriginString,
                            options.challenge,
                            isCrossOrigin,
                            payment,
                            options.relyingPartyId,
                            topOrigin);
            if (clientDataHash == null) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }
        } else {
            assert payment == null;
        }

        if (!isChrome(mAuthenticationContextProvider.getWebContents())) {
            if (options.isConditional) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_IMPLEMENTED);
                return;
            }
            if (CredManSupportProvider.getCredManSupportForWebView() == CredManSupport.DISABLED) {
                if (!mPlayServicesAvailable) {
                    Log.e(TAG, "Google Play Services' Fido2 Api is not available.");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                maybeDispatchGetAssertionRequest(
                        options, callerOriginString, maybeClientDataHash, null);
                return;
            }
            int result =
                    mCredManHelper.startGetRequest(
                            options,
                            callerOriginString,
                            mClientDataJson,
                            clientDataHash,
                            mGetAssertionCallback,
                            this::returnErrorAndResetCallback,
                            /* ignoreGpm= */ false);
            if (result != AuthenticatorStatus.SUCCESS) returnErrorAndResetCallback(result);
            return;
        }

        // Payments should still go through Google Play Services. Also, if the request has
        // pre-hashed PRF inputs then we cannot represent that in JSON and so can only forward to
        // Play Services.
        final byte[] finalClientDataHash = clientDataHash;
        if (payment == null
                && !options.extensions.prfInputsHashed
                && getBarrierMode() == Barrier.Mode.ONLY_CRED_MAN) {
            if (options.isConditional) {
                mBarrier.resetAndSetWaitStatus(Barrier.Mode.ONLY_CRED_MAN);
                mCredManHelper.startPrefetchRequest(
                        options,
                        convertOriginToString(origin),
                        mClientDataJson,
                        clientDataHash,
                        mGetAssertionCallback,
                        this::returnErrorAndResetCallback,
                        mBarrier,
                        /* ignoreGpm= */ false);
            } else if (hasAllowCredentials && mPlayServicesAvailable) {
                // If the allowlist contains non-discoverable credentials then
                // the request needs to be routed directly to Play Services.
                checkForMatchingCredentials(options, origin, clientDataHash);
            } else {
                // WebauthnMode.CHROME_3PP_ENABLED will keep using CredMan's no credentials UI.
                if (is(mAuthenticationContextProvider.getWebContents(), WebauthnMode.CHROME)) {
                    mCredManHelper.setNoCredentialsFallback(
                            () ->
                                    this.maybeDispatchGetAssertionRequest(
                                            options,
                                            convertOriginToString(origin),
                                            finalClientDataHash,
                                            /* credentialId= */ null));
                } else {
                    mCredManHelper.setNoCredentialsFallback(null);
                }
                int response =
                        mCredManHelper.startGetRequest(
                                options,
                                convertOriginToString(origin),
                                mClientDataJson,
                                clientDataHash,
                                mGetAssertionCallback,
                                this::returnErrorAndResetCallback,
                                /* ignoreGpm= */ false);
                if (response != AuthenticatorStatus.SUCCESS) returnErrorAndResetCallback(response);
            }
            return;
        }

        if (!mPlayServicesAvailable) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        // Conditional requests for Chrome 3rd party PWM mode when CredMan not enabled is not
        // defined yet.
        WebContents webContents = mAuthenticationContextProvider.getWebContents();
        if (options.isConditional && is(webContents, WebauthnMode.CHROME_3PP_ENABLED)) {

            returnErrorAndResetCallback(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        // Enumerate credentials from Play Services so that we can show the picker in Chrome UI.
        // Chrome 3rd party mode does not support enumeration in Chrome UI, hence use FIDO 2
        // enumeration for them.
        if (frameHost != null
                && (options.isConditional || !hasAllowCredentials)
                && is(webContents, WebauthnMode.CHROME)) {
            if (getBarrierMode() == Barrier.Mode.BOTH) {
                mBarrier.resetAndSetWaitStatus(Barrier.Mode.BOTH);
                mCredManHelper.startPrefetchRequest(
                        options,
                        callerOriginString,
                        mClientDataJson,
                        clientDataHash,
                        mGetAssertionCallback,
                        this::returnErrorAndResetCallback,
                        mBarrier,
                        /* ignoreGpm= */ true);
            } else {
                mBarrier.resetAndSetWaitStatus(Barrier.Mode.ONLY_FIDO_2_API);
            }
            mConditionalUiState = ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST;
            long conditionalUiCredentialListInitialTimeMs = SystemClock.elapsedRealtime();
            Fido2ApiCallHelper.getInstance()
                    .invokeFido2GetCredentials(
                            mAuthenticationContextProvider,
                            options.relyingPartyId,
                            (credentials) ->
                                    mBarrier.onFido2ApiSuccessful(
                                            () ->
                                                    onWebauthnCredentialDetailsListReceived(
                                                            options,
                                                            callerOriginString,
                                                            finalClientDataHash,
                                                            credentials,
                                                            conditionalUiCredentialListInitialTimeMs)),
                            (e) ->
                                    mBarrier.onFido2ApiFailed(
                                            AuthenticatorStatus.NOT_ALLOWED_ERROR));
            return;
        }

        if (hasAllowCredentials
                && !options.isConditional
                && getBarrierMode() == Barrier.Mode.BOTH) {
            checkForMatchingCredentials(options, origin, clientDataHash);
            return;
        }
        maybeDispatchGetAssertionRequest(options, callerOriginString, clientDataHash, null);
    }

    public void cancelConditionalGetAssertion() {
        mCredManHelper.cancelConditionalGetAssertion();

        switch (mConditionalUiState) {
            case WAITING_FOR_RP_ID_VALIDATION:
                mConditionalUiState = ConditionalUiState.CANCEL_PENDING_RP_ID_VALIDATION_COMPLETE;
                break;
            case WAITING_FOR_CREDENTIAL_LIST:
                mConditionalUiState = ConditionalUiState.CANCEL_PENDING;
                mBarrier.onFido2ApiCancelled();
                break;
            case WAITING_FOR_SELECTION:
                getBridge().cleanupRequest(mAuthenticationContextProvider.getRenderFrameHost());
                mConditionalUiState = ConditionalUiState.NONE;
                mBarrier.onFido2ApiCancelled();
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
            IsUvpaaResponseCallback callback) {
        boolean chromeRequest = isChrome(mAuthenticationContextProvider.getWebContents());
        if ((!chromeRequest
                        && CredManSupportProvider.getCredManSupportForWebView()
                                == CredManSupport.FULL_UNLESS_INAPPLICABLE)
                || (chromeRequest && getBarrierMode() == Barrier.Mode.ONLY_CRED_MAN)) {
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(true);
            return;
        }

        if (!mPlayServicesAvailable) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            // Note that |IsUserVerifyingPlatformAuthenticatorAvailable| only returns
            // true or false, making it unable to handle any error status.
            // So it callbacks with false if Fido2PrivilegedApi is not available.
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
            return;
        }

        Fido2ApiCallParams params =
                WebauthnModeProvider.getInstance()
                        .getFido2ApiCallParams(mAuthenticationContextProvider.getWebContents());
        Fido2ApiCall call = new Fido2ApiCall(mAuthenticationContextProvider.getContext(), params);
        Fido2ApiCall.BooleanResult result = new Fido2ApiCall.BooleanResult();
        Parcel args = call.start();
        args.writeStrongBinder(result);

        Task<Boolean> task =
                call.run(
                        WebauthnModeProvider.getInstance()
                                .getFido2ApiCallParams(
                                        mAuthenticationContextProvider.getWebContents())
                                .mIsUserVerifyingPlatformAuthenticatorAvailableMethodId,
                        Fido2ApiCall.TRANSACTION_ISUVPAA,
                        args,
                        result);
        task.addOnSuccessListener(
                (isUVPAA) -> {
                    callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(isUVPAA);
                });
        task.addOnFailureListener(
                (e) -> {
                    Log.e(TAG, "FIDO2 API call failed", e);
                    callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
                });
    }

    public void handleGetMatchingCredentialIdsRequest(
            String relyingPartyId,
            byte[][] allowCredentialIds,
            boolean requireThirdPartyPayment,
            GetMatchingCredentialIdsResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        assert mErrorCallback == null;
        mErrorCallback = errorCallback;

        if (!mPlayServicesAvailable) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        Fido2ApiCallHelper.getInstance()
                .invokeFido2GetCredentials(
                        mAuthenticationContextProvider,
                        relyingPartyId,
                        (credentials) ->
                                onGetMatchingCredentialIdsListReceived(
                                        credentials,
                                        allowCredentialIds,
                                        requireThirdPartyPayment,
                                        callback),
                        this::onBinderCallException);
    }

    private void onGetMatchingCredentialIdsListReceived(
            List<WebauthnCredentialDetails> retrievedCredentials,
            byte[][] allowCredentialIds,
            boolean requireThirdPartyPayment,
            GetMatchingCredentialIdsResponseCallback callback) {
        List<byte[]> matchingCredentialIds = new ArrayList<>();
        for (WebauthnCredentialDetails credential : retrievedCredentials) {
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

    public void setIsHybridRequest(boolean isHybridRequest) {
        mIsHybridRequest = isHybridRequest;
    }

    public void overrideBrowserBridgeForTesting(WebauthnBrowserBridge bridge) {
        mBrowserBridge = bridge;
    }

    public void setCredManHelperForTesting(CredManHelper helper) {
        mCredManHelper = helper;
    }

    public void setBarrierForTesting(Barrier barrier) {
        mBarrier = barrier;
    }

    private void onWebauthnCredentialDetailsListReceived(
            PublicKeyCredentialRequestOptions options,
            String callerOriginString,
            byte[] clientDataHash,
            List<WebauthnCredentialDetails> credentials,
            long conditionalUiCredentialListInitialTimeMs) {
        assert mConditionalUiState == ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST
                || mConditionalUiState == ConditionalUiState.CANCEL_PENDING;

        boolean hasAllowCredentials =
                options.allowCredentials != null && options.allowCredentials.length != 0;
        boolean isConditionalRequest = options.isConditional;
        assert isConditionalRequest || !hasAllowCredentials;

        if (!credentials.isEmpty()) {
            RecordHistogram.recordTimesHistogram(
                    "WebAuthentication.CredentialFetchDuration.GmsCore",
                    SystemClock.elapsedRealtime() - conditionalUiCredentialListInitialTimeMs);
        }

        if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
            // The request was completed synchronously when the cancellation was received,
            // so no need to return an error to the renderer.
            mConditionalUiState = ConditionalUiState.NONE;
            return;
        }

        List<WebauthnCredentialDetails> discoverableCredentials = new ArrayList<>();
        for (WebauthnCredentialDetails credential : credentials) {
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

        if (!isConditionalRequest
                && discoverableCredentials.isEmpty()
                && getBarrierMode() != Barrier.Mode.BOTH) {
            mConditionalUiState = ConditionalUiState.NONE;
            // When no passkeys are present for a non-conditional request, pass the request
            // through to GMSCore. It will show an error message to the user, but can offer the
            // user alternatives to use external passkeys.
            // If the barrier mode is BOTH, the no passkeys state is handled by Chrome. Do not pass
            // the request to GMSCore.
            maybeDispatchGetAssertionRequest(options, callerOriginString, clientDataHash, null);
            return;
        }

        Runnable hybridCallback = null;
        if (GmsCoreUtils.isHybridClientApiSupported()) {
            hybridCallback =
                    () ->
                            dispatchHybridGetAssertionRequest(
                                    options, callerOriginString, clientDataHash);
        }

        mConditionalUiState = ConditionalUiState.WAITING_FOR_SELECTION;
        getBridge()
                .onCredentialsDetailsListReceived(
                        mAuthenticationContextProvider.getRenderFrameHost(),
                        discoverableCredentials,
                        isConditionalRequest,
                        (selectedCredentialId) ->
                                maybeDispatchGetAssertionRequest(
                                        options,
                                        callerOriginString,
                                        clientDataHash,
                                        selectedCredentialId),
                        hybridCallback);
    }

    /**
     * Check whether a get() request needs routing to Play Services for a credential.
     *
     * <p>This function is called if a non-payments, non-conditional get() call with an allowlist is
     * received.
     *
     * <p>When Barrier.Mode is`ONLY_CRED_MAN`, all discoverable credentials are available in
     * CredMan. If any of the elements of the allowlist are non-discoverable credentials in the
     * local platform authenticator then the request should be sent directly to Play Services. It is
     * not required to dispatch the request to CredMan.
     *
     * <p>When Barrier.Mode is `BOTH`, some discoverable credentials may also be in Play Services.
     * If any of the elements of the allowlist match the credentials in the local platform
     * authenticator then the request should be sent directly to Play Services.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private void checkForMatchingCredentials(
            PublicKeyCredentialRequestOptions options, Origin callerOrigin, byte[] clientDataHash) {
        assert options.allowCredentials != null;
        assert options.allowCredentials.length > 0;
        assert !options.isConditional;
        assert mPlayServicesAvailable;
        Barrier.Mode mode = getBarrierMode();
        assert mode == Barrier.Mode.ONLY_CRED_MAN || mode == Barrier.Mode.BOTH;

        Fido2ApiCallHelper.getInstance()
                .invokeFido2GetCredentials(
                        mAuthenticationContextProvider,
                        options.relyingPartyId,
                        (credentials) ->
                                checkForMatchingCredentialsReceived(
                                        options, callerOrigin, clientDataHash, credentials),
                        (e) -> {
                            Log.e(
                                    TAG,
                                    "FIDO2 call to enumerate credentials failed. Dispatching to"
                                            + " CredMan. Barrier.Mode = "
                                            + mode,
                                    e);
                            mCredManHelper.startGetRequest(
                                    options,
                                    convertOriginToString(callerOrigin),
                                    mClientDataJson,
                                    clientDataHash,
                                    mGetAssertionCallback,
                                    this::returnErrorAndResetCallback,
                                    mode == Barrier.Mode.BOTH);
                        });
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private void checkForMatchingCredentialsReceived(
            PublicKeyCredentialRequestOptions options,
            Origin callerOrigin,
            byte[] clientDataHash,
            List<WebauthnCredentialDetails> retrievedCredentials) {
        assert options.allowCredentials != null;
        assert options.allowCredentials.length > 0;
        assert !options.isConditional;
        assert mPlayServicesAvailable;
        Barrier.Mode mode = getBarrierMode();
        assert mode == Barrier.Mode.ONLY_CRED_MAN || mode == Barrier.Mode.BOTH;

        for (WebauthnCredentialDetails credential : retrievedCredentials) {
            // In ONLY_CRED_MAN mode, all discoverable credentials are handled by CredMan. It is not
            // required to check for discoverable credentials.
            if (mode == Barrier.Mode.ONLY_CRED_MAN && credential.mIsDiscoverable) {
                continue;
            }

            for (PublicKeyCredentialDescriptor allowedId : options.allowCredentials) {
                if (allowedId.type != PublicKeyCredentialType.PUBLIC_KEY) {
                    continue;
                }

                if (Arrays.equals(allowedId.id, credential.mCredentialId)) {
                    // This get() request can be satisfied by Play Services with
                    // a non-discoverable credential so route it there.
                    maybeDispatchGetAssertionRequest(
                            options,
                            convertOriginToString(callerOrigin),
                            clientDataHash,
                            /* credentialId= */ null);
                    return;
                }
            }
        }

        mCredManHelper.setNoCredentialsFallback(
                () ->
                        this.maybeDispatchGetAssertionRequest(
                                options,
                                convertOriginToString(callerOrigin),
                                clientDataHash,
                                /* credentialId= */ null));

        // No elements of the allowlist are local, non-discoverable credentials
        // so route to CredMan.
        mCredManHelper.startGetRequest(
                options,
                convertOriginToString(callerOrigin),
                mClientDataJson,
                clientDataHash,
                mGetAssertionCallback,
                this::returnErrorAndResetCallback,
                mode == Barrier.Mode.BOTH);
    }

    private void maybeDispatchGetAssertionRequest(
            PublicKeyCredentialRequestOptions options,
            String callerOriginString,
            byte[] clientDataHash,
            byte[] credentialId) {
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
                    getBridge().cleanupRequest(mAuthenticationContextProvider.getRenderFrameHost());
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

        Fido2ApiCallHelper.getInstance()
                .invokeFido2GetAssertion(
                        mAuthenticationContextProvider,
                        options,
                        Uri.parse(callerOriginString),
                        clientDataHash,
                        getMaybeResultReceiver(),
                        this::onGotPendingIntent,
                        this::onBinderCallException);
    }

    private void dispatchHybridGetAssertionRequest(
            PublicKeyCredentialRequestOptions options,
            String callerOriginString,
            byte[] clientDataHash) {
        assert mConditionalUiState == ConditionalUiState.NONE
                || mConditionalUiState == ConditionalUiState.REQUEST_SENT_TO_PLATFORM
                || mConditionalUiState == ConditionalUiState.WAITING_FOR_SELECTION;

        if (mConditionalUiState == ConditionalUiState.REQUEST_SENT_TO_PLATFORM) {
            Log.e(TAG, "Received a second credential selection while the first still in progress.");
            return;
        }
        mConditionalUiState = ConditionalUiState.REQUEST_SENT_TO_PLATFORM;

        Fido2ApiCallParams params =
                WebauthnModeProvider.getInstance()
                        .getFido2ApiCallParams(mAuthenticationContextProvider.getWebContents());
        Fido2ApiCall call = new Fido2ApiCall(mAuthenticationContextProvider.getContext(), params);
        Parcel args = call.start();
        String callbackDescriptor =
                WebauthnModeProvider.getInstance()
                        .getFido2ApiCallParams(mAuthenticationContextProvider.getWebContents())
                        .mCallbackDescriptor;
        Fido2ApiCall.PendingIntentResult result =
                new Fido2ApiCall.PendingIntentResult(callbackDescriptor);
        args.writeStrongBinder(result);
        args.writeInt(1); // This indicates that the following options are present.
        Fido2Api.appendBrowserGetAssertionOptionsToParcel(
                options,
                Uri.parse(callerOriginString),
                clientDataHash,
                /* tunnelId= */ null,
                /* resultReceiver= */ null,
                args);
        Task<PendingIntent> task =
                call.run(
                        Fido2ApiCall.METHOD_BROWSER_HYBRID_SIGN,
                        Fido2ApiCall.TRANSACTION_HYBRID_SIGN,
                        args,
                        result);
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

        if (!mAuthenticationContextProvider.getIntentSender().showIntent(pendingIntent, this)) {
            Log.e(TAG, "Failed to send intent to FIDO API");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }
    }

    @Nullable
    private ResultReceiver getMaybeResultReceiver() {
        // The FIDO API traditionally returned a PendingIntent, which the calling app was expected
        // to invoke and then receive the result from the Activity it launched.
        //
        // However in a WebView context this is problematic because the WebView does not control the
        // app's activity to get the result. Thus support for using a ResultReceiver was added to
        // the API. Since we don't want to immediately increase the minimum GMS Core version needed
        // to run Chromium on Android, this code supports both methods.
        //
        // In time, once the GMS Core update has propagated sufficiently, we could consider removing
        // support for anything except the ResultReceiver.
        if (isChrome(mAuthenticationContextProvider.getWebContents())) return null;
        return new ResultReceiver(new Handler(Looper.getMainLooper())) {
            @Override
            protected void onReceiveResult(int resultCode, Bundle resultData) {
                onResultReceiverResult(resultData);
            }
        };
    }

    private void onResultReceiverResult(Bundle resultData) {
        int errorCode = AuthenticatorStatus.UNKNOWN_ERROR;
        Object response = null;
        byte[] responseBytes = resultData.getByteArray(Fido2Api.CREDENTIAL_EXTRA);
        if (responseBytes != null) {
            try {
                response = Fido2Api.parseResponse(responseBytes);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Failed to parse FIDO2 API response from ResultReceiver", e);
                response = null;
            }
        }

        handleFido2Response(errorCode, response);
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

        handleFido2Response(errorCode, response);
    }

    private void handleFido2Response(int errorCode, Object response) {
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        if (mConditionalUiState != ConditionalUiState.NONE) {
            if (response == null || response instanceof Pair) {
                if (response != null) {
                    Pair<Integer, String> error = (Pair<Integer, String>) response;
                    Log.e(
                            TAG,
                            "FIDO2 API call resulted in error: "
                                    + error.first
                                    + " "
                                    + (error.second != null ? error.second : ""));
                    errorCode = convertError(error);
                }

                if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                    mConditionalUiState = ConditionalUiState.NONE;
                    getBridge().cleanupRequest(frameHost);
                    mBarrier.onFido2ApiCancelled();
                } else {
                    // The user can try again by selecting another conditional UI credential.
                    mConditionalUiState = ConditionalUiState.WAITING_FOR_SELECTION;
                }
                return;
            }
            mConditionalUiState = ConditionalUiState.NONE;
            getBridge().cleanupRequest(frameHost);
        }

        if (response == null) {
            // Use the error already set.
        } else if (response instanceof Pair) {
            Pair<Integer, String> error = (Pair<Integer, String>) response;
            Log.e(
                    TAG,
                    "FIDO2 API call resulted in error: "
                            + error.first
                            + " "
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
                if (mClientDataJson != null) {
                    creationResponse.info.clientDataJson = mClientDataJson;
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
                    r.info.clientDataJson = mClientDataJson;
                    if (frameHost != null) {
                        frameHost.notifyWebAuthnAssertionRequestSucceeded();
                    }
                }
                r.extensions.echoAppidExtension = mAppIdExtensionUsed;
                mGetAssertionCallback.onSignResponse(AuthenticatorStatus.SUCCESS, r);
                mGetAssertionCallback = null;
                return;
            }
        }

        returnErrorAndResetCallback(errorCode);
    }

    /**
     * Helper method to convert AuthenticatorErrorResponse errors.
     *
     * @return error code corresponding to an AuthenticatorStatus.
     */
    private static int convertError(Pair<Integer, String> error) {
        final int errorCode = error.first;
        @Nullable final String errorMsg = error.second;

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

    private byte[] buildClientDataJsonAndComputeHash(
            @ClientDataRequestType int clientDataRequestType,
            String callerOrigin,
            byte[] challenge,
            boolean isCrossOrigin,
            PaymentOptions paymentOptions,
            String relyingPartyId,
            Origin topOrigin) {
        String clientDataJson =
                ClientDataJson.buildClientDataJson(
                        clientDataRequestType,
                        callerOrigin,
                        challenge,
                        isCrossOrigin,
                        paymentOptions,
                        relyingPartyId,
                        topOrigin);
        if (clientDataJson == null) {
            return null;
        }
        mClientDataJson = clientDataJson.getBytes();
        MessageDigest messageDigest;
        try {
            messageDigest = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            return null;
        }
        messageDigest.update(mClientDataJson);
        return messageDigest.digest();
    }

    @Override
    public WebauthnBrowserBridge getBridge() {
        if (!isChrome(mAuthenticationContextProvider.getWebContents())) {
            return null;
        }
        if (mBrowserBridge == null) {
            mBrowserBridge = new WebauthnBrowserBridge();
        }
        return mBrowserBridge;
    }

    protected void destroyBridge() {
        if (mBrowserBridge == null) return;
        mBrowserBridge.destroy();
        mBrowserBridge = null;
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
