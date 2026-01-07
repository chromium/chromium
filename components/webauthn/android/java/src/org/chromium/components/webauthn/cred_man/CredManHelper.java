// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.webauthn.WebauthnLogger.log;
import static org.chromium.components.webauthn.WebauthnLogger.logError;
import static org.chromium.components.webauthn.WebauthnModeProvider.is;

import android.content.Context;
import android.credentials.CreateCredentialException;
import android.credentials.CreateCredentialRequest;
import android.credentials.CreateCredentialResponse;
import android.credentials.CredentialManager;
import android.credentials.GetCredentialException;
import android.credentials.GetCredentialRequest;
import android.credentials.GetCredentialResponse;
import android.credentials.PrepareGetCredentialResponse;
import android.os.Build;
import android.os.Bundle;
import android.os.OutcomeReceiver;
import android.os.SystemClock;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.CredentialInfo;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.GetCredentialOptions;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.Mediation;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.password_manager.BrowserAssistedLoginType;
import org.chromium.components.webauthn.AuthenticationContextProvider;
import org.chromium.components.webauthn.Barrier;
import org.chromium.components.webauthn.CredManSupport;
import org.chromium.components.webauthn.CredentialRequestResult;
import org.chromium.components.webauthn.Fido2CredentialRequest.CancellableUiState;
import org.chromium.components.webauthn.Fido2CredentialRequestJni;
import org.chromium.components.webauthn.GetAssertionOutcome;
import org.chromium.components.webauthn.MakeCredentialOutcome;
import org.chromium.components.webauthn.RequestMetrics;
import org.chromium.components.webauthn.WebauthnBrowserBridge;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.components.webauthn.WebauthnRequestCallback;
import org.chromium.components.webauthn.WebauthnRequestResponse;
import org.chromium.components.webauthn.cred_man.CredManMetricsHelper.CredManCreateRequestEnum;
import org.chromium.components.webauthn.cred_man.CredManMetricsHelper.CredManGetRequestEnum;
import org.chromium.components.webauthn.cred_man.CredManMetricsHelper.CredManPrepareRequestEnum;
import org.chromium.content_public.browser.RenderFrameHost;

import java.nio.ByteBuffer;

@NullMarked
public class CredManHelper {
    private static final String TAG = "CredManHelper";

    // These two values are formed differently because they come from the
    // Jetpack library, not the framework.
    @VisibleForTesting
    public static final String CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_INVALID_STATE_ERROR =
            "androidx.credentials.TYPE_CREATE_PUBLIC_KEY_CREDENTIAL_DOM_EXCEPTION/androidx.credentials.TYPE_INVALID_STATE_ERROR";

    protected static final String CRED_MAN_PREFIX = "androidx.credentials.";
    protected static final String TYPE_PASSKEY = CRED_MAN_PREFIX + "TYPE_PUBLIC_KEY_CREDENTIAL";
    protected static final String BUNDLE_KEY_REGISTRATION_RESPONSE_JSON =
            CRED_MAN_PREFIX + "BUNDLE_KEY_REGISTRATION_RESPONSE_JSON";

    @VisibleForTesting
    static final String CREDENTIAL_SOURCE_KEY = "com.android.chrome.CREDENTIAL_SOURCE";

    @VisibleForTesting static final String GPM_SOURCE = "GPM";
    @VisibleForTesting static final String REMOTE_SOURCE = "REMOTE";

    private @Nullable Barrier mBarrier;
    private final boolean mPlayServicesAvailable;
    private boolean mRequestPasswords;
    private final AuthenticationContextProvider mAuthenticationContextProvider;
    private final WebauthnBrowserBridge.Provider mBridgeProvider;
    private byte @Nullable [] mClientDataJson;
    private @CancellableUiState int mCancellableUiState = CancellableUiState.NONE;
    private final @Nullable CredManRequestDecorator mCredManRequestDecorator;
    private CredManMetricsHelper mMetricsHelper;
    private @Nullable Runnable mNoCredentialsFallback;

    public CredManHelper(
            AuthenticationContextProvider authenticationContextProvider,
            WebauthnBrowserBridge.Provider bridgeProvider,
            boolean playServicesAvailable) {
        mMetricsHelper = new CredManMetricsHelper();
        mAuthenticationContextProvider = authenticationContextProvider;
        mBridgeProvider = bridgeProvider;
        mPlayServicesAvailable = playServicesAvailable;
        mCredManRequestDecorator =
                WebauthnModeProvider.getInstance()
                        .getCredManRequestDecorator(authenticationContextProvider.getWebContents());
    }

    /** Create a credential using the Android 14 CredMan API. */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public int startMakeRequest(
            PublicKeyCredentialCreationOptions options,
            String originString,
            byte @Nullable [] clientDataJson,
            byte @Nullable [] clientDataHash) {
        log(TAG, "startMakeRequest");
        mClientDataJson = clientDataJson;
        final String requestAsJson =
                Fido2CredentialRequestJni.get().createOptionsToJson(options.serialize());

        OutcomeReceiver<CreateCredentialResponse, CreateCredentialException> receiver =
                new OutcomeReceiver<>() {
                    @Override
                    public void onError(CreateCredentialException exception) {
                        String errorType = exception.getType();
                        logError(
                                TAG,
                                "CredMan CreateCredential call failed with "
                                        + errorType
                                        + " ("
                                        + exception.getMessage()
                                        + ")");
                        WebauthnRequestCallback callback =
                                mAuthenticationContextProvider.getRequestCallback();
                        if (callback == null) {
                            logError(TAG, "No request callback for makeCredential request.");
                            return;
                        }
                        if (errorType.equals(CreateCredentialException.TYPE_USER_CANCELED)) {
                            callback.onComplete(
                                    WebauthnRequestResponse.forFailedMakeCredential(
                                            AuthenticatorStatus.NOT_ALLOWED_ERROR,
                                            new RequestMetrics.Builder()
                                                    .setMakeCredentialOutcome(
                                                            MakeCredentialOutcome.USER_CANCELLATION)
                                                    .setMakeCredentialResult(
                                                            CredentialRequestResult.USER_CANCELLED)
                                                    .build()));
                            mMetricsHelper.recordCredManCreateRequestHistogram(
                                    CredManCreateRequestEnum.CANCELLED);
                        } else if (errorType.equals(
                                CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_INVALID_STATE_ERROR)) {
                            callback.onComplete(
                                    WebauthnRequestResponse.forFailedMakeCredential(
                                            AuthenticatorStatus.CREDENTIAL_EXCLUDED,
                                            new RequestMetrics.Builder()
                                                    .setMakeCredentialOutcome(
                                                            MakeCredentialOutcome
                                                                    .CREDENTIAL_EXCLUDED)
                                                    .setMakeCredentialResult(
                                                            CredentialRequestResult
                                                                    .ANDROID_CRED_MAN_ERROR)
                                                    .build()));
                            // This is successful from the point of view of the user.
                            mMetricsHelper.recordCredManCreateRequestHistogram(
                                    CredManCreateRequestEnum.SUCCESS);
                        } else {
                            // Includes:
                            //  * CreateCredentialException.TYPE_UNKNOWN
                            //  * CreateCredentialException.TYPE_NO_CREATE_OPTIONS
                            //  * CreateCredentialException.TYPE_INTERRUPTED
                            callback.onComplete(
                                    WebauthnRequestResponse.forFailedMakeCredential(
                                            AuthenticatorStatus.UNKNOWN_ERROR,
                                            new RequestMetrics.Builder()
                                                    .setMakeCredentialResult(
                                                            CredentialRequestResult
                                                                    .ANDROID_CRED_MAN_ERROR)
                                                    .build()));
                            mMetricsHelper.recordCredManCreateRequestHistogram(
                                    CredManCreateRequestEnum.FAILURE);
                        }
                    }

                    @Override
                    public void onResult(CreateCredentialResponse createCredentialResponse) {
                        log(TAG, "startMakeRequest.onResult");
                        Bundle data = createCredentialResponse.getData();
                        MakeCredentialAuthenticatorResponse response =
                                parseCreateCredentialResponseData(data);
                        WebauthnRequestCallback callback =
                                mAuthenticationContextProvider.getRequestCallback();
                        if (callback == null) {
                            logError(TAG, "No request callback for makeCredential request.");
                            return;
                        }
                        if (response == null) {
                            callback.onComplete(
                                    WebauthnRequestResponse.forFailedMakeCredential(
                                            AuthenticatorStatus.UNKNOWN_ERROR,
                                            new RequestMetrics.Builder()
                                                    .setMakeCredentialResult(
                                                            CredentialRequestResult
                                                                    .ANDROID_CRED_MAN_ERROR)
                                                    .build()));
                            mMetricsHelper.recordCredManCreateRequestHistogram(
                                    CredManCreateRequestEnum.FAILURE);
                            return;
                        }
                        if (mClientDataJson != null) {
                            response.info.clientDataJson = mClientDataJson;
                        }
                        response.echoCredProps = options.credProps;
                        callback.onComplete(
                                WebauthnRequestResponse.forSuccessfulMakeCredential(
                                        response,
                                        new RequestMetrics.Builder()
                                                .setMakeCredentialOutcome(
                                                        MakeCredentialOutcome.SUCCESS)
                                                .setMakeCredentialResult(
                                                        CredentialRequestResult
                                                                .ANDROID_CRED_MAN_SUCCESS)
                                                .build()));
                        mMetricsHelper.recordCredManCreateRequestHistogram(
                                CredManCreateRequestEnum.SUCCESS);
                    }
                };

        final CredManCreateCredentialRequestHelper requestHelper =
                new CredManCreateCredentialRequestHelper.Builder(requestAsJson, clientDataHash)
                        .setUserId(options.user.id)
                        .setOrigin(originString)
                        .build();
        final CreateCredentialRequest request =
                requestHelper.getCreateCredentialRequest(mCredManRequestDecorator);
        Context context = mAuthenticationContextProvider.getContext();
        assumeNonNull(context);
        final CredentialManager manager =
                (CredentialManager) context.getSystemService(Context.CREDENTIAL_SERVICE);
        manager.createCredential(context, request, null, context.getMainExecutor(), receiver);
        mMetricsHelper.recordCredManCreateRequestHistogram(CredManCreateRequestEnum.SENT_REQUEST);
        return AuthenticatorStatus.SUCCESS;
    }

    /** Queries credential availability using the Android 14 CredMan API. */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void startPrefetchRequest(
            GetCredentialOptions options,
            String originString,
            byte @Nullable [] clientDataJson,
            byte @Nullable [] clientDataHash,
            Barrier barrier,
            @Nullable Runnable stopImmediateTimer,
            boolean ignoreGpm) {
        log(TAG, "startPrefetchRequest");
        long startTimeMs = SystemClock.elapsedRealtime();
        mBarrier = barrier; // Store this for any cancellation requests.
        final Barrier localBarrier = barrier;
        final WebauthnBrowserBridge localBridge = assumeNonNull(mBridgeProvider.getBridge());

        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        OutcomeReceiver<PrepareGetCredentialResponse, GetCredentialException> receiver =
                new OutcomeReceiver<>() {
                    @Override
                    public void onError(GetCredentialException e) {
                        assert mCancellableUiState != CancellableUiState.WAITING_FOR_SELECTION;
                        // prepareGetCredential uses getCredentialException, but it cannot be user
                        // cancelled so all errors map to UNKNOWN_ERROR.
                        logError(
                                TAG,
                                "CredMan prepareGetCredential call failed with "
                                        + e.getType()
                                        + " ("
                                        + e.getMessage()
                                        + ")");
                        mCancellableUiState = CancellableUiState.NONE;
                        localBarrier.onCredManFailed(AuthenticatorStatus.UNKNOWN_ERROR);
                        mMetricsHelper.recordCredmanPrepareRequestHistogram(
                                CredManPrepareRequestEnum.FAILURE);
                    }

                    @Override
                    public void onResult(
                            PrepareGetCredentialResponse prepareGetCredentialResponse) {
                        log(TAG, "startPrefetchRequest.onResult");
                        if (mCancellableUiState == CancellableUiState.CANCEL_PENDING) {
                            // The request was completed synchronously when the cancellation was
                            // received.
                            mCancellableUiState = CancellableUiState.NONE;
                            return;
                        }
                        if (mCancellableUiState != CancellableUiState.WAITING_FOR_CREDENTIAL_LIST) {
                            logError(
                                    TAG,
                                    "prepareGetCredential request received a"
                                            + " response while the state is "
                                            + mCancellableUiState
                                            + ". Ignoring the response.");
                            return;
                        }
                        boolean hasPublicKeyCredentials =
                                prepareGetCredentialResponse.hasCredentialResults(TYPE_PASSKEY);
                        boolean hasAuthenticationResults =
                                prepareGetCredentialResponse.hasAuthenticationResults();
                        log(
                                TAG,
                                "startPrefetchRequest.onResult with"
                                        + " hasPublicKeyCredentials: "
                                        + hasPublicKeyCredentials
                                        + " and hasAuthenticationResults: "
                                        + hasAuthenticationResults);

                        mCancellableUiState = CancellableUiState.WAITING_FOR_SELECTION;

                        Runnable barrierCallback;
                        WebauthnRequestCallback callback =
                                mAuthenticationContextProvider.getRequestCallback();
                        if (callback == null) {
                            logError(TAG, "No request callback for getCredential request.");
                            return;
                        }
                        if (options.mediation == Mediation.IMMEDIATE
                                && CredManSupportProvider.getCredManSupport()
                                        == CredManSupport.FULL_UNLESS_INAPPLICABLE) {
                            // For an Immediate Mediation request that is being sent only to
                            // CredMan, the prefetch only happens because we need to cancel
                            // if the request is taking more than a certain amount of time to
                            // resolve. If credentials were found then it is followed by a call
                            // to `startGetRequest`.
                            assumeNonNull(stopImmediateTimer).run();

                            setRequestPasswords(options.password && hasAuthenticationResults);

                            if (!hasPublicKeyCredentials && !mRequestPasswords) {
                                // TODO(https://crbug.com/408002783): This should have a distinct
                                // GetAssertionOutcome for logging.
                                RequestMetrics metrics =
                                        new RequestMetrics.Builder()
                                                .setGetAssertionOutcome(
                                                        GetAssertionOutcome.OTHER_FAILURE)
                                                .setGetAssertionResult(
                                                        CredentialRequestResult
                                                                .ANDROID_CRED_MAN_ERROR)
                                                .build();
                                callback.onComplete(
                                        WebauthnRequestResponse.forFailedGetCredential(
                                                AuthenticatorStatus.NOT_ALLOWED_ERROR, metrics));
                                return;
                            }
                            // This fallback should not be used because the prefetch identified
                            // usable credentials, but there is raciness here if the credential is
                            // getting deleted by other means. Setting a fallback avoids UI being
                            // shown when it should not be.
                            setNoCredentialsFallback(
                                    () -> {
                                        RequestMetrics metrics =
                                                new RequestMetrics.Builder()
                                                        .setGetAssertionOutcome(
                                                                GetAssertionOutcome.OTHER_FAILURE)
                                                        .setGetAssertionResult(
                                                                CredentialRequestResult
                                                                        .ANDROID_CRED_MAN_ERROR)
                                                        .build();
                                        callback.onComplete(
                                                WebauthnRequestResponse.forFailedGetCredential(
                                                        AuthenticatorStatus.NOT_ALLOWED_ERROR,
                                                        metrics));
                                    });
                            barrierCallback =
                                    () ->
                                            startGetRequest(
                                                    options,
                                                    originString,
                                                    clientDataJson,
                                                    clientDataHash,
                                                    ignoreGpm);
                        } else {
                            barrierCallback =
                                    () -> {
                                        localBridge.onCredManConditionalRequestPending(
                                                frameHost,
                                                hasPublicKeyCredentials || hasAuthenticationResults,
                                                (requestPasswords) -> {
                                                    setRequestPasswords(requestPasswords);
                                                    startGetRequest(
                                                            options,
                                                            originString,
                                                            clientDataJson,
                                                            clientDataHash,
                                                            ignoreGpm);
                                                });
                                    };
                        }

                        localBarrier.onCredManSuccessful(barrierCallback);
                        mMetricsHelper.recordCredmanPrepareRequestHistogram(
                                hasPublicKeyCredentials
                                        ? CredManPrepareRequestEnum.SUCCESS_HAS_RESULTS
                                        : CredManPrepareRequestEnum.SUCCESS_NO_RESULTS);
                        mMetricsHelper.recordCredmanPrepareRequestDuration(
                                SystemClock.elapsedRealtime() - startTimeMs,
                                hasPublicKeyCredentials);
                    }
                };

        mCancellableUiState = CancellableUiState.WAITING_FOR_CREDENTIAL_LIST;
        final GetCredentialRequest getCredentialRequest =
                buildGetCredentialRequest(
                        options.publicKey,
                        originString,
                        clientDataHash,
                        /* requestPasswords= */ false,
                        /* preferImmediatelyAvailable= */ false,
                        /* ignoreGpm= */ ignoreGpm);
        if (getCredentialRequest == null) {
            mCancellableUiState = CancellableUiState.NONE;
            mMetricsHelper.recordCredmanPrepareRequestHistogram(
                    CredManPrepareRequestEnum.COULD_NOT_SEND_REQUEST);
            localBarrier.onCredManFailed(AuthenticatorStatus.NOT_ALLOWED_ERROR);
            return;
        }

        Context context = mAuthenticationContextProvider.getContext();
        assumeNonNull(context);
        final CredentialManager manager =
                (CredentialManager) context.getSystemService(Context.CREDENTIAL_SERVICE);
        manager.prepareGetCredential(
                getCredentialRequest, null, context.getMainExecutor(), receiver);
        mMetricsHelper.recordCredmanPrepareRequestHistogram(CredManPrepareRequestEnum.SENT_REQUEST);
    }

    public void setNoCredentialsFallback(@Nullable Runnable noCredentialsFallback) {
        mNoCredentialsFallback = noCredentialsFallback;
    }

    /** Gets the credential using the Android 14 CredMan API. */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public int startGetRequest(
            GetCredentialOptions options,
            String originString,
            byte @Nullable [] clientDataJson,
            byte @Nullable [] clientDataHash,
            boolean ignoreGpm) {
        log(TAG, "startGetRequest");
        mClientDataJson = clientDataJson;
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        final WebauthnBrowserBridge localBridge = assumeNonNull(mBridgeProvider.getBridge());

        // The Android 14 APIs have to be called via reflection until Chromium
        // builds with the Android 14 SDK by default.
        OutcomeReceiver<GetCredentialResponse, GetCredentialException> receiver =
                new OutcomeReceiver<>() {
                    @Override
                    public void onError(GetCredentialException getCredentialException) {
                        String errorType = getCredentialException.getType();
                        logError(
                                TAG,
                                "CredMan getCredential call failed with "
                                        + errorType
                                        + " ("
                                        + getCredentialException.getMessage()
                                        + ")");
                        notifyBrowserOnCredManClosed(false);
                        if (mCancellableUiState == CancellableUiState.CANCEL_PENDING) {
                            mCancellableUiState = CancellableUiState.NONE;
                            return;
                        }
                        WebauthnRequestCallback callback =
                                mAuthenticationContextProvider.getRequestCallback();
                        if (callback == null) {
                            logError(TAG, "No request callback for getCredential request");
                            return;
                        }
                        if (errorType.equals(GetCredentialException.TYPE_USER_CANCELED)) {
                            if (mCancellableUiState == CancellableUiState.NONE) {
                                RequestMetrics metrics =
                                        new RequestMetrics.Builder()
                                                .setGetAssertionOutcome(
                                                        GetAssertionOutcome.USER_CANCELLATION)
                                                .setGetAssertionResult(
                                                        CredentialRequestResult.USER_CANCELLED)
                                                .build();
                                callback.onComplete(
                                        WebauthnRequestResponse.forFailedGetCredential(
                                                AuthenticatorStatus.NOT_ALLOWED_ERROR, metrics));
                            }

                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.CANCELLED, mCancellableUiState);
                        } else if (errorType.equals(GetCredentialException.TYPE_NO_CREDENTIAL)) {
                            // This was a modal request and no credentials were found.
                            // The UI that CredMan would show in this case is unsuitable
                            // so the request is forwarded to Play Services instead. Play
                            // Services shouldn't find any credentials either, but it
                            // will show a bottomsheet to that effect.
                            assert mCancellableUiState == CancellableUiState.NONE;
                            assert options.mediation != Mediation.CONDITIONAL;

                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.NO_CREDENTIAL_FOUND, mCancellableUiState);
                            if (mNoCredentialsFallback != null) {
                                mNoCredentialsFallback.run();
                            } else if (mCancellableUiState == CancellableUiState.NONE) {
                                RequestMetrics metrics =
                                        new RequestMetrics.Builder()
                                                .setGetAssertionOutcome(
                                                        GetAssertionOutcome
                                                                .CREDENTIAL_NOT_RECOGNIZED)
                                                .setGetAssertionResult(
                                                        CredentialRequestResult
                                                                .ANDROID_CRED_MAN_ERROR)
                                                .build();
                                callback.onComplete(
                                        WebauthnRequestResponse.forFailedGetCredential(
                                                AuthenticatorStatus.NOT_ALLOWED_ERROR, metrics));
                            }
                        } else {
                            // Includes:
                            //  * GetCredentialException.TYPE_UNKNOWN
                            //  * GetCredentialException.TYPE_NO_CREATE_OPTIONS
                            //  * GetCredentialException.TYPE_INTERRUPTED
                            RequestMetrics metrics =
                                    new RequestMetrics.Builder()
                                            .setGetAssertionResult(
                                                    CredentialRequestResult.ANDROID_CRED_MAN_ERROR)
                                            .build();
                            callback.onComplete(
                                    WebauthnRequestResponse.forFailedGetCredential(
                                            AuthenticatorStatus.UNKNOWN_ERROR, metrics));
                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.FAILURE, mCancellableUiState);
                        }
                        mCancellableUiState =
                                options.mediation == Mediation.CONDITIONAL
                                        ? CancellableUiState.WAITING_FOR_SELECTION
                                        : CancellableUiState.NONE;
                    }

                    @Override
                    public void onResult(GetCredentialResponse getCredentialResponse) {
                        log(TAG, "startGetRequest.onResult");
                        if (mCancellableUiState == CancellableUiState.CANCEL_PENDING) {
                            notifyBrowserOnCredManClosed(false);
                            mCancellableUiState = CancellableUiState.NONE;
                            localBridge.cleanupCredManRequest(frameHost);
                            return;
                        }
                        Bundle data = getCredentialResponse.getCredential().getData();
                        String type = getCredentialResponse.getCredential().getType();

                        WebauthnRequestCallback callback =
                                mAuthenticationContextProvider.getRequestCallback();
                        if (callback == null) {
                            logError(TAG, "No request callback for getCredential request");
                            return;
                        }
                        if (!TYPE_PASSKEY.equals(type)) {
                            if (options.mediation == Mediation.IMMEDIATE) {
                                CredentialInfo passwordCredential =
                                        WebauthnBrowserBridge.buildPasswordCredentialInfo(
                                                WebauthnBrowserBridge.stringToMojoString16(
                                                        data.getString(
                                                                CRED_MAN_PREFIX + "BUNDLE_KEY_ID")),
                                                WebauthnBrowserBridge.stringToMojoString16(
                                                        data.getString(
                                                                CRED_MAN_PREFIX
                                                                        + "BUNDLE_KEY_PASSWORD")));
                                callback.onComplete(
                                        WebauthnRequestResponse.forSuccessfulPassword(
                                                passwordCredential));
                                return;
                            }

                            localBridge.onPasswordCredentialReceived(
                                    frameHost,
                                    data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_ID"),
                                    data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_PASSWORD"));
                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.SUCCESS_PASSWORD, mCancellableUiState);
                            return;
                        }

                        String json =
                                data.getString(
                                        CRED_MAN_PREFIX
                                                + "BUNDLE_KEY_AUTHENTICATION_RESPONSE_JSON");
                        assertNonNull(json);
                        byte[] responseSerialized =
                                Fido2CredentialRequestJni.get().getCredentialResponseFromJson(json);
                        if (responseSerialized == null) {
                            logError(
                                    TAG,
                                    "Failed to convert response from CredMan to Mojo"
                                            + " object: %s",
                                    json);
                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.FAILURE, mCancellableUiState);
                            mCancellableUiState =
                                    options.mediation == Mediation.CONDITIONAL
                                            ? CancellableUiState.WAITING_FOR_SELECTION
                                            : CancellableUiState.NONE;
                            notifyBrowserOnCredManClosed(false);
                            RequestMetrics metrics =
                                    new RequestMetrics.Builder()
                                            .setGetAssertionResult(
                                                    CredentialRequestResult.ANDROID_CRED_MAN_ERROR)
                                            .build();
                            callback.onComplete(
                                    WebauthnRequestResponse.forFailedGetCredential(
                                            AuthenticatorStatus.UNKNOWN_ERROR, metrics));
                            return;
                        }

                        GetAssertionAuthenticatorResponse response;
                        try {
                            response =
                                    GetAssertionAuthenticatorResponse.deserialize(
                                            ByteBuffer.wrap(responseSerialized));
                        } catch (org.chromium.mojo.bindings.DeserializationException e) {
                            logDeserializationException(e);
                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.FAILURE, mCancellableUiState);
                            mCancellableUiState =
                                    options.mediation == Mediation.CONDITIONAL
                                            ? CancellableUiState.WAITING_FOR_SELECTION
                                            : CancellableUiState.NONE;
                            notifyBrowserOnCredManClosed(false);
                            RequestMetrics metrics =
                                    new RequestMetrics.Builder()
                                            .setGetAssertionResult(
                                                    CredentialRequestResult.ANDROID_CRED_MAN_ERROR)
                                            .build();
                            callback.onComplete(
                                    WebauthnRequestResponse.forFailedGetCredential(
                                            AuthenticatorStatus.UNKNOWN_ERROR, metrics));
                            return;
                        }
                        if (mClientDataJson != null) {
                            response.info.clientDataJson = mClientDataJson;
                        }
                        response.extensions.echoAppidExtension =
                                options.publicKey != null
                                        && assumeNonNull(options.publicKey).extensions.appid
                                                != null;
                        mCancellableUiState =
                                options.mediation == Mediation.CONDITIONAL
                                        ? CancellableUiState.WAITING_FOR_SELECTION
                                        : CancellableUiState.NONE;
                        notifyBrowserOnCredManClosed(true);
                        mMetricsHelper.reportGetCredentialMetrics(
                                CredManGetRequestEnum.SUCCESS_PASSKEY, mCancellableUiState);
                        if (frameHost != null) {
                            frameHost.notifyWebAuthnAssertionRequestSucceeded();
                        }
                        @BrowserAssistedLoginType
                        int browserAssistedLoginType = getBrowserAssistedLoginType(data);
                        RequestMetrics metrics =
                                new RequestMetrics.Builder()
                                        .setGetAssertionOutcome(GetAssertionOutcome.SUCCESS)
                                        .setGetAssertionResult(
                                                CredentialRequestResult.ANDROID_CRED_MAN_SUCCESS)
                                        .setBrowserAssistedLoginType(browserAssistedLoginType)
                                        .build();
                        assumeNonNull(callback);
                        callback.onComplete(
                                WebauthnRequestResponse.forSuccessfulGetAssertion(
                                        response, metrics));
                    }
                };

        if (mCancellableUiState == CancellableUiState.WAITING_FOR_CREDENTIAL_LIST) {
            logError(
                    TAG,
                    "Received a second credential selection while the first still in progress.");
            mMetricsHelper.reportGetCredentialMetrics(
                    CredManGetRequestEnum.COULD_NOT_SEND_REQUEST, mCancellableUiState);
            return AuthenticatorStatus.NOT_ALLOWED_ERROR;
        }

        if (options.mediation == Mediation.IMMEDIATE) {
            mRequestPasswords = options.password;
        }
        mCancellableUiState =
                options.mediation == Mediation.CONDITIONAL
                        ? CancellableUiState.WAITING_FOR_CREDENTIAL_LIST
                        : CancellableUiState.NONE;
        final GetCredentialRequest getCredentialRequest =
                buildGetCredentialRequest(
                        options.publicKey,
                        originString,
                        clientDataHash,
                        mRequestPasswords,
                        shouldPreferImmediatelyAvailable(options.mediation),
                        ignoreGpm);
        if (getCredentialRequest == null) {
            mMetricsHelper.reportGetCredentialMetrics(
                    CredManGetRequestEnum.COULD_NOT_SEND_REQUEST, mCancellableUiState);
            mCancellableUiState =
                    options.mediation == Mediation.CONDITIONAL
                            ? CancellableUiState.WAITING_FOR_SELECTION
                            : CancellableUiState.NONE;
            return AuthenticatorStatus.NOT_ALLOWED_ERROR;
        }
        Context context = mAuthenticationContextProvider.getContext();
        assumeNonNull(context);
        final CredentialManager manager =
                (CredentialManager) context.getSystemService(Context.CREDENTIAL_SERVICE);
        manager.getCredential(
                context, getCredentialRequest, null, context.getMainExecutor(), receiver);
        mMetricsHelper.reportGetCredentialMetrics(
                CredManGetRequestEnum.SENT_REQUEST, mCancellableUiState);
        return AuthenticatorStatus.SUCCESS;
    }

    public void cancelGetAssertion(int error) {
        log(TAG, "cancelGetAssertion");
        switch (mCancellableUiState) {
            case CancellableUiState.WAITING_FOR_CREDENTIAL_LIST:
                mCancellableUiState = CancellableUiState.CANCEL_PENDING;
                assumeNonNull(mBarrier);
                mBarrier.onCredManCancelled(error);
                break;
            case CancellableUiState.WAITING_FOR_SELECTION:
                assumeNonNull(mBridgeProvider.getBridge());
                mBridgeProvider
                        .getBridge()
                        .cleanupCredManRequest(mAuthenticationContextProvider.getRenderFrameHost());
                mCancellableUiState = CancellableUiState.NONE;
                assumeNonNull(mBarrier);
                mBarrier.onCredManCancelled(error);
                break;
            default:
                // No action
        }
    }

    public void setMetricsHelperForTesting(CredManMetricsHelper metricsHelper) {
        mMetricsHelper = metricsHelper;
    }

    void setRequestPasswords(boolean requestPasswords) {
        mRequestPasswords = requestPasswords;
    }

    boolean shouldPreferImmediatelyAvailable(@Mediation.EnumType int mediation) {
        if (mediation == Mediation.IMMEDIATE) {
            return true;
        }

        // Chrome renders its own UI when there are no credentials when using CredMan. However, this
        // is not true for WebView or Chrome 3rd party PWM mode - there are no other UIs. Thus
        // they never ask CredMan to skip its UI.
        if (is(mAuthenticationContextProvider.getWebContents(), WebauthnMode.CHROME)
                && mPlayServicesAvailable) {
            return mediation != Mediation.CONDITIONAL;
        }
        return false;
    }

    private void notifyBrowserOnCredManClosed(boolean success) {
        if (mBridgeProvider.getBridge() == null) return;
        mBridgeProvider
                .getBridge()
                .onCredManUiClosed(mAuthenticationContextProvider.getRenderFrameHost(), success);
    }

    /**
     * Construct a CredMan request for credentials.
     *
     * @param options The WebAuthn get() call.
     * @param originString The origin that made the WebAuthn request.
     * @param clientDataHash The hash of the ClientDataJSON to be passed to the CredMan API.
     * @param requestPasswords True if password credentials should also be requested.
     * @param preferImmediatelyAvailable True to make the eventual request fail with a
     *     `NO_CREDENTIAL` error if there are no credentials found.
     * @param ignoreGpm True if Google Password Manager should ignore CredMan requests.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private GetCredentialRequest buildGetCredentialRequest(
            @Nullable PublicKeyCredentialRequestOptions options,
            String originString,
            byte @Nullable [] clientDataHash,
            boolean requestPasswords,
            boolean preferImmediatelyAvailable,
            boolean ignoreGpm) {
        final @Nullable String requestAsJson =
                options == null
                        ? null
                        : Fido2CredentialRequestJni.get().getOptionsToJson(options.serialize());

        boolean hasAllowCredentials =
                options != null
                        && options.allowCredentials != null
                        && options.allowCredentials.length != 0;
        CredManGetCredentialRequestHelper helper =
                new CredManGetCredentialRequestHelper.Builder(
                                requestAsJson,
                                clientDataHash,
                                preferImmediatelyAvailable,
                                hasAllowCredentials,
                                requestPasswords)
                        .setOrigin(originString)
                        .setIgnoreGpm(ignoreGpm)
                        .setRenderFrameHost(mAuthenticationContextProvider.getRenderFrameHost())
                        .build();
        return helper.getGetCredentialRequest(mCredManRequestDecorator);
    }

    private static void logDeserializationException(Throwable e) {
        logError(
                TAG,
                "Failed to parse Mojo object. If this is happening in a test, and"
                    + " authenticator.mojom was updated, then you'll need to update the fake Mojo"
                    + " structures in Fido2ApiTestHelper. Robolectric doesn't support JNI calls so"
                    + " the JNI calls to translate from JSON -> serialized Mojo are mocked out and"
                    + " the responses are hard-coded. If the Mojo structure is updated then the"
                    + " responses also need to be updated. Flip `kUpdateRobolectricTests` in"
                    + " `value_conversions_unittest.cc`, run `component_unittests"
                    + " --gtest_filter=\"WebAuthnentication*\"` and it'll print out updated Java"
                    + " literals for `Fido2ApiTestHelper.java`. Run against an Android target"
                    + " otherwise decoding may still fail in tests.",
                e);
    }

    public static @Nullable MakeCredentialAuthenticatorResponse parseCreateCredentialResponseData(
            Bundle data) {
        String json = data.getString(BUNDLE_KEY_REGISTRATION_RESPONSE_JSON);
        assertNonNull(json);
        byte[] responseSerialized =
                Fido2CredentialRequestJni.get().makeCredentialResponseFromJson(json);
        if (responseSerialized == null) {
            logError(TAG, "Failed to convert response from CredMan to Mojo object: %s", json);
            return null;
        }
        try {
            return MakeCredentialAuthenticatorResponse.deserialize(
                    ByteBuffer.wrap(responseSerialized));
        } catch (org.chromium.mojo.bindings.DeserializationException e) {
            logDeserializationException(e);
            return null;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static @BrowserAssistedLoginType int getBrowserAssistedLoginType(Bundle data) {
        String credentialSource = data.getString(CREDENTIAL_SOURCE_KEY);
        int loginType = BrowserAssistedLoginType.PASSKEY_UNKNOWN;
        if (GPM_SOURCE.equals(credentialSource)) {
            loginType = BrowserAssistedLoginType.PASSKEY_STORED_IN_GPM;
        } else if (REMOTE_SOURCE.equals(credentialSource)) {
            loginType = BrowserAssistedLoginType.PASSKEY_HYBRID_OR_SECURITY_KEY;
        }

        return loginType;
    }
}
