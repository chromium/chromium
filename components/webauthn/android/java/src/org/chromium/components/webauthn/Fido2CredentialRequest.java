// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.webauthn.WebauthnLogger.log;
import static org.chromium.components.webauthn.WebauthnLogger.logError;
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
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.tasks.Task;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.AuthenticatorTransport;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.GetCredentialOptions;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.Mediation;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialReportOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.password_manager.BrowserAssistedLoginType;
import org.chromium.components.webauthn.Fido2ApiCall.Fido2ApiCallParams;
import org.chromium.components.webauthn.cred_man.CredManHelper;
import org.chromium.components.webauthn.cred_man.CredManSupportProvider;
import org.chromium.content_public.browser.ClientDataJson;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.DeviceFeatureList;
import org.chromium.net.GURLUtils;
import org.chromium.ui.util.RunnableTimer;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Uses the Google Play Services Fido2 APIs. Holds the logic of each request. */
@JNINamespace("webauthn")
@NullMarked
public class Fido2CredentialRequest implements WebauthnBrowserBridge.Provider {
    private static final String TAG = "Fido2CredentialRequest";
    static final String NON_EMPTY_ALLOWLIST_ERROR_MSG =
            "Authentication request must have non-empty allowList";
    static final String NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG =
            "Request doesn't have a valid list of allowed credentials.";
    static final String NO_SCREENLOCK_ERROR_MSG = "The device is not secured with any screen lock";
    static final String CREDENTIAL_EXISTS_ERROR_MSG =
            "One of the excluded credentials exists on the local device";
    static final String LOW_LEVEL_ERROR_MSG = "Low level error 0x6a80";
    static final String CANCELLED_ERROR_MSG = "[16] Cancelled by user.";

    @IntDef({
        Fido2ApiRequestType.MAKE_CREDENTIAL,
        Fido2ApiRequestType.GET_ASSERTION,
        Fido2ApiRequestType.GET_ASSERTION_HYBRID,
        Fido2ApiRequestType.GET_ASSERTION_LEGACY
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Fido2ApiRequestType {
        int MAKE_CREDENTIAL = 0;
        int GET_ASSERTION = 1;
        int GET_ASSERTION_HYBRID = 2;
        int GET_ASSERTION_LEGACY = 3;
    }

    // mPlayServicesAvailable caches whether the Play Services FIDO API is
    // available.
    private final boolean mPlayServicesAvailable;
    private AuthenticationContextProvider mAuthenticationContextProvider;
    private CredManHelper mCredManHelper;
    private final IdentityCredentialsHelper mIdentityCredentialsHelper;
    private Barrier mBarrier;
    // mFrameHost is null in makeCredential requests. For getAssertion requests
    // it's non-null for conditional requests and may be non-null in other
    // requests.
    private boolean mAppIdExtensionUsed;
    private boolean mEchoCredProps;
    private @Nullable WebauthnBrowserBridge mBrowserBridge;

    private RunnableTimer mImmediateTimer = new RunnableTimer();

    // Some modes do credential enumeration in advance of calling a platform API to get a passkey
    // assertion. In these cases a cancellation before the final request is sent can prevent
    // UI from being shown. Cancellation is ignored if the UI might already be showing.
    @IntDef({
        CancellableUiState.NONE,
        CancellableUiState.WAITING_FOR_RP_ID_VALIDATION,
        CancellableUiState.WAITING_FOR_CREDENTIAL_LIST,
        CancellableUiState.WAITING_FOR_SELECTION,
        CancellableUiState.REQUEST_SENT_TO_PLATFORM,
        CancellableUiState.CANCEL_PENDING,
        CancellableUiState.CANCEL_PENDING_RP_ID_VALIDATION_COMPLETE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CancellableUiState {
        int NONE = 0;
        int WAITING_FOR_RP_ID_VALIDATION = 1;
        int WAITING_FOR_CREDENTIAL_LIST = 2;
        int WAITING_FOR_SELECTION = 3;
        int REQUEST_SENT_TO_PLATFORM = 4;
        int CANCEL_PENDING = 5;
        int CANCEL_PENDING_RP_ID_VALIDATION_COMPLETE = 6;
    }

    private @CancellableUiState int mCancellableUiState = CancellableUiState.NONE;

    // Not null when the GMSCore-created ClientDataJson needs to be overridden or when using the
    // CredMan API.
    private byte @Nullable [] mClientDataJson;

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
        mIdentityCredentialsHelper = new IdentityCredentialsHelper(mAuthenticationContextProvider);
        mBarrier = new Barrier(mAuthenticationContextProvider);
    }

    public void setAuthenticationContextProvider(
            AuthenticationContextProvider authenticationContextProvider) {
        mAuthenticationContextProvider = authenticationContextProvider;
    }

    private void returnErrorAndResetCallback(
            int errorCode,
            @Nullable Integer response,
            @Nullable @CredentialRequestResult Integer credentialRequestResult) {

        log(TAG, "returnErrorAndResetCallback: %d", errorCode);
        WebauthnRequestCallback requestCallback =
                mAuthenticationContextProvider.getRequestCallback();
        if (requestCallback == null) return;
        switch (requestCallback.getCallbackType()) {
            case WebauthnRequestCallback.CallbackType.MAKE_CREDENTIAL:
                RequestMetrics.Builder makeBuilder = new RequestMetrics.Builder();
                if (response != null) {
                    makeBuilder.setMakeCredentialOutcome(response);
                }
                if (credentialRequestResult != null) {
                    makeBuilder.setMakeCredentialResult(credentialRequestResult);
                }
                requestCallback.onComplete(
                        WebauthnRequestResponse.forFailedMakeCredential(
                                errorCode, makeBuilder.build()));
                break;
            case WebauthnRequestCallback.CallbackType.GET_CREDENTIAL:
                RequestMetrics.Builder builder =
                        new RequestMetrics.Builder();
                if (response != null) {
                    builder.setGetAssertionOutcome(response);
                }
                if (credentialRequestResult != null) {
                    builder.setGetAssertionResult(credentialRequestResult);
                }
                requestCallback.onComplete(
                        WebauthnRequestResponse.forFailedGetCredential(errorCode, builder.build()));
                break;
            case WebauthnRequestCallback.CallbackType.REPORT:
                requestCallback.onComplete(WebauthnRequestResponse.forReport(errorCode));
                break;
            default:
                assert false : "Unknown callback type";
        }
    }

    private @Barrier.Mode int getBarrierMode() {
        @CredManSupport int support = CredManSupportProvider.getCredManSupport();
        @Barrier.Mode int mode;
        switch (support) {
            case CredManSupport.DISABLED:
                mode = Barrier.Mode.ONLY_FIDO_2_API;
                break;
            case CredManSupport.FULL_UNLESS_INAPPLICABLE:
                mode = Barrier.Mode.ONLY_CRED_MAN;
                break;
            case CredManSupport.PARALLEL_WITH_FIDO_2:
                mode = Barrier.Mode.BOTH;
                break;
            default:
                assert support == CredManSupport.NOT_EVALUATED
                        : "All `CredManMode`s must be handled!";
                mode = Barrier.Mode.ONLY_FIDO_2_API;
        }
        log(TAG, "Barrier mode is " + mode);

        return mode;
    }

    /**
     * Process a WebAuthn create() request.
     *
     * @param options The arguments to create()
     * @param maybeBrowserOptions Optional set of browser-specific data, like channel or incognito.
     * @param origin The origin that made the WebAuthn call.
     * @param topOrigin The origin of the main frame.
     * @param paymentOptions The options set by Secure Payment Confirmation.
     */
    public void handleMakeCredentialRequest(
            PublicKeyCredentialCreationOptions options,
            @Nullable Bundle maybeBrowserOptions,
            Origin origin,
            @Nullable Origin topOrigin,
            @Nullable PaymentOptions paymentOptions) {
        log(TAG, "handleMakeCredentialRequest");
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        assert frameHost != null;
        assert mAuthenticationContextProvider.getRequestCallback() != null;
        assert (paymentOptions != null)
                == (options.isPaymentCredentialCreation
                        && ContentFeatureMap.isEnabled(
                                BlinkFeatures.SECURE_PAYMENT_CONFIRMATION_BROWSER_BOUND_KEYS));
        @Nullable Origin remoteDesktopOrigin = null;
        if (options.remoteDesktopClientOverride != null
                && isChrome(mAuthenticationContextProvider.getWebContents())) {
            // SECURITY: remoteDesktopClientOverride comes from the renderer process and is
            // untrusted. We only use the override origin if the "caller origin" is explicitly
            // allowlisted with an enterprise policy.
            // This validation happens in the security checker's ValidateDomainAndRelyingPartyID
            // method.
            remoteDesktopOrigin = new Origin(options.remoteDesktopClientOverride.origin);
        }
        frameHost.performMakeCredentialWebAuthSecurityChecks(
                options.relyingParty.id,
                origin,
                options.isPaymentCredentialCreation,
                remoteDesktopOrigin,
                (result) -> {
                    if (result.securityCheckResult != AuthenticatorStatus.SUCCESS) {
                        returnErrorAndResetCallback(
                                result.securityCheckResult,
                                MakeCredentialOutcome.SECURITY_ERROR,
                                /* credentialRequestResult= */ null);
                        return;
                    }
                    continueMakeCredentialRequestAfterRpIdValidation(
                            options,
                            maybeBrowserOptions,
                            origin,
                            topOrigin,
                            paymentOptions,
                            result.isCrossOrigin);
                });
    }

    @SuppressWarnings("NewApi")
    private void continueMakeCredentialRequestAfterRpIdValidation(
            PublicKeyCredentialCreationOptions options,
            @Nullable Bundle maybeBrowserOptions,
            Origin origin,
            @Nullable Origin topOrigin,
            @Nullable PaymentOptions paymentOptions,
            boolean isCrossOrigin) {
        log(TAG, "continueMakeCredentialRequestAfterRpIdValidation");
        final boolean rkDiscouraged =
                options.authenticatorSelection == null
                        || options.authenticatorSelection.residentKey
                                == ResidentKeyRequirement.DISCOURAGED;
        mEchoCredProps = options.credProps;

        byte[] clientDataHash = null;
        if (!is(mAuthenticationContextProvider.getWebContents(), WebauthnMode.APP)) {
            assert options.challenge != null;
            boolean effectiveCrossOrigin = isCrossOrigin;
            String effectiveOriginString = convertOriginToString(origin);
            // Handle remote desktop client override for ClientDataJSON.
            // The origin from remoteDesktopClientOverride is only used after validation in
            // ValidateDomainAndRelyingPartyID() confirmed that the "caller origin" is allowlisted.
            if (options.remoteDesktopClientOverride != null) {
                effectiveOriginString =
                        convertOriginToString(
                                new Origin(options.remoteDesktopClientOverride.origin));
                effectiveCrossOrigin = !options.remoteDesktopClientOverride.sameOriginWithAncestors;
            }
            clientDataHash =
                    buildClientDataJsonAndComputeHash(
                            ClientDataRequestType.WEB_AUTHN_CREATE,
                            effectiveOriginString,
                            options.challenge,
                            effectiveCrossOrigin,
                            options.isPaymentCredentialCreation ? paymentOptions : null,
                            options.relyingParty.name,
                            topOrigin);
            if (clientDataHash == null) {
                returnErrorAndResetCallback(
                        AuthenticatorStatus.NOT_ALLOWED_ERROR,
                        /* response= */ null,
                        /* credentialRequestResult= */ null);
                return;
            }
        }

        if (options.isConditional) {
            mIdentityCredentialsHelper.handleConditionalCreateRequest(
                    options, convertOriginToString(origin), mClientDataJson, clientDataHash);
            return;
        }

        if (!isChrome(mAuthenticationContextProvider.getWebContents())) {
            if (CredManSupportProvider.getCredManSupportForWebView() == CredManSupport.DISABLED) {
                if (!mPlayServicesAvailable) {
                    logError(TAG, "Google Play Services' Fido2 API is not available.");
                    returnErrorAndResetCallback(
                            AuthenticatorStatus.UNKNOWN_ERROR,
                            /* response= */ null,
                            /* credentialRequestResult= */ null);
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
                                    getMaybeResultReceiver(Fido2ApiRequestType.MAKE_CREDENTIAL),
                                    (pendingIntent) ->
                                            onGotPendingIntent(
                                                    pendingIntent,
                                                    Fido2ApiRequestType.MAKE_CREDENTIAL),
                                    (e) ->
                                            onBinderCallException(
                                                    e, Fido2ApiRequestType.MAKE_CREDENTIAL));
                } catch (NoSuchAlgorithmException e) {
                    returnErrorAndResetCallback(
                            AuthenticatorStatus.ALGORITHM_UNSUPPORTED,
                            MakeCredentialOutcome.ALGORITHM_NOT_SUPPORTED,
                            /* credentialRequestResult= */ null);
                    return;
                }
                return;
            }
            int result =
                    mCredManHelper.startMakeRequest(
                            options,
                            convertOriginToString(origin),
                            mClientDataJson,
                            clientDataHash);
            if (result != AuthenticatorStatus.SUCCESS) {
                returnErrorAndResetCallback(
                        result,
                        /* response= */ null,
                        CredentialRequestResult.ANDROID_CRED_MAN_ERROR);
            }
            return;
        }

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
                && getBarrierMode() == Barrier.Mode.ONLY_CRED_MAN) {
            int result =
                    mCredManHelper.startMakeRequest(
                            options,
                            convertOriginToString(origin),
                            mClientDataJson,
                            clientDataHash);
            if (result != AuthenticatorStatus.SUCCESS) {
                returnErrorAndResetCallback(
                        result,
                        /* response= */ null,
                        CredentialRequestResult.ANDROID_CRED_MAN_ERROR);
            }
            return;
        }

        if (!mPlayServicesAvailable) {
            logError(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(
                    AuthenticatorStatus.UNKNOWN_ERROR,
                    /* response= */ null,
                    /* credentialRequestResult= */ null);
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
                            getMaybeResultReceiver(Fido2ApiRequestType.MAKE_CREDENTIAL),
                            (pendingIntent) ->
                                    onGotPendingIntent(
                                            pendingIntent, Fido2ApiRequestType.MAKE_CREDENTIAL),
                            (e) -> onBinderCallException(e, Fido2ApiRequestType.MAKE_CREDENTIAL));
        } catch (NoSuchAlgorithmException e) {
            returnErrorAndResetCallback(
                    AuthenticatorStatus.ALGORITHM_UNSUPPORTED,
                    MakeCredentialOutcome.ALGORITHM_NOT_SUPPORTED,
                    /* credentialRequestResult= */ null);
            return;
        }
    }

    @SuppressWarnings("NewApi")
    private void handlePasswordOnlyImmediateRequest(GetCredentialOptions options, Origin origin) {
        assert options.password && options.mediation == Mediation.IMMEDIATE;
        final String originString = convertOriginToString(origin);
        if (!isChrome(mAuthenticationContextProvider.getWebContents())) {
            if (CredManSupportProvider.getCredManSupportForWebView() == CredManSupport.DISABLED) {
                returnErrorAndResetCallback(
                        AuthenticatorStatus.NOT_IMPLEMENTED,
                        /* response= */ null,
                        /* credentialRequestResult= */ null);
                return;
            }
            int result =
                    mCredManHelper.startGetRequest(
                            options,
                            originString,
                            /* clientDataJson= */ null,
                            /* clientDataHash= */ null,
                            /* ignoreGpm= */ false);
            if (result != AuthenticatorStatus.SUCCESS) {
                returnErrorAndResetCallback(
                        result,
                        /* response= */ null,
                        CredentialRequestResult.ANDROID_CRED_MAN_ERROR);
            }
            return;
        }

        if (getBarrierMode() == Barrier.Mode.ONLY_CRED_MAN) {
            int result =
                    mCredManHelper.startGetRequest(
                            options,
                            originString,
                            /* clientDataJson= */ null,
                            /* clientDataHash= */ null,
                            /* ignoreGpm= */ false);
            if (result != AuthenticatorStatus.SUCCESS) {
                returnErrorAndResetCallback(
                        result,
                        /* response= */ null,
                        CredentialRequestResult.ANDROID_CRED_MAN_ERROR);
            }
            return;
        }

        // For password-only immediate requests in Chrome without ONLY_CRED_MAN,
        // we go directly to showing the TouchToFill UI. There is no CredMan
        // involved because Chrome doesn't support passwords from CredMan in parallel mode.
        mCancellableUiState = CancellableUiState.WAITING_FOR_CREDENTIAL_LIST;
        onWebauthnCredentialDetailsListReceived(
                options, originString, /* clientDataHash= */ null, new ArrayList<>());
    }

    /**
     * Process a WebAuthn get() request.
     *
     * @param options The arguments to get(). If `options.mediation` is `CONDITIONAL` then
     *     `frameHost` must be non-null.
     * @param origin The origin that made the WebAuthn call.
     * @param topOrigin The origin of the main frame.
     * @param payment Options for Secure Payment Confirmation. May only be non-null if `frameHost`
     *     is non-null.
     */
    @SuppressWarnings("NewApi")
    public void handleGetCredentialRequest(
            GetCredentialOptions options,
            Origin origin,
            @Nullable Origin topOrigin,
            @Nullable PaymentOptions payment) {
        log(TAG, "handleGetCredentialRequest");
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        assert frameHost != null;

        if (options.publicKey == null) {
            handlePasswordOnlyImmediateRequest(options, origin);
            return;
        }

        PublicKeyCredentialRequestOptions publicKeyOptions = options.publicKey;

        // TODO(https://crbug.com/381219428): Handle challenge_url.
        if (publicKeyOptions.challenge == null) {
            returnErrorAndResetCallback(
                    AuthenticatorStatus.NOT_IMPLEMENTED,
                    /* response= */ null,
                    /* credentialRequestResult= */ null);
            return;
        }

        if (options.mediation == Mediation.IMMEDIATE) {
            WebContents webContents = mAuthenticationContextProvider.getWebContents();
            if (publicKeyOptions.allowCredentials != null
                    && publicKeyOptions.allowCredentials.length != 0) {
                log(TAG, "Immediate Get called with non-empty allowCredentials");
                returnErrorAndResetCallback(
                        AuthenticatorStatus.NOT_ALLOWED_ERROR,
                        GetAssertionOutcome.SECURITY_ERROR,
                        /* credentialRequestResult= */ null);
                return;
            }
            if (webContents != null && webContents.isIncognito()) {
                log(TAG, "Immediate Get called in Incognito mode");
                mBarrier.setImmediateIncognito();
            }
        }

        @Nullable Origin remoteDesktopOrigin = null;
        if (publicKeyOptions.extensions.remoteDesktopClientOverride != null
                && isChrome(mAuthenticationContextProvider.getWebContents())) {
            // SECURITY: remoteDesktopClientOverride comes from the renderer process and is
            // untrusted. We only use the override origin if the "caller origin" is explicitly
            // allowlisted with an enterprise policy.
            // This validation happens in the security checker's ValidateDomainAndRelyingPartyID
            // method.
            remoteDesktopOrigin =
                    new Origin(publicKeyOptions.extensions.remoteDesktopClientOverride.origin);
        }
        mCancellableUiState = CancellableUiState.WAITING_FOR_RP_ID_VALIDATION;
        frameHost.performGetAssertionWebAuthSecurityChecks(
                publicKeyOptions.relyingPartyId,
                origin,
                payment != null,
                remoteDesktopOrigin,
                (results) -> {
                    if (mCancellableUiState
                            == CancellableUiState.CANCEL_PENDING_RP_ID_VALIDATION_COMPLETE) {
                        // This request was canceled while waiting for RP ID validation to
                        // complete.
                        returnErrorAndResetCallback(
                                AuthenticatorStatus.ABORT_ERROR,
                                /* response= */ null,
                                /* credentialRequestResult= */ null);
                        return;
                    }
                    mCancellableUiState = CancellableUiState.NONE;
                    if (results.securityCheckResult != AuthenticatorStatus.SUCCESS) {
                        returnErrorAndResetCallback(
                                results.securityCheckResult,
                                GetAssertionOutcome.SECURITY_ERROR,
                                /* credentialRequestResult= */ null);
                        return;
                    }
                    continueGetCredentialRequestAfterRpIdValidation(
                            options, origin, topOrigin, payment, results.isCrossOrigin);
                });
    }

    @SuppressWarnings("NewApi")
    private void continueGetCredentialRequestAfterRpIdValidation(
            GetCredentialOptions options,
            Origin origin,
            @Nullable Origin topOrigin,
            @Nullable PaymentOptions payment,
            boolean isCrossOrigin) {
        log(TAG, "continueGetCredentialRequestAfterRpIdValidation");
        PublicKeyCredentialRequestOptions publicKeyOptions = assumeNonNull(options.publicKey);
        boolean hasAllowCredentials =
                publicKeyOptions.allowCredentials != null
                        && publicKeyOptions.allowCredentials.length != 0;

        if (!hasAllowCredentials) {
            // No UVM support for discoverable credentials.
            publicKeyOptions.extensions.userVerificationMethods = false;
        }

        if (publicKeyOptions.extensions.appid != null) {
            mAppIdExtensionUsed = true;
        }

        final String callerOriginString = convertOriginToString(origin);
        byte[] clientDataHash = null;
        if (!is(mAuthenticationContextProvider.getWebContents(), WebauthnMode.APP)) {
            assert publicKeyOptions.challenge != null;

            boolean effectiveCrossOrigin = isCrossOrigin;
            String effectiveOriginString = callerOriginString;
            // Handle remote desktop client override for ClientDataJSON.
            // The origin from remoteDesktopClientOverride is only used after validation in
            // ValidateDomainAndRelyingPartyID() confirmed that the "caller origin" is allowlisted.
            if (publicKeyOptions.extensions.remoteDesktopClientOverride != null) {
                effectiveOriginString =
                        convertOriginToString(
                                new Origin(
                                        publicKeyOptions
                                                .extensions
                                                .remoteDesktopClientOverride
                                                .origin));
                effectiveCrossOrigin =
                        !publicKeyOptions
                                .extensions
                                .remoteDesktopClientOverride
                                .sameOriginWithAncestors;
            }
            clientDataHash =
                    buildClientDataJsonAndComputeHash(
                            (payment != null)
                                    ? ClientDataRequestType.PAYMENT_GET
                                    : ClientDataRequestType.WEB_AUTHN_GET,
                            effectiveOriginString,
                            publicKeyOptions.challenge,
                            effectiveCrossOrigin,
                            payment,
                            publicKeyOptions.relyingPartyId,
                            topOrigin);
            if (clientDataHash == null) {
                returnErrorAndResetCallback(
                        AuthenticatorStatus.NOT_ALLOWED_ERROR,
                        /* response= */ null,
                        /* credentialRequestResult= */ null);
                return;
            }
        } else {
            assert payment == null;
        }

        if (!isChrome(mAuthenticationContextProvider.getWebContents())) {
            if (options.mediation == Mediation.CONDITIONAL) {
                returnErrorAndResetCallback(
                        AuthenticatorStatus.NOT_IMPLEMENTED,
                        /* response= */ null,
                        /* credentialRequestResult= */ null);
                return;
            }
            if (CredManSupportProvider.getCredManSupportForWebView() == CredManSupport.DISABLED) {
                if (!mPlayServicesAvailable) {
                    logError(TAG, "Google Play Services' Fido2 Api is not available.");
                    returnErrorAndResetCallback(
                            AuthenticatorStatus.UNKNOWN_ERROR,
                            /* response= */ null,
                            /* credentialRequestResult= */ null);
                    return;
                }
                maybeDispatchGetAssertionRequest(
                        options,
                        callerOriginString,
                        clientDataHash,
                        null,
                        Fido2ApiRequestType.GET_ASSERTION);
                return;
            }
            int result =
                    mCredManHelper.startGetRequest(
                            options,
                            callerOriginString,
                            mClientDataJson,
                            clientDataHash,
                            /* ignoreGpm= */ false);
            if (result != AuthenticatorStatus.SUCCESS) {
                returnErrorAndResetCallback(
                        result,
                        /* response= */ null,
                        CredentialRequestResult.ANDROID_CRED_MAN_ERROR);
            }
            return;
        }

        // Payments should still go through Google Play Services.
        final byte[] finalClientDataHash = clientDataHash;
        if (payment == null && getBarrierMode() == Barrier.Mode.ONLY_CRED_MAN) {
            if (options.mediation == Mediation.CONDITIONAL
                    || options.mediation == Mediation.IMMEDIATE) {
                if (options.mediation == Mediation.IMMEDIATE) {
                    startImmediateTimer();
                }
                mBarrier.resetAndSetWaitStatus(Barrier.Mode.ONLY_CRED_MAN);
                mCredManHelper.startPrefetchRequest(
                        options,
                        convertOriginToString(origin),
                        mClientDataJson,
                        clientDataHash,
                        mBarrier,
                        this::stopImmediateTimer,
                        /* ignoreGpm= */ false);
            } else if (hasAllowCredentials && mPlayServicesAvailable) {
                // If the allowlist contains non-discoverable credentials then
                // the request needs to be routed directly to Play Services.
                if (is(mAuthenticationContextProvider.getWebContents(), WebauthnMode.CHROME)) {
                    mCredManHelper.setNoCredentialsFallback(
                            () ->
                                    this.maybeDispatchGetAssertionRequest(
                                            options,
                                            convertOriginToString(origin),
                                            finalClientDataHash,
                                            /* credentialId= */ null,
                                            Fido2ApiRequestType.GET_ASSERTION));
                }
                checkForMatchingCredentials(options, origin, clientDataHash);
            } else {
                if (is(mAuthenticationContextProvider.getWebContents(), WebauthnMode.CHROME)) {
                    // WebauthnMode.CHROME_3PP_ENABLED will keep using CredMan's no credentials UI.
                    mCredManHelper.setNoCredentialsFallback(
                            () ->
                                    this.maybeDispatchGetAssertionRequest(
                                            options,
                                            convertOriginToString(origin),
                                            finalClientDataHash,
                                            /* credentialId= */ null,
                                            Fido2ApiRequestType.GET_ASSERTION));
                } else {
                    mCredManHelper.setNoCredentialsFallback(null);
                }
                int response =
                        mCredManHelper.startGetRequest(
                                options,
                                convertOriginToString(origin),
                                mClientDataJson,
                                clientDataHash,
                                /* ignoreGpm= */ false);
                if (response != AuthenticatorStatus.SUCCESS) {
                    returnErrorAndResetCallback(
                            response,
                            /* response= */ null,
                            CredentialRequestResult.ANDROID_CRED_MAN_ERROR);
                }
            }
            return;
        }

        if (!mPlayServicesAvailable) {
            logError(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(
                    AuthenticatorStatus.UNKNOWN_ERROR,
                    /* response= */ null,
                    /* credentialRequestResult= */ null);
            return;
        }

        // Conditional requests for Chrome 3rd party PWM mode when CredMan not enabled is not
        // defined yet.
        WebContents webContents = mAuthenticationContextProvider.getWebContents();
        if (options.mediation == Mediation.CONDITIONAL
                && is(webContents, WebauthnMode.CHROME_3PP_ENABLED)) {
            returnErrorAndResetCallback(
                    AuthenticatorStatus.NOT_IMPLEMENTED,
                    /* response= */ null,
                    /* credentialRequestResult= */ null);
            return;
        }

        // Enumerate credentials from Play Services so that we can show the picker in Chrome UI.
        // Chrome 3rd party mode does not support enumeration in Chrome UI, hence use FIDO2
        // enumeration for them.
        if ((options.mediation == Mediation.CONDITIONAL || !hasAllowCredentials)
                && is(webContents, WebauthnMode.CHROME)) {
            if (getBarrierMode() == Barrier.Mode.BOTH) {
                mBarrier.resetAndSetWaitStatus(Barrier.Mode.BOTH);
                mCredManHelper.startPrefetchRequest(
                        options,
                        callerOriginString,
                        mClientDataJson,
                        clientDataHash,
                        mBarrier,
                        null,
                        /* ignoreGpm= */ true);
            } else {
                mBarrier.resetAndSetWaitStatus(Barrier.Mode.ONLY_FIDO_2_API);
            }
            mCancellableUiState = CancellableUiState.WAITING_FOR_CREDENTIAL_LIST;
            GmsCoreGetCredentialsHelper.Reason reason;
            if (payment != null) {
                reason = GmsCoreGetCredentialsHelper.Reason.PAYMENT;
            } else if (publicKeyOptions.relyingPartyId.equals("google.com")) {
                reason = GmsCoreGetCredentialsHelper.Reason.GET_ASSERTION_GOOGLE_RP;
            } else {
                reason = GmsCoreGetCredentialsHelper.Reason.GET_ASSERTION_NON_GOOGLE;
            }
            if (options.mediation == Mediation.IMMEDIATE) {
                startImmediateTimer();
            }
            GmsCoreGetCredentialsHelper.getInstance()
                    .getCredentials(
                            mAuthenticationContextProvider,
                            publicKeyOptions.relyingPartyId,
                            reason,
                            (credentials) ->
                                    mBarrier.onFido2ApiSuccessful(
                                            () ->
                                                    onWebauthnCredentialDetailsListReceived(
                                                            options,
                                                            callerOriginString,
                                                            finalClientDataHash,
                                                            credentials)),
                            (e) ->
                                    mBarrier.onFido2ApiFailed(
                                            AuthenticatorStatus.NOT_ALLOWED_ERROR));
            return;
        }

        if (hasAllowCredentials
                && options.mediation != Mediation.CONDITIONAL
                && getBarrierMode() == Barrier.Mode.BOTH) {
            checkForMatchingCredentials(options, origin, clientDataHash);
            return;
        }
        maybeDispatchGetAssertionRequest(
                options,
                callerOriginString,
                clientDataHash,
                null,
                Fido2ApiRequestType.GET_ASSERTION);
    }

    public void cancelGetAssertion() {
        log(TAG, "cancelGetAssertion");
        mCredManHelper.cancelGetAssertion(AuthenticatorStatus.ABORT_ERROR);

        switch (mCancellableUiState) {
            case CancellableUiState.WAITING_FOR_RP_ID_VALIDATION:
                mCancellableUiState = CancellableUiState.CANCEL_PENDING_RP_ID_VALIDATION_COMPLETE;
                break;
            case CancellableUiState.WAITING_FOR_CREDENTIAL_LIST:
                mCancellableUiState = CancellableUiState.CANCEL_PENDING;
                mBarrier.onFido2ApiCancelled();
                break;
            case CancellableUiState.WAITING_FOR_SELECTION:
                assumeNonNull(getBridge());
                getBridge().cleanupRequest(mAuthenticationContextProvider.getRenderFrameHost());
                mCancellableUiState = CancellableUiState.NONE;
                mBarrier.onFido2ApiCancelled();
                break;
            case CancellableUiState.REQUEST_SENT_TO_PLATFORM:
                // If the platform successfully completes the getAssertion then cancelation is
                // ignored, but if it returns an error then CANCEL_PENDING removes the option to
                // try again.
                mCancellableUiState = CancellableUiState.CANCEL_PENDING;
                break;
            default:
                // No action
        }
    }

    public void handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
            IsUvpaaResponseCallback callback) {
        log(TAG, "handleIsUserVerifyingPlatformAuthenticatorAvailableRequest");
        boolean chromeRequest = isChrome(mAuthenticationContextProvider.getWebContents());
        if ((!chromeRequest
                        && CredManSupportProvider.getCredManSupportForWebView()
                                == CredManSupport.FULL_UNLESS_INAPPLICABLE)
                || (chromeRequest && getBarrierMode() == Barrier.Mode.ONLY_CRED_MAN)) {
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(true);
            return;
        }

        if (!mPlayServicesAvailable) {
            logError(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            // Note that |IsUserVerifyingPlatformAuthenticatorAvailable| only returns
            // true or false, making it unable to handle any error status.
            // So it callbacks with false if Fido2PrivilegedApi is not available.
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
            return;
        }

        Fido2ApiCallParams params =
                WebauthnModeProvider.getInstance()
                        .getFido2ApiCallParams(mAuthenticationContextProvider.getWebContents());
        assertNonNull(mAuthenticationContextProvider.getContext());
        assertNonNull(params);
        Fido2ApiCall call = new Fido2ApiCall(mAuthenticationContextProvider.getContext(), params);
        Fido2ApiCall.BooleanResult result = new Fido2ApiCall.BooleanResult();
        Parcel args = call.start();
        args.writeStrongBinder(result);

        Task<Boolean> task =
                call.run(
                        params.mIsUserVerifyingPlatformAuthenticatorAvailableMethodId,
                        Fido2ApiCall.TRANSACTION_ISUVPAA,
                        args,
                        result);
        task.addOnSuccessListener(
                (isUvpaa) -> {
                    callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(isUvpaa);
                });
        task.addOnFailureListener(
                (e) -> {
                    logError(TAG, "FIDO2 API call failed", e);
                    callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
                });
    }

    public void handleReportRequest(PublicKeyCredentialReportOptions options, Origin origin) {
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        assert frameHost != null;

        if (options.unknownCredentialId == null
                && options.allAcceptedCredentials == null
                && options.currentUserDetails == null) {
            returnErrorAndResetCallback(
                    AuthenticatorStatus.UNKNOWN_ERROR,
                    /* response= */ null,
                    /* credentialRequestResult= */ null);
            return;
        }

        frameHost.performReportWebAuthSecurityChecks(
                options.relyingPartyId,
                origin,
                (results) -> {
                    log(
                            TAG,
                            "handleReportRequest.onSecurityCheckComplete, isCrossOrigin: %b,"
                                    + " securityCheckResult: %d",
                            results.isCrossOrigin,
                            results.securityCheckResult);

                    WebauthnRequestCallback callback =
                            assumeNonNull(mAuthenticationContextProvider.getRequestCallback());
                    if (results.securityCheckResult != AuthenticatorStatus.SUCCESS) {
                        returnErrorAndResetCallback(
                                results.securityCheckResult,
                                /* response= */ null,
                                /* credentialRequestResult= */ null);
                        return;
                    }
                    mIdentityCredentialsHelper.handleReportRequest(
                            options, convertOriginToString(origin));
                    callback.onComplete(
                            WebauthnRequestResponse.forReport(AuthenticatorStatus.SUCCESS));
                });
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

    public void setImmediateTimerForTesting(RunnableTimer timer) {
        mImmediateTimer = timer;
    }

    private void onWebauthnCredentialDetailsListReceived(
            GetCredentialOptions options,
            String callerOriginString,
            byte @Nullable [] clientDataHash,
            List<WebauthnCredentialDetails> credentials) {
        log(TAG, "onWebauthnCredentialDetailsListReceived");
        assert mCancellableUiState == CancellableUiState.WAITING_FOR_CREDENTIAL_LIST
                || mCancellableUiState == CancellableUiState.CANCEL_PENDING;
        boolean isConditionalRequest = options.mediation == Mediation.CONDITIONAL;
        boolean isImmediateRequest = options.mediation == Mediation.IMMEDIATE;

        if (mCancellableUiState == CancellableUiState.CANCEL_PENDING) {
            // The request was completed synchronously when the cancellation was received,
            // so no need to return an error to the renderer.
            mCancellableUiState = CancellableUiState.NONE;
            return;
        }

        stopImmediateTimer();

        List<WebauthnCredentialDetails> discoverableCredentials = new ArrayList<>();
        if (options.publicKey != null) {
            PublicKeyCredentialRequestOptions publicKeyOptions = options.publicKey;
            boolean hasAllowCredentials =
                    publicKeyOptions.allowCredentials != null
                            && publicKeyOptions.allowCredentials.length != 0;
            assert isConditionalRequest || !hasAllowCredentials;

            for (WebauthnCredentialDetails credential : credentials) {
                if (!credential.mIsDiscoverable) continue;

                if (!hasAllowCredentials) {
                    discoverableCredentials.add(credential);
                    continue;
                }

                for (PublicKeyCredentialDescriptor descriptor : publicKeyOptions.allowCredentials) {
                    if (Arrays.equals(credential.mCredentialId, descriptor.id)) {
                        discoverableCredentials.add(credential);
                        break;
                    }
                }
            }
        }

        if (!isConditionalRequest
                && !isImmediateRequest
                && discoverableCredentials.isEmpty()
                && getBarrierMode() != Barrier.Mode.BOTH) {
            mCancellableUiState = CancellableUiState.NONE;
            // When no passkeys are present for a non-conditional non-immediate request pass the
            // request through to GMSCore. It will show an error message to the user, but can offer
            // the user alternatives to use external passkeys.
            // If the barrier mode is BOTH, the no passkeys state is handled by Chrome. Do not pass
            // the request to GMSCore.
            maybeDispatchGetAssertionRequest(
                    options,
                    callerOriginString,
                    clientDataHash,
                    null,
                    Fido2ApiRequestType.GET_ASSERTION);
            return;
        }

        Runnable hybridCallback = null;
        if (GmsCoreUtils.isHybridClientApiSupported() && options.publicKey != null) {
            PublicKeyCredentialRequestOptions publicKeyOptions = options.publicKey;
            hybridCallback =
                    () ->
                            dispatchHybridGetAssertionRequest(
                                    publicKeyOptions,
                                    callerOriginString,
                                    clientDataHash,
                                    isConditionalRequest);
        }

        @AssertionMediationType int mediationType = AssertionMediationType.MODAL;
        if (isConditionalRequest) {
            mediationType = AssertionMediationType.CONDITIONAL;
        } else if (isImmediateRequest) {
            if (options.password) {
                mediationType = AssertionMediationType.IMMEDIATE_WITH_PASSWORDS;
            } else {
                if (discoverableCredentials.isEmpty()) {
                    log(TAG, "Immediate Get request did not display UI: no passkeys found");
                    // Since passwords were not requested as a part of this immediate request, we
                    // already know there are no credentials to provide, so the request can be
                    // rejected now.
                    returnErrorAndResetCallback(
                            AuthenticatorStatus.NOT_ALLOWED_ERROR,
                            /* response= */ null,
                            /* credentialRequestResult= */ null);
                    return;
                }
                mediationType = AssertionMediationType.IMMEDIATE_PASSKEYS_ONLY;
            }
        }

        mCancellableUiState = CancellableUiState.WAITING_FOR_SELECTION;
        assumeNonNull(getBridge());
        getBridge()
                .onCredentialsDetailsListReceived(
                        mAuthenticationContextProvider.getRenderFrameHost(),
                        discoverableCredentials,
                        mediationType,
                        (selectedCredential) -> {
                            if (selectedCredential.webAuthnCredential() != null) {
                                maybeDispatchGetAssertionRequest(
                                        options,
                                        callerOriginString,
                                        clientDataHash,
                                        selectedCredential.webAuthnCredential(),
                                        Fido2ApiRequestType.GET_ASSERTION);
                            } else {
                                assertNonNull(selectedCredential.passwordCredential());
                                WebauthnRequestCallback callback =
                                        mAuthenticationContextProvider.getRequestCallback();
                                if (callback != null) {
                                    callback.onComplete(
                                            WebauthnRequestResponse.forSuccessfulPassword(
                                                    selectedCredential.passwordCredential()));
                                }
                            }
                        },
                        hybridCallback,
                        (reason) -> handleNonCredentialReturn(options, reason));
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
            GetCredentialOptions options, Origin callerOrigin, byte @Nullable [] clientDataHash) {
        log(TAG, "checkForMatchingCredentials");
        PublicKeyCredentialRequestOptions publicKeyOptions = assumeNonNull(options.publicKey);
        assert publicKeyOptions.allowCredentials != null;
        assert publicKeyOptions.allowCredentials.length > 0;
        assert options.mediation != Mediation.CONDITIONAL;
        assert mPlayServicesAvailable;
        @Barrier.Mode int mode = getBarrierMode();
        assert mode == Barrier.Mode.ONLY_CRED_MAN || mode == Barrier.Mode.BOTH;

        GmsCoreGetCredentialsHelper.getInstance()
                .getCredentials(
                        mAuthenticationContextProvider,
                        publicKeyOptions.relyingPartyId,
                        GmsCoreGetCredentialsHelper.Reason.CHECK_FOR_MATCHING_CREDENTIALS,
                        (credentials) ->
                                checkForMatchingCredentialsReceived(
                                        options, callerOrigin, clientDataHash, credentials),
                        (e) -> {
                            logError(
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
                                    mode == Barrier.Mode.BOTH);
                        });
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private void checkForMatchingCredentialsReceived(
            GetCredentialOptions options,
            Origin callerOrigin,
            byte @Nullable [] clientDataHash,
            List<WebauthnCredentialDetails> retrievedCredentials) {
        log(TAG, "checkForMatchingCredentialsReceived");
        PublicKeyCredentialRequestOptions publicKeyOptions = assumeNonNull(options.publicKey);
        assert publicKeyOptions.allowCredentials != null;
        assert publicKeyOptions.allowCredentials.length > 0;
        assert options.mediation != Mediation.CONDITIONAL;
        assert mPlayServicesAvailable;
        @Barrier.Mode int mode = getBarrierMode();
        assert mode == Barrier.Mode.ONLY_CRED_MAN || mode == Barrier.Mode.BOTH;

        for (WebauthnCredentialDetails credential : retrievedCredentials) {
            // In ONLY_CRED_MAN mode, all discoverable credentials are handled by CredMan. It is not
            // required to check for discoverable credentials.
            if (mode == Barrier.Mode.ONLY_CRED_MAN && credential.mIsDiscoverable) {
                continue;
            }

            for (PublicKeyCredentialDescriptor allowedId : publicKeyOptions.allowCredentials) {
                if (allowedId.type != PublicKeyCredentialType.PUBLIC_KEY) {
                    continue;
                }

                if (Arrays.equals(allowedId.id, credential.mCredentialId)) {
                    // This get() request can be satisfied by Play Services with
                    // a non-discoverable credential so route it there.
                    RecordHistogram.recordBooleanHistogram(
                            "WebAuthentication.Android.NonDiscoverableCredentialsFound", true);
                    maybeDispatchGetAssertionRequest(
                            options,
                            convertOriginToString(callerOrigin),
                            clientDataHash,
                            /* credentialId= */ null,
                            Fido2ApiRequestType.GET_ASSERTION_LEGACY);
                    return;
                }
            }
        }
        RecordHistogram.recordBooleanHistogram(
                "WebAuthentication.Android.NonDiscoverableCredentialsFound", false);

        mCredManHelper.setNoCredentialsFallback(
                () ->
                        this.maybeDispatchGetAssertionRequest(
                                options,
                                convertOriginToString(callerOrigin),
                                clientDataHash,
                                /* credentialId= */ null,
                                Fido2ApiRequestType.GET_ASSERTION));
        // No elements of the allowlist are local, non-discoverable credentials
        // so route to CredMan.
        mCredManHelper.startGetRequest(
                options,
                convertOriginToString(callerOrigin),
                mClientDataJson,
                clientDataHash,
                mode == Barrier.Mode.BOTH);
    }

    private void maybeDispatchGetAssertionRequest(
            GetCredentialOptions options,
            String callerOriginString,
            byte @Nullable [] clientDataHash,
            byte @Nullable [] credentialId,
            @Fido2ApiRequestType int requestType) {
        log(TAG, "maybeDispatchGetAssertionRequest for request type: %d", requestType);
        PublicKeyCredentialRequestOptions publicKeyOptions = assumeNonNull(options.publicKey);
        assert mCancellableUiState == CancellableUiState.NONE
                || mCancellableUiState == CancellableUiState.REQUEST_SENT_TO_PLATFORM
                || mCancellableUiState == CancellableUiState.WAITING_FOR_SELECTION;

        // If this is called a second time while the first sign-in attempt is still outstanding,
        // ignore the second call.
        if (mCancellableUiState == CancellableUiState.REQUEST_SENT_TO_PLATFORM) {
            logError(
                    TAG,
                    "Received a second credential selection while the first still in progress.");
            return;
        }

        mCancellableUiState = CancellableUiState.NONE;
        if (credentialId != null) {
            assert (credentialId.length > 0);
            PublicKeyCredentialDescriptor selectedCredential = new PublicKeyCredentialDescriptor();
            selectedCredential.type = PublicKeyCredentialType.PUBLIC_KEY;
            selectedCredential.id = credentialId;
            selectedCredential.transports = new int[] {AuthenticatorTransport.INTERNAL};
            publicKeyOptions.allowCredentials =
                    new PublicKeyCredentialDescriptor[] {selectedCredential};
        }

        if (options.mediation == Mediation.CONDITIONAL) {
            mCancellableUiState = CancellableUiState.REQUEST_SENT_TO_PLATFORM;
        }

        Fido2ApiCallHelper.getInstance()
                .invokeFido2GetAssertion(
                        mAuthenticationContextProvider,
                        publicKeyOptions,
                        Uri.parse(callerOriginString),
                        clientDataHash,
                        getMaybeResultReceiver(requestType),
                        (pendingIntent) -> onGotPendingIntent(pendingIntent, requestType),
                        (e) -> onBinderCallException(e, requestType));
    }

    private void handleNonCredentialReturn(GetCredentialOptions options, Integer reason) {
        if (options.mediation == Mediation.IMMEDIATE) {
            log(TAG, "Immediate Get request did not display UI: Code " + reason);
            // TODO(https://crbug.com/433543129): Add metrics for the rejection reason
            // in order to distinguish user dismissal from no credentials being
            // available.
            returnErrorAndResetCallback(
                    AuthenticatorStatus.NOT_ALLOWED_ERROR,
                    /* response= */ null,
                    /* credentialRequestResult= */ null);
            return;
        }

        if (reason == NonCredentialReturnReason.ERROR) {
            logError(TAG, "Bottom sheet not displayed due to an error.");
            assumeNonNull(getBridge());
            getBridge().cleanupRequest(mAuthenticationContextProvider.getRenderFrameHost());
            returnErrorAndResetCallback(
                    AuthenticatorStatus.UNKNOWN_ERROR,
                    /* response= */ null,
                    /* credentialRequestResult= */ null);
            return;
        }

        // Conditional mediation should never return from the user closing the sheet.
        assert (options.mediation != Mediation.CONDITIONAL);
        assert (reason == NonCredentialReturnReason.USER_DISMISSED);
        returnErrorAndResetCallback(
                AuthenticatorStatus.NOT_ALLOWED_ERROR,
                GetAssertionOutcome.USER_CANCELLATION,
                CredentialRequestResult.USER_CANCELLED);
    }

    private void dispatchHybridGetAssertionRequest(
            PublicKeyCredentialRequestOptions options,
            String callerOriginString,
            byte @Nullable [] clientDataHash,
            boolean isConditional) {
        log(TAG, "dispatchHybridGetAssertionRequest");
        assert mCancellableUiState == CancellableUiState.NONE
                || mCancellableUiState == CancellableUiState.REQUEST_SENT_TO_PLATFORM
                || mCancellableUiState == CancellableUiState.WAITING_FOR_SELECTION;

        if (mCancellableUiState == CancellableUiState.REQUEST_SENT_TO_PLATFORM) {
            logError(
                    TAG,
                    "Received a second credential selection while the first still in progress.");
            return;
        }
        if (isConditional) {
            mCancellableUiState = CancellableUiState.REQUEST_SENT_TO_PLATFORM;
        } else {
            mCancellableUiState = CancellableUiState.NONE;
        }

        Fido2ApiCallHelper.getInstance()
                .invokeFido2HybridGetAssertion(
                        mAuthenticationContextProvider,
                        options,
                        Uri.parse(callerOriginString),
                        clientDataHash,
                        (pendingIntent) ->
                                onGotPendingIntent(
                                        pendingIntent, Fido2ApiRequestType.GET_ASSERTION_HYBRID),
                        (e) -> onBinderCallException(e, Fido2ApiRequestType.GET_ASSERTION_HYBRID));
    }

    // Handles a PendingIntent from the GMSCore FIDO library.
    private void onGotPendingIntent(
            PendingIntent pendingIntent, @Fido2ApiRequestType int requestType) {
        log(TAG, "onGotPendingIntent");
        if (pendingIntent == null) {
            logError(TAG, "Didn't receive a pending intent.");
            returnErrorAndResetCallback(
                    AuthenticatorStatus.UNKNOWN_ERROR,
                    /* response= */ null,
                    getCredentialRequestResult(
                            requestType, /* success= */ false, /* ukmOutcome= */ null));
            return;
        }

        Callback<Pair<Integer, @Nullable Intent>> callback =
                (result) -> onResult((Pair<Integer, @Nullable Intent>) result, requestType);
        if (!mAuthenticationContextProvider.getIntentSender().showIntent(pendingIntent, callback)) {
            logError(TAG, "Failed to send intent to FIDO API");
            returnErrorAndResetCallback(
                    AuthenticatorStatus.UNKNOWN_ERROR,
                    /* response= */ null,
                    getCredentialRequestResult(
                            requestType, /* success= */ false, /* ukmOutcome= */ null));
            return;
        }
    }

    private void onBinderCallException(Exception e, @Fido2ApiRequestType int requestType) {
        logError(TAG, "FIDO2 API call failed", e);
        returnErrorAndResetCallback(
                AuthenticatorStatus.NOT_ALLOWED_ERROR,
                /* response= */ null,
                getCredentialRequestResult(
                        requestType, /* success= */ false, /* ukmOutcome= */ null));
    }

    private @Nullable ResultReceiver getMaybeResultReceiver(@Fido2ApiRequestType int requestType) {
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
                onResultReceiverResult(resultData, requestType);
            }
        };
    }

    private void onResultReceiverResult(Bundle resultData, @Fido2ApiRequestType int requestType) {
        log(TAG, "onResultReceiverResult");
        int errorCode = AuthenticatorStatus.UNKNOWN_ERROR;
        Object response = null;
        byte[] responseBytes = resultData.getByteArray(Fido2Api.CREDENTIAL_EXTRA);
        if (responseBytes != null) {
            try {
                response = Fido2Api.parseResponse(responseBytes);
            } catch (IllegalArgumentException e) {
                logError(TAG, "Failed to parse FIDO2 API response from ResultReceiver", e);
                response = null;
            }
        }

        Integer browserAssistedLoginType = null;
        if (WebauthnFeatureMap.getInstance()
                .isEnabled(WebauthnFeatures.WEBAUTHN_ANDROID_CRED_MAN_REQUEST_EXTRA_BUNDLE)) {
            if (requestType == Fido2ApiRequestType.GET_ASSERTION) {
                browserAssistedLoginType = BrowserAssistedLoginType.PASSKEY_STORED_IN_GPM;
            } else if (requestType == Fido2ApiRequestType.GET_ASSERTION_HYBRID) {
                browserAssistedLoginType = BrowserAssistedLoginType.PASSKEY_HYBRID_OR_SECURITY_KEY;
            } else if (requestType == Fido2ApiRequestType.GET_ASSERTION_LEGACY) {
                browserAssistedLoginType = BrowserAssistedLoginType.PASSKEY_UNKNOWN;
            }
        }

        handleFido2Response(
                errorCode,
                response,
                /* getAssertionOutcome= */ null,
                /* makeCredentialOutcome= */ null,
                requestType,
                browserAssistedLoginType);
    }

    private @Nullable Integer getCredentialRequestResult(
            @Fido2ApiRequestType int requestType, boolean success, @Nullable Integer ukmOutcome) {
        log(TAG, "getCredentialRequestResult: success: " + success + " ukmOutcome: " + ukmOutcome);
        if (!success) {
            if (ukmOutcome != null && ukmOutcome == GetAssertionOutcome.USER_CANCELLATION) {
                return CredentialRequestResult.USER_CANCELLED;
            }
            if (ukmOutcome != null && ukmOutcome == GetAssertionOutcome.UI_TIMEOUT) {
                return CredentialRequestResult.TIMEOUT;
            }
            if (ukmOutcome != null && ukmOutcome == MakeCredentialOutcome.USER_CANCELLATION) {
                return CredentialRequestResult.USER_CANCELLED;
            }
            if (ukmOutcome != null && ukmOutcome == MakeCredentialOutcome.UI_TIMEOUT) {
                return CredentialRequestResult.TIMEOUT;
            }
        }
        if (requestType == Fido2ApiRequestType.GET_ASSERTION) {
            return success
                    ? CredentialRequestResult.ANDROID_FIDO2_SUCCESS
                    : CredentialRequestResult.ANDROID_FIDO2_ERROR;
        } else if (requestType == Fido2ApiRequestType.GET_ASSERTION_HYBRID) {
            return success
                    ? CredentialRequestResult.ANDROID_FIDO2_HYBRID_SUCCESS
                    : CredentialRequestResult.ANDROID_FIDO2_HYBRID_ERROR;
        } else if (requestType == Fido2ApiRequestType.GET_ASSERTION_LEGACY) {
            return success
                    ? CredentialRequestResult.ANDROID_FIDO2_LEGACY_SUCCESS
                    : CredentialRequestResult.ANDROID_FIDO2_LEGACY_ERROR;
        } else if (requestType == Fido2ApiRequestType.MAKE_CREDENTIAL) {
            return success
                    ? CredentialRequestResult.ANDROID_FIDO2_SUCCESS
                    : CredentialRequestResult.ANDROID_FIDO2_ERROR;
        }
        return null;
    }

    // Handles the result.
    public void onResult(
            Pair<Integer, @Nullable Intent> result, @Fido2ApiRequestType int requestType) {
        log(TAG, "onResult");
        final int resultCode = result.first;
        final @Nullable Intent data = result.second;
        int errorCode = AuthenticatorStatus.UNKNOWN_ERROR;
        Object response = null;

        assert mCancellableUiState == CancellableUiState.NONE
                || mCancellableUiState == CancellableUiState.REQUEST_SENT_TO_PLATFORM
                || mCancellableUiState == CancellableUiState.CANCEL_PENDING;

        @GetAssertionOutcome Integer getAssertionOutcome = null;
        @MakeCredentialOutcome Integer makeCredentialOutcome = null;
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
                WebauthnRequestCallback callback =
                        assumeNonNull(mAuthenticationContextProvider.getRequestCallback());
                switch (callback.getCallbackType()) {
                    case WebauthnRequestCallback.CallbackType.GET_CREDENTIAL:
                        getAssertionOutcome = GetAssertionOutcome.USER_CANCELLATION;
                        break;
                    case WebauthnRequestCallback.CallbackType.MAKE_CREDENTIAL:
                        makeCredentialOutcome = MakeCredentialOutcome.USER_CANCELLATION;
                        break;
                    case WebauthnRequestCallback.CallbackType.REPORT:
                        assert false;
                        break;
                }
                errorCode = AuthenticatorStatus.NOT_ALLOWED_ERROR;
                break;

            default:
                logError(TAG, "FIDO2 PendingIntent resulted in code: " + resultCode);
                break;
        }

        Integer browserAssistedLoginType = null;
        if (WebauthnFeatureMap.getInstance()
                .isEnabled(WebauthnFeatures.WEBAUTHN_ANDROID_CRED_MAN_REQUEST_EXTRA_BUNDLE)) {
            if (requestType == Fido2ApiRequestType.GET_ASSERTION) {
                browserAssistedLoginType = BrowserAssistedLoginType.PASSKEY_STORED_IN_GPM;
            } else if (requestType == Fido2ApiRequestType.GET_ASSERTION_HYBRID) {
                browserAssistedLoginType = BrowserAssistedLoginType.PASSKEY_HYBRID_OR_SECURITY_KEY;
            } else if (requestType == Fido2ApiRequestType.GET_ASSERTION_LEGACY) {
                browserAssistedLoginType = BrowserAssistedLoginType.PASSKEY_UNKNOWN;
            }
        }

        handleFido2Response(
                errorCode,
                response,
                getAssertionOutcome,
                makeCredentialOutcome,
                requestType,
                browserAssistedLoginType);
    }

    private void handleFido2Response(
            int errorCode,
            @Nullable Object response,
            @Nullable @GetAssertionOutcome Integer getAssertionOutcome,
            @Nullable @MakeCredentialOutcome Integer makeCredentialOutcome,
            @Fido2ApiRequestType int requestType,
            @Nullable Integer browserAssistedLoginType) {
        log(TAG, "handleFido2Response");
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        if (mCancellableUiState != CancellableUiState.NONE) {
            if (response == null || response instanceof Pair) {
                if (response != null) {
                    Pair<Integer, String> error = (Pair<Integer, String>) response;
                    logError(
                            TAG,
                            "FIDO2 API call resulted in error: "
                                    + error.first
                                    + " "
                                    + (error.second != null ? error.second : ""));
                    errorCode = convertError(error);
                }

                if (mCancellableUiState == CancellableUiState.CANCEL_PENDING) {
                    mCancellableUiState = CancellableUiState.NONE;
                    assumeNonNull(getBridge());
                    getBridge().cleanupRequest(frameHost);
                    mBarrier.onFido2ApiCancelled();
                } else {
                    // The user can try again by selecting another conditional UI credential.
                    mCancellableUiState = CancellableUiState.WAITING_FOR_SELECTION;
                }
                return;
            }
            mCancellableUiState = CancellableUiState.NONE;
            assumeNonNull(getBridge());
            getBridge().cleanupRequest(frameHost);
        }

        WebauthnRequestCallback requestCallback =
                mAuthenticationContextProvider.getRequestCallback();
        if (requestCallback == null) {
            return;
        }
        @WebauthnRequestCallback.CallbackType
        int requestCallbackType = assumeNonNull(requestCallback).getCallbackType();
        if (response == null) {
            // Use the error already set.
        } else if (response instanceof Pair) {
            Pair<Integer, String> error = (Pair<Integer, String>) response;
            logError(
                    TAG,
                    "FIDO2 API call resulted in error: "
                            + error.first
                            + " "
                            + (error.second != null ? error.second : ""));
            errorCode = convertError(error);
            if (requestCallbackType == WebauthnRequestCallback.CallbackType.GET_CREDENTIAL
                    && getAssertionOutcome == null) {
                getAssertionOutcome = getAssertionOutcomeCodeFromFidoError(error);
            } else if (requestCallbackType == WebauthnRequestCallback.CallbackType.MAKE_CREDENTIAL
                    && makeCredentialOutcome == null) {
                makeCredentialOutcome = makeCredentialOutcomeCodeFromFidoError(error);
            }
        } else if (requestCallbackType == WebauthnRequestCallback.CallbackType.MAKE_CREDENTIAL) {
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
                Integer makeCredentialResult =
                        getCredentialRequestResult(
                                requestType, /* success= */ true, /* ukmOutcome= */ null);
                RequestMetrics.Builder builder =
                        new RequestMetrics.Builder()
                                .setMakeCredentialOutcome(MakeCredentialOutcome.SUCCESS);
                if (makeCredentialResult != null) {
                    builder.setMakeCredentialResult(makeCredentialResult);
                }
                requestCallback.onComplete(
                        WebauthnRequestResponse.forSuccessfulMakeCredential(
                                creationResponse, builder.build()));
                return;
            }
        } else if (requestCallbackType == WebauthnRequestCallback.CallbackType.GET_CREDENTIAL) {
            if (response instanceof GetAssertionAuthenticatorResponse) {
                GetAssertionAuthenticatorResponse r = (GetAssertionAuthenticatorResponse) response;
                if (mClientDataJson != null) {
                    r.info.clientDataJson = mClientDataJson;
                    assumeNonNull(frameHost);
                    frameHost.notifyWebAuthnAssertionRequestSucceeded();
                }
                r.extensions.echoAppidExtension = mAppIdExtensionUsed;
                Integer getAssertionResult =
                        getCredentialRequestResult(
                                requestType, /* success= */ true, /* ukmOutcome= */ null);
                RequestMetrics.Builder builder =
                        new RequestMetrics.Builder()
                                .setGetAssertionOutcome(GetAssertionOutcome.SUCCESS);
                if (getAssertionResult != null) {
                    builder.setGetAssertionResult(getAssertionResult);
                }
                if (browserAssistedLoginType != null) {
                    builder.setBrowserAssistedLoginType(browserAssistedLoginType);
                }
                requestCallback.onComplete(
                        WebauthnRequestResponse.forSuccessfulGetAssertion(r, builder.build()));
                return;
            }
        }

        if (requestCallbackType == WebauthnRequestCallback.CallbackType.MAKE_CREDENTIAL) {
            returnErrorAndResetCallback(
                    errorCode,
                    makeCredentialOutcome,
                    getCredentialRequestResult(
                            requestType, /* success= */ false, makeCredentialOutcome));
        } else if (requestCallbackType == WebauthnRequestCallback.CallbackType.GET_CREDENTIAL) {
            returnErrorAndResetCallback(
                    errorCode,
                    getAssertionOutcome,
                    getCredentialRequestResult(
                            requestType, /* success= */ false, getAssertionOutcome));
        } else {
            returnErrorAndResetCallback(
                    errorCode, /* response= */ null, /* credentialRequestResult= */ null);
        }
    }

    @MakeCredentialOutcome
    int makeCredentialOutcomeCodeFromFidoError(Pair<Integer, String> error) {
        final int errorCode = error.first;
        final @Nullable String errorMsg = error.second;
        switch (errorCode) {
            case Fido2Api.SECURITY_ERR:
                return MakeCredentialOutcome.SECURITY_ERROR;
            case Fido2Api.TIMEOUT_ERR:
                return MakeCredentialOutcome.UI_TIMEOUT;
            case Fido2Api.NOT_ALLOWED_ERR:
                if (NON_EMPTY_ALLOWLIST_ERROR_MSG.equals(errorMsg)
                        || NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG.equals(errorMsg)) {
                    return MakeCredentialOutcome.RK_NOT_SUPPORTED;
                }
                if (CANCELLED_ERROR_MSG.equals(errorMsg)) {
                    return MakeCredentialOutcome.USER_CANCELLATION;
                }
                return MakeCredentialOutcome.PLATFORM_NOT_ALLOWED;
            case Fido2Api.CONSTRAINT_ERR:
                if (NO_SCREENLOCK_ERROR_MSG.equals(errorMsg)) {
                    return MakeCredentialOutcome.UV_NOT_SUPPORTED;
                }
                return MakeCredentialOutcome.OTHER_FAILURE;
            case Fido2Api.INVALID_STATE_ERR:
                if (CREDENTIAL_EXISTS_ERROR_MSG.equals(errorMsg)) {
                    return MakeCredentialOutcome.CREDENTIAL_EXCLUDED;
                }
            // else fallthrough.
            default:
                return MakeCredentialOutcome.OTHER_FAILURE;
        }
    }

    @GetAssertionOutcome
    int getAssertionOutcomeCodeFromFidoError(Pair<Integer, String> error) {
        final int errorCode = error.first;
        final @Nullable String errorMsg = error.second;
        switch (errorCode) {
            case Fido2Api.SECURITY_ERR:
                return GetAssertionOutcome.SECURITY_ERROR;
            case Fido2Api.TIMEOUT_ERR:
                return GetAssertionOutcome.UI_TIMEOUT;
            case Fido2Api.NOT_ALLOWED_ERR:
                if (NON_EMPTY_ALLOWLIST_ERROR_MSG.equals(errorMsg)
                        || NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG.equals(errorMsg)) {
                    return GetAssertionOutcome.RK_NOT_SUPPORTED;
                }
                if (CANCELLED_ERROR_MSG.equals(errorMsg)) {
                    return GetAssertionOutcome.USER_CANCELLATION;
                }
                return GetAssertionOutcome.PLATFORM_NOT_ALLOWED;
            case Fido2Api.CONSTRAINT_ERR:
                if (NO_SCREENLOCK_ERROR_MSG.equals(errorMsg)) {
                    return GetAssertionOutcome.UV_NOT_SUPPORTED;
                }
                return GetAssertionOutcome.OTHER_FAILURE;
            case Fido2Api.UNKNOWN_ERR:
                if (LOW_LEVEL_ERROR_MSG.equals(errorMsg)) {
                    return GetAssertionOutcome.CREDENTIAL_NOT_RECOGNIZED;
                }
            // else fallthrough.
            default:
                return GetAssertionOutcome.OTHER_FAILURE;
        }
    }

    /**
     * Helper method to convert AuthenticatorErrorResponse errors.
     *
     * @return error code corresponding to an AuthenticatorStatus.
     */
    private static int convertError(Pair<Integer, String> error) {
        final int errorCode = error.first;
        final @Nullable String errorMsg = error.second;

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
                if (NON_EMPTY_ALLOWLIST_ERROR_MSG.equals(errorMsg)
                        || NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG.equals(errorMsg)) {
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
                if (NO_SCREENLOCK_ERROR_MSG.equals(errorMsg)) {
                    return AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED;
                }
                return AuthenticatorStatus.UNKNOWN_ERROR;
            case Fido2Api.INVALID_STATE_ERR:
                if (CREDENTIAL_EXISTS_ERROR_MSG.equals(errorMsg)) {
                    return AuthenticatorStatus.CREDENTIAL_EXCLUDED;
                }
            // else fallthrough.
            case Fido2Api.UNKNOWN_ERR:
                if (LOW_LEVEL_ERROR_MSG.equals(errorMsg)) {
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

    private byte @Nullable [] buildClientDataJsonAndComputeHash(
            @ClientDataRequestType int clientDataRequestType,
            String callerOrigin,
            byte[] challenge,
            boolean isCrossOrigin,
            @Nullable PaymentOptions paymentOptions,
            String relyingPartyId,
            @Nullable Origin topOrigin) {
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

    private void startImmediateTimer() {
        mImmediateTimer.startTimer(
                DeviceFeatureList.sWebAuthnImmmediateTimeoutMs.getValue(),
                this::onImmediateTimeout);
    }

    private void stopImmediateTimer() {
        mImmediateTimer.cancelTimer();
    }

    private void onImmediateTimeout() {
        WebauthnRequestCallback requestCallback =
                mAuthenticationContextProvider.getRequestCallback();
        if (requestCallback == null) {
            return;
        }
        assert requestCallback.getCallbackType()
                == WebauthnRequestCallback.CallbackType.GET_CREDENTIAL;
        logError(TAG, "Timed out waiting for immediate request");
        mCredManHelper.cancelGetAssertion(AuthenticatorStatus.NOT_ALLOWED_ERROR);
        mBarrier.onFido2ApiCancelled(AuthenticatorStatus.NOT_ALLOWED_ERROR);
        mCancellableUiState = CancellableUiState.CANCEL_PENDING;
    }

    @Override
    public @Nullable WebauthnBrowserBridge getBridge() {
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

        byte @Nullable [] makeCredentialResponseFromJson(String json);

        String getOptionsToJson(ByteBuffer serializedOptions);

        byte @Nullable [] getCredentialResponseFromJson(String json);

        String reportOptionsToJson(ByteBuffer serializedOptions);
    }
}
