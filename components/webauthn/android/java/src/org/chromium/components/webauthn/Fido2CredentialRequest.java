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
import android.os.Parcel;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.tasks.Task;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
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
import org.chromium.content_public.browser.ClientDataJson;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.device.DeviceFeatureList;
import org.chromium.device.DeviceFeatureMap;
import org.chromium.net.GURLUtils;
import org.chromium.url.Origin;

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
@JNINamespace("webauthn")
public class Fido2CredentialRequest
        implements Callback<Pair<Integer, Intent>>, CredManHelper.BridgeProvider {
    private static final String TAG = "Fido2Request";
    static final String NON_EMPTY_ALLOWLIST_ERROR_MSG =
            "Authentication request must have non-empty allowList";
    static final String NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG =
            "Request doesn't have a valid list of allowed credentials.";
    static final String NO_SCREENLOCK_ERROR_MSG = "The device is not secured with any screen lock";
    static final String CREDENTIAL_EXISTS_ERROR_MSG =
            "One of the excluded credentials exists on the local device";
    static final String LOW_LEVEL_ERROR_MSG = "Low level error 0x6a80";
    public static final int GMSCORE_MIN_VERSION_HYBRID_API = 231206000;

    private final WebAuthenticationDelegate.IntentSender mIntentSender;
    // mPlayServicesAvailable caches whether the Play Services FIDO API is
    // available.
    private final boolean mPlayServicesAvailable;
    private Context mContext;
    private GetAssertionResponseCallback mGetAssertionCallback;
    private MakeCredentialResponseCallback mMakeCredentialCallback;
    private FidoErrorResponseCallback mErrorCallback;
    private CredManHelper mCredManHelper;
    private Barrier mBarrier;
    // mFrameHost is null in makeCredential requests. For getAssertion requests
    // it's non-null for conditional requests and may be non-null in other
    // requests.
    private RenderFrameHost mFrameHost;
    private boolean mAppIdExtensionUsed;
    private boolean mEchoCredProps;
    private WebAuthnBrowserBridge mBrowserBridge;
    private boolean mAttestationAcceptable;
    private boolean mIsCrossOrigin;
    // mIsHybridRequest is true if this request comes from a hybrid (i.e. cross-device) flow rather
    // than a WebContents. Handling the hybrid protocol can be delegated to Chrome (by Play
    // Services).
    private boolean mIsHybridRequest;

    public enum ConditionalUiState {
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
    private byte[] mClientDataJson;

    /**
     * Constructs the object.
     *
     * @param intentSender Interface for starting {@link Intent}s from Play Services.
     * @param supportLevel Whether this code should use the privileged or non-privileged Play
     *         Services API. (Note that a value of `NONE` is not allowed.)
     */
    public Fido2CredentialRequest(WebAuthenticationDelegate.IntentSender intentSender) {
        mIntentSender = intentSender;
        mPlayServicesAvailable = Fido2ApiCallHelper.getInstance().arePlayServicesAvailable();
        mCredManHelper = new CredManHelper(this, mPlayServicesAvailable);
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
        if (support != CredManSupport.DISABLED
                && mIsHybridRequest
                && DeviceFeatureMap.isEnabled(
                        DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN_FOR_HYBRID)) {
            return Barrier.Mode.ONLY_CRED_MAN;
        }
        switch (support) {
            case CredManSupport.DISABLED:
                return Barrier.Mode.ONLY_FIDO_2_API;
            case CredManSupport.IF_REQUIRED:
                if (mIsHybridRequest
                        && DeviceFeatureMap.isEnabled(
                                DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN_FOR_HYBRID)) {
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
     * @param frameHost The source RenderFrameHost, or null. If null, `maybeClientDataHash` must be
     *         non-null and no security checks will be performed.
     * @param maybeClientDataHash The SHA-256 of the ClientDataJSON. Non-null iff frameHost is null.
     * @param origin The origin that made the WebAuthn call.
     * @param callback Success callback.
     * @param errorCallback Failure callback.
     */
    @SuppressWarnings("NewApi")
    public void handleMakeCredentialRequest(Context context,
            PublicKeyCredentialCreationOptions options, RenderFrameHost frameHost,
            byte[] maybeClientDataHash, Origin origin, MakeCredentialResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        assert (frameHost != null) ^ (maybeClientDataHash != null);
        assert mMakeCredentialCallback == null && mErrorCallback == null;
        mContext = context;
        mMakeCredentialCallback = callback;
        mErrorCallback = errorCallback;

        if (frameHost != null) {
            int securityCheck = frameHost.performMakeCredentialWebAuthSecurityChecks(
                    options.relyingParty.id, origin, options.isPaymentCredentialCreation);
            if (securityCheck != AuthenticatorStatus.SUCCESS) {
                returnErrorAndResetCallback(securityCheck);
                return;
            }
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
                            mContext,
                            mFrameHost,
                            options,
                            convertOriginToString(origin),
                            maybeClientDataHash,
                            callback,
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
            Fido2ApiCallHelper.getInstance().invokeFido2MakeCredential(options,
                    Uri.parse(convertOriginToString(origin)), maybeClientDataHash,
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

    /**
     * Process a WebAuthn get() request.
     *
     * @param context The context used for both Play Services and CredMan calls.
     * @param options The arguments to get(). If `isConditional` is true then `frameHost` must be
     *         non-null.
     * @param frameHost The source RenderFrameHost, or null. If null, `maybeClientDataHash` must be
     *         non-null and no security checks will be performed.
     * @param maybeClientDataHash The SHA-256 of the ClientDataJSON. Non-null iff frameHost is null.
     * @param origin The origin that made the WebAuthn call.
     * @param topOrigin The origin of the main frame.
     * @param payment Options for Secure Payment Confirmation. May only be non-null if `frameHost`
     *         is non-null.
     * @param callback Success callback.
     * @param errorCallback Failure callback.
     */
    @SuppressWarnings("NewApi")
    public void handleGetAssertionRequest(Context context,
            PublicKeyCredentialRequestOptions options, RenderFrameHost frameHost,
            byte[] maybeClientDataHash, Origin origin, Origin topOrigin, PaymentOptions payment,
            GetAssertionResponseCallback callback, FidoErrorResponseCallback errorCallback) {
        assert (frameHost != null) ^ (maybeClientDataHash != null);
        assert payment == null || frameHost != null;
        assert !options.isConditional || frameHost != null;
        assert mGetAssertionCallback == null && mErrorCallback == null;
        mContext = context;
        mGetAssertionCallback = callback;
        mErrorCallback = errorCallback;
        mFrameHost = frameHost;

        if (frameHost != null) {
            WebAuthSecurityChecksResults webAuthSecurityChecksResults =
                    frameHost.performGetAssertionWebAuthSecurityChecks(
                            options.relyingPartyId, origin, payment != null);
            if (webAuthSecurityChecksResults.securityCheckResult != AuthenticatorStatus.SUCCESS) {
                returnErrorAndResetCallback(webAuthSecurityChecksResults.securityCheckResult);
                return;
            }
            mIsCrossOrigin = webAuthSecurityChecksResults.isCrossOrigin;
        }

        boolean hasAllowCredentials =
                options.allowCredentials != null && options.allowCredentials.length != 0;

        if (!hasAllowCredentials) {
            // No UVM support for discoverable credentials.
            options.extensions.userVerificationMethods = false;
        }

        if (options.extensions.appid != null) {
            mAppIdExtensionUsed = true;
        }

        // Payments should still go through Google Play Services. Also, if the request has
        // pre-hashed PRF inputs then we cannot represent that in JSON and so can only forward to
        // Play Services.
        if (payment == null && !options.extensions.prfInputsHashed
                && getBarrierMode() == Barrier.Mode.ONLY_CRED_MAN) {
            if (options.isConditional) {
                mBarrier.resetAndSetWaitStatus(Barrier.Mode.ONLY_CRED_MAN);
                mCredManHelper.startPrefetchRequest(mContext, mFrameHost, options,
                        convertOriginToString(origin), mIsCrossOrigin,
                        /*maybeClientDataHash=*/null, callback, this::returnErrorAndResetCallback,
                        mBarrier, /*ignoreGpm=*/false);
            } else if (hasAllowCredentials && mPlayServicesAvailable) {
                // If the allowlist contains non-discoverable credentials then
                // the request needs to be routed directly to Play Services.
                checkForMatchingCredentials(options, origin, maybeClientDataHash);
            } else {
                mCredManHelper.setNoCredentialsFallback(
                        ()
                                -> this.maybeDispatchGetAssertionRequest(options,
                                        convertOriginToString(origin), maybeClientDataHash,
                                        /*credentialId=*/null));
                int response = mCredManHelper.startGetRequest(mContext, mFrameHost, options,
                        convertOriginToString(origin), mIsCrossOrigin, maybeClientDataHash,
                        callback, this::returnErrorAndResetCallback, /*ignoreGpm=*/false);
                if (response != AuthenticatorStatus.SUCCESS) returnErrorAndResetCallback(response);
            }
            return;
        }

        if (!mPlayServicesAvailable) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        final String callerOriginString = convertOriginToString(origin);
        byte[] clientDataHash = maybeClientDataHash;
        if (payment != null
                && PaymentFeatureList.isEnabled(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION)) {
            assert options.challenge != null;
            assert clientDataHash == null;
            clientDataHash = buildClientDataJsonAndComputeHash(ClientDataRequestType.PAYMENT_GET,
                    callerOriginString, options.challenge, mIsCrossOrigin, payment,
                    options.relyingPartyId, topOrigin);
            if (clientDataHash == null) {
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }
        }

        if (mFrameHost != null && (options.isConditional || !hasAllowCredentials)) {
            // Enumerate credentials from Play Services so that we can show the
            // picker in Chrome UI.
            final byte[] finalClientDataHash = clientDataHash;

            if (getBarrierMode() == Barrier.Mode.BOTH) {
                mBarrier.resetAndSetWaitStatus(Barrier.Mode.BOTH);
                mCredManHelper.startPrefetchRequest(
                        context,
                        frameHost,
                        options,
                        callerOriginString,
                        mIsCrossOrigin,
                        maybeClientDataHash,
                        callback,
                        this::returnErrorAndResetCallback,
                        mBarrier,
                        /* ignoreGpm= */ true);
            } else {
                mBarrier.resetAndSetWaitStatus(Barrier.Mode.ONLY_FIDO_2_API);
            }
            mConditionalUiState = ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST;
            Fido2ApiCallHelper.getInstance().invokeFido2GetCredentials(options.relyingPartyId,
                    (credentials)
                            -> mBarrier.onFido2ApiSuccessful(
                                    ()
                                            -> onWebAuthnCredentialDetailsListReceived(options,
                                                    callerOriginString, finalClientDataHash,
                                                    credentials)),
                    (e) -> mBarrier.onFido2ApiFailed(AuthenticatorStatus.NOT_ALLOWED_ERROR));
            return;
        }

        if (hasAllowCredentials
                && !options.isConditional
                && getBarrierMode() == Barrier.Mode.BOTH) {
            checkForMatchingCredentials(options, origin, maybeClientDataHash);
            return;
        }
        maybeDispatchGetAssertionRequest(options, callerOriginString, clientDataHash, null);
    }

    public void cancelConditionalGetAssertion(RenderFrameHost frameHost) {
        mCredManHelper.cancelConditionalGetAssertion(frameHost);
        switch (mConditionalUiState) {
            case WAITING_FOR_CREDENTIAL_LIST:
                mConditionalUiState = ConditionalUiState.CANCEL_PENDING;
                mBarrier.onFido2ApiCancelled();
                break;
            case WAITING_FOR_SELECTION:
                mBrowserBridge.cleanupRequest(frameHost);
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
            Context context, IsUvpaaResponseCallback callback) {
        if (getBarrierMode() == Barrier.Mode.ONLY_CRED_MAN) {
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

        Fido2ApiCall call = new Fido2ApiCall(context);
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

        if (!mPlayServicesAvailable) {
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

    public void setIsHybridRequest(boolean isHybridRequest) {
        mIsHybridRequest = isHybridRequest;
    }

    public void overrideBrowserBridgeForTesting(WebAuthnBrowserBridge bridge) {
        mBrowserBridge = bridge;
    }

    public void setCredManHelperForTesting(CredManHelper helper) {
        mCredManHelper = helper;
    }

    public void setBarrierForTesting(Barrier barrier) {
        mBarrier = barrier;
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
            PublicKeyCredentialRequestOptions options,
            Origin callerOrigin,
            byte[] maybeClientDataHash) {
        assert options.allowCredentials != null;
        assert options.allowCredentials.length > 0;
        assert !options.isConditional;
        assert mPlayServicesAvailable;
        Barrier.Mode mode = getBarrierMode();
        assert mode == Barrier.Mode.ONLY_CRED_MAN || mode == Barrier.Mode.BOTH;

        Fido2ApiCallHelper.getInstance()
                .invokeFido2GetCredentials(
                        options.relyingPartyId,
                        (credentials) ->
                                checkForMatchingCredentialsReceived(
                                        options, callerOrigin, maybeClientDataHash, credentials),
                        (e) -> {
                            Log.e(
                                    TAG,
                                    "FIDO2 call to enumerate credentials failed. Dispatching to"
                                            + " CredMan. Barrier.Mode = "
                                            + mode,
                                    e);
                            mCredManHelper.startGetRequest(
                                    mContext,
                                    mFrameHost,
                                    options,
                                    convertOriginToString(callerOrigin),
                                    mIsCrossOrigin,
                                    maybeClientDataHash,
                                    mGetAssertionCallback,
                                    this::returnErrorAndResetCallback,
                                    mode == Barrier.Mode.BOTH);
                        });
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private void checkForMatchingCredentialsReceived(
            PublicKeyCredentialRequestOptions options,
            Origin callerOrigin,
            byte[] maybeClientDataHash,
            List<WebAuthnCredentialDetails> retrievedCredentials) {
        assert options.allowCredentials != null;
        assert options.allowCredentials.length > 0;
        assert !options.isConditional;
        assert mPlayServicesAvailable;
        Barrier.Mode mode = getBarrierMode();
        assert mode == Barrier.Mode.ONLY_CRED_MAN || mode == Barrier.Mode.BOTH;

        for (WebAuthnCredentialDetails credential : retrievedCredentials) {
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
                    maybeDispatchGetAssertionRequest(options, convertOriginToString(callerOrigin),
                            maybeClientDataHash, /*credentialId=*/null);
                    return;
                }
            }
        }

        mCredManHelper.setNoCredentialsFallback(
                () ->
                        this.maybeDispatchGetAssertionRequest(
                                options,
                                convertOriginToString(callerOrigin),
                                maybeClientDataHash,
                                /* credentialId= */ null));

        // No elements of the allowlist are local, non-discoverable credentials
        // so route to CredMan.
        mCredManHelper.startGetRequest(
                mContext,
                mFrameHost,
                options,
                convertOriginToString(callerOrigin),
                mIsCrossOrigin,
                maybeClientDataHash,
                mGetAssertionCallback,
                this::returnErrorAndResetCallback,
                mode == Barrier.Mode.BOTH);
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

        Fido2ApiCall call = new Fido2ApiCall(mContext);
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
                    mBarrier.onFido2ApiCancelled();
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
                    r.info.clientDataJson = mClientDataJson;
                    if (mFrameHost != null) {
                        mFrameHost.notifyWebAuthnAssertionRequestSucceeded();
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

    private byte[] buildClientDataJsonAndComputeHash(
            @ClientDataRequestType int clientDataRequestType, String callerOrigin, byte[] challenge,
            boolean isCrossOrigin, PaymentOptions paymentOptions, String relyingPartyId,
            Origin topOrigin) {
        String clientDataJson = ClientDataJson.buildClientDataJson(clientDataRequestType,
                callerOrigin, challenge, isCrossOrigin, paymentOptions, relyingPartyId, topOrigin);
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

    private boolean isHybridClientApiAvailable() {
        return PackageUtils.getPackageVersion("com.google.android.gms")
                >= GMSCORE_MIN_VERSION_HYBRID_API
                && DeviceFeatureMap.isEnabled(DeviceFeatureList.WEBAUTHN_ANDROID_HYBRID_CLIENT_UI);
    }

    @Override
    public WebAuthnBrowserBridge getBridge() {
        if (mBrowserBridge == null) {
            mBrowserBridge = new WebAuthnBrowserBridge();
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
