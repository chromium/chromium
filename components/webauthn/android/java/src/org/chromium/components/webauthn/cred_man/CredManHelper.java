// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

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

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.components.webauthn.AuthenticationContextProvider;
import org.chromium.components.webauthn.Barrier;
import org.chromium.components.webauthn.Fido2CredentialRequest.ConditionalUiState;
import org.chromium.components.webauthn.Fido2CredentialRequestJni;
import org.chromium.components.webauthn.GetAssertionResponseCallback;
import org.chromium.components.webauthn.MakeCredentialResponseCallback;
import org.chromium.components.webauthn.WebauthnBrowserBridge;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.components.webauthn.cred_man.CredManMetricsHelper.CredManCreateRequestEnum;
import org.chromium.components.webauthn.cred_man.CredManMetricsHelper.CredManGetRequestEnum;
import org.chromium.components.webauthn.cred_man.CredManMetricsHelper.CredManPrepareRequestEnum;
import org.chromium.content_public.browser.RenderFrameHost;

import java.nio.ByteBuffer;

public class CredManHelper {
    // These two values are formed differently because they come from the
    // Jetpack library, not the framework.
    @VisibleForTesting
    public static final String CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_INVALID_STATE_ERROR =
            "androidx.credentials.TYPE_CREATE_PUBLIC_KEY_CREDENTIAL_DOM_EXCEPTION/androidx.credentials.TYPE_INVALID_STATE_ERROR";

    protected static final String CRED_MAN_PREFIX = "androidx.credentials.";
    protected static final String TYPE_PASSKEY = CRED_MAN_PREFIX + "TYPE_PUBLIC_KEY_CREDENTIAL";

    private static final String TAG = "CredManHelper";

    private Callback<Integer> mErrorCallback;
    private Barrier mBarrier;
    private boolean mPlayServicesAvailable;
    private boolean mRequestPasswords;
    private final AuthenticationContextProvider mAuthenticationContextProvider;
    private final WebauthnBrowserBridge.Provider mBridgeProvider;
    private byte[] mClientDataJson;
    private ConditionalUiState mConditionalUiState = ConditionalUiState.NONE;
    private CredManRequestDecorator mCredManRequestDecorator;
    private CredManMetricsHelper mMetricsHelper;
    private Runnable mNoCredentialsFallback;

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
            @Nullable byte[] clientDataJson,
            @Nullable byte[] clientDataHash,
            MakeCredentialResponseCallback makeCallback,
            Callback<Integer> errorCallback) {
        mClientDataJson = clientDataJson;
        final String requestAsJson =
                Fido2CredentialRequestJni.get().createOptionsToJson(options.serialize());

        OutcomeReceiver<CreateCredentialResponse, CreateCredentialException> receiver =
                new OutcomeReceiver<>() {
                    @Override
                    public void onError(CreateCredentialException exception) {
                        String errorType = exception.getType();
                        Log.e(
                                TAG,
                                "CredMan CreateCredential call failed: %s",
                                errorType + " (" + exception.getMessage() + ")");
                        if (errorType.equals(CreateCredentialException.TYPE_USER_CANCELED)) {
                            errorCallback.onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                            mMetricsHelper.recordCredManCreateRequestHistogram(
                                    CredManCreateRequestEnum.CANCELLED);
                        } else if (errorType.equals(
                                CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_INVALID_STATE_ERROR)) {
                            errorCallback.onResult(AuthenticatorStatus.CREDENTIAL_EXCLUDED);
                            // This is successful from the point of view of the user.
                            mMetricsHelper.recordCredManCreateRequestHistogram(
                                    CredManCreateRequestEnum.SUCCESS);
                        } else {
                            // Includes:
                            //  * CreateCredentialException.TYPE_UNKNOWN
                            //  * CreateCredentialException.TYPE_NO_CREATE_OPTIONS
                            //  * CreateCredentialException.TYPE_INTERRUPTED
                            errorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                            mMetricsHelper.recordCredManCreateRequestHistogram(
                                    CredManCreateRequestEnum.FAILURE);
                        }
                    }

                    @Override
                    public void onResult(CreateCredentialResponse createCredentialResponse) {
                        Bundle data;
                        data = createCredentialResponse.getData();
                        String json =
                                data.getString(
                                        CRED_MAN_PREFIX + "BUNDLE_KEY_REGISTRATION_RESPONSE_JSON");
                        byte[] responseSerialized =
                                Fido2CredentialRequestJni.get()
                                        .makeCredentialResponseFromJson(json);
                        if (responseSerialized == null) {
                            Log.e(
                                    TAG,
                                    "Failed to convert response from CredMan to Mojo object: %s",
                                    json);
                            errorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                            mMetricsHelper.recordCredManCreateRequestHistogram(
                                    CredManCreateRequestEnum.FAILURE);
                            return;
                        }
                        MakeCredentialAuthenticatorResponse response;
                        try {
                            response =
                                    MakeCredentialAuthenticatorResponse.deserialize(
                                            ByteBuffer.wrap(responseSerialized));
                        } catch (org.chromium.mojo.bindings.DeserializationException e) {
                            logDeserializationException(e);
                            errorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                            mMetricsHelper.recordCredManCreateRequestHistogram(
                                    CredManCreateRequestEnum.FAILURE);
                            return;
                        }
                        if (mClientDataJson != null) {
                            response.info.clientDataJson = mClientDataJson;
                        }
                        response.echoCredProps = options.credProps;
                        makeCallback.onRegisterResponse(AuthenticatorStatus.SUCCESS, response);
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
        final CredentialManager manager =
                (CredentialManager) context.getSystemService(Context.CREDENTIAL_SERVICE);
        manager.createCredential(context, request, null, context.getMainExecutor(), receiver);
        mMetricsHelper.recordCredManCreateRequestHistogram(CredManCreateRequestEnum.SENT_REQUEST);
        return AuthenticatorStatus.SUCCESS;
    }

    /** Queries credential availability using the Android 14 CredMan API. */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void startPrefetchRequest(
            PublicKeyCredentialRequestOptions options,
            String originString,
            @Nullable byte[] clientDataJson,
            @Nullable byte[] clientDataHash,
            GetAssertionResponseCallback getCallback,
            Callback<Integer> errorCallback,
            Barrier barrier,
            boolean ignoreGpm) {
        long startTimeMs = SystemClock.elapsedRealtime();
        mErrorCallback = errorCallback;
        mBarrier = barrier;

        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();
        OutcomeReceiver<PrepareGetCredentialResponse, GetCredentialException> receiver =
                new OutcomeReceiver<>() {
                    @Override
                    public void onError(GetCredentialException e) {
                        assert mConditionalUiState != ConditionalUiState.WAITING_FOR_SELECTION;
                        // prepareGetCredential uses getCredentialException, but it cannot be user
                        // cancelled so all errors map to UNKNOWN_ERROR.
                        Log.e(
                                TAG,
                                "CredMan prepareGetCredential call failed: %s",
                                e.getType() + " (" + e.getMessage() + ")");
                        mConditionalUiState = ConditionalUiState.NONE;
                        mBarrier.onCredManFailed(AuthenticatorStatus.UNKNOWN_ERROR);
                        mMetricsHelper.recordCredmanPrepareRequestHistogram(
                                CredManPrepareRequestEnum.FAILURE);
                    }

                    @Override
                    public void onResult(
                            PrepareGetCredentialResponse prepareGetCredentialResponse) {
                        if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                            // The request was completed synchronously when the cancellation was
                            // received.
                            mConditionalUiState = ConditionalUiState.NONE;
                            mBridgeProvider.getBridge().cleanupCredManRequest(frameHost);
                            return;
                        }
                        if (mConditionalUiState != ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST) {
                            Log.e(
                                    TAG,
                                    "CredMan prepareGetCredential request received a response while"
                                            + " the state is "
                                            + mConditionalUiState
                                            + ". Ignoring the response.");
                            return;
                        }
                        boolean hasPublicKeyCredentials =
                                prepareGetCredentialResponse.hasCredentialResults(TYPE_PASSKEY);
                        boolean hasAuthenticationResults =
                                prepareGetCredentialResponse.hasAuthenticationResults();

                        mConditionalUiState = ConditionalUiState.WAITING_FOR_SELECTION;

                        mBarrier.onCredManSuccessful(
                                () -> {
                                    mBridgeProvider
                                            .getBridge()
                                            .onCredManConditionalRequestPending(
                                                    frameHost,
                                                    hasPublicKeyCredentials
                                                            || hasAuthenticationResults,
                                                    (requestPasswords) -> {
                                                        setRequestPasswords(requestPasswords);
                                                        startGetRequest(
                                                                options,
                                                                originString,
                                                                clientDataJson,
                                                                clientDataHash,
                                                                getCallback,
                                                                errorCallback,
                                                                ignoreGpm);
                                                    });
                                });
                        mMetricsHelper.recordCredmanPrepareRequestHistogram(
                                hasPublicKeyCredentials
                                        ? CredManPrepareRequestEnum.SUCCESS_HAS_RESULTS
                                        : CredManPrepareRequestEnum.SUCCESS_NO_RESULTS);
                        mMetricsHelper.recordCredmanPrepareRequestDuration(
                                SystemClock.elapsedRealtime() - startTimeMs,
                                hasPublicKeyCredentials);
                    }
                };

        mConditionalUiState = ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST;
        final GetCredentialRequest getCredentialRequest =
                buildGetCredentialRequest(
                        options,
                        originString,
                        clientDataHash,
                        /* requestPasswords= */ false,
                        /* preferImmediatelyAvailable= */ false,
                        /* ignoreGpm= */ ignoreGpm);
        if (getCredentialRequest == null) {
            mConditionalUiState = ConditionalUiState.NONE;
            mMetricsHelper.recordCredmanPrepareRequestHistogram(
                    CredManPrepareRequestEnum.COULD_NOT_SEND_REQUEST);
            mBarrier.onCredManFailed(AuthenticatorStatus.NOT_ALLOWED_ERROR);
            return;
        }

        Context context = mAuthenticationContextProvider.getContext();
        final CredentialManager manager =
                (CredentialManager) context.getSystemService(Context.CREDENTIAL_SERVICE);
        manager.prepareGetCredential(
                getCredentialRequest, null, context.getMainExecutor(), receiver);
        mMetricsHelper.recordCredmanPrepareRequestHistogram(CredManPrepareRequestEnum.SENT_REQUEST);
    }

    public void setNoCredentialsFallback(Runnable noCredentialsFallback) {
        mNoCredentialsFallback = noCredentialsFallback;
    }

    /** Gets the credential using the Android 14 CredMan API. */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public int startGetRequest(
            PublicKeyCredentialRequestOptions options,
            String originString,
            @Nullable byte[] clientDataJson,
            byte[] clientDataHash,
            GetAssertionResponseCallback getCallback,
            Callback<Integer> errorCallback,
            boolean ignoreGpm) {
        mErrorCallback = errorCallback;
        mClientDataJson = clientDataJson;
        RenderFrameHost frameHost = mAuthenticationContextProvider.getRenderFrameHost();

        // The Android 14 APIs have to be called via reflection until Chromium
        // builds with the Android 14 SDK by default.
        OutcomeReceiver<GetCredentialResponse, GetCredentialException> receiver =
                new OutcomeReceiver<>() {
                    @Override
                    public void onError(GetCredentialException getCredentialException) {
                        String errorType = getCredentialException.getType();
                        Log.e(
                                TAG,
                                "CredMan getCredential call failed: %s",
                                errorType + " (" + getCredentialException.getMessage() + ")");
                        notifyBrowserOnCredManClosed(false);
                        if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                            mConditionalUiState = ConditionalUiState.NONE;
                            mBridgeProvider.getBridge().cleanupCredManRequest(frameHost);
                            mBarrier.onCredManCancelled();
                            return;
                        }
                        if (errorType.equals(GetCredentialException.TYPE_USER_CANCELED)) {
                            if (mConditionalUiState == ConditionalUiState.NONE) {
                                mErrorCallback.onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                            }

                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.CANCELLED, mConditionalUiState);
                        } else if (errorType.equals(GetCredentialException.TYPE_NO_CREDENTIAL)) {
                            // This was a modal request and no credentials were found.
                            // The UI that CredMan would show in this case is unsuitable
                            // so the request is forwarded to Play Services instead. Play
                            // Services shouldn't find any credentials either, but it
                            // will show a bottomsheet to that effect.
                            assert mConditionalUiState == ConditionalUiState.NONE;
                            assert !options.isConditional;

                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.NO_CREDENTIAL_FOUND, mConditionalUiState);
                            if (mNoCredentialsFallback != null) {
                                mNoCredentialsFallback.run();
                            } else if (mConditionalUiState == ConditionalUiState.NONE) {
                                mErrorCallback.onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                            }
                        } else {
                            // Includes:
                            //  * GetCredentialException.TYPE_UNKNOWN
                            //  * GetCredentialException.TYPE_NO_CREATE_OPTIONS
                            //  * GetCredentialException.TYPE_INTERRUPTED
                            mErrorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.FAILURE, mConditionalUiState);
                        }
                        mConditionalUiState =
                                options.isConditional
                                        ? ConditionalUiState.WAITING_FOR_SELECTION
                                        : ConditionalUiState.NONE;
                    }

                    @Override
                    public void onResult(GetCredentialResponse getCredentialResponse) {
                        if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                            notifyBrowserOnCredManClosed(false);
                            mConditionalUiState = ConditionalUiState.NONE;
                            mBridgeProvider.getBridge().cleanupCredManRequest(frameHost);
                            mBarrier.onCredManCancelled();
                            return;
                        }
                        Bundle data = getCredentialResponse.getCredential().getData();
                        String type = getCredentialResponse.getCredential().getType();

                        if (!TYPE_PASSKEY.equals(type)) {
                            mBridgeProvider
                                    .getBridge()
                                    .onPasswordCredentialReceived(
                                            frameHost,
                                            data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_ID"),
                                            data.getString(
                                                    CRED_MAN_PREFIX + "BUNDLE_KEY_PASSWORD"));
                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.SUCCESS_PASSWORD, mConditionalUiState);
                            return;
                        }

                        String json =
                                data.getString(
                                        CRED_MAN_PREFIX
                                                + "BUNDLE_KEY_AUTHENTICATION_RESPONSE_JSON");
                        byte[] responseSerialized =
                                Fido2CredentialRequestJni.get().getCredentialResponseFromJson(json);
                        if (responseSerialized == null) {
                            Log.e(
                                    TAG,
                                    "Failed to convert response from CredMan to Mojo object: %s",
                                    json);
                            mMetricsHelper.reportGetCredentialMetrics(
                                    CredManGetRequestEnum.FAILURE, mConditionalUiState);
                            mConditionalUiState =
                                    options.isConditional
                                            ? ConditionalUiState.WAITING_FOR_SELECTION
                                            : ConditionalUiState.NONE;
                            notifyBrowserOnCredManClosed(false);
                            mErrorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
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
                                    CredManGetRequestEnum.FAILURE, mConditionalUiState);
                            mConditionalUiState =
                                    options.isConditional
                                            ? ConditionalUiState.WAITING_FOR_SELECTION
                                            : ConditionalUiState.NONE;
                            notifyBrowserOnCredManClosed(false);
                            mErrorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                            return;
                        }
                        if (mClientDataJson != null) {
                            response.info.clientDataJson = mClientDataJson;
                        }
                        response.extensions.echoAppidExtension = options.extensions.appid != null;
                        mConditionalUiState =
                                options.isConditional
                                        ? ConditionalUiState.WAITING_FOR_SELECTION
                                        : ConditionalUiState.NONE;
                        notifyBrowserOnCredManClosed(true);
                        mMetricsHelper.reportGetCredentialMetrics(
                                CredManGetRequestEnum.SUCCESS_PASSKEY, mConditionalUiState);
                        if (frameHost != null) {
                            frameHost.notifyWebAuthnAssertionRequestSucceeded();
                        }
                        getCallback.onSignResponse(AuthenticatorStatus.SUCCESS, response);
                    }
                };

        if (mConditionalUiState == ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST) {
            Log.e(TAG, "Received a second credential selection while the first still in progress.");
            mMetricsHelper.reportGetCredentialMetrics(
                    CredManGetRequestEnum.COULD_NOT_SEND_REQUEST, mConditionalUiState);
            return AuthenticatorStatus.NOT_ALLOWED_ERROR;
        }
        mConditionalUiState =
                options.isConditional
                        ? ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST
                        : ConditionalUiState.NONE;
        final GetCredentialRequest getCredentialRequest =
                buildGetCredentialRequest(
                        options,
                        originString,
                        clientDataHash,
                        mRequestPasswords,
                        shouldPreferImmediatelyAvailable(options),
                        ignoreGpm);
        if (getCredentialRequest == null) {
            mMetricsHelper.reportGetCredentialMetrics(
                    CredManGetRequestEnum.COULD_NOT_SEND_REQUEST, mConditionalUiState);
            mConditionalUiState =
                    options.isConditional
                            ? ConditionalUiState.WAITING_FOR_SELECTION
                            : ConditionalUiState.NONE;
            return AuthenticatorStatus.NOT_ALLOWED_ERROR;
        }
        Context context = mAuthenticationContextProvider.getContext();
        final CredentialManager manager =
                (CredentialManager) context.getSystemService(Context.CREDENTIAL_SERVICE);
        manager.getCredential(
                context, getCredentialRequest, null, context.getMainExecutor(), receiver);
        mMetricsHelper.reportGetCredentialMetrics(
                CredManGetRequestEnum.SENT_REQUEST, mConditionalUiState);
        return AuthenticatorStatus.SUCCESS;
    }

    public void cancelConditionalGetAssertion() {
        switch (mConditionalUiState) {
            case WAITING_FOR_CREDENTIAL_LIST:
                mConditionalUiState = ConditionalUiState.CANCEL_PENDING;
                mBarrier.onCredManCancelled();
                break;
            case WAITING_FOR_SELECTION:
                mBridgeProvider
                        .getBridge()
                        .cleanupCredManRequest(mAuthenticationContextProvider.getRenderFrameHost());
                mConditionalUiState = ConditionalUiState.NONE;
                mBarrier.onCredManCancelled();
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

    boolean shouldPreferImmediatelyAvailable(PublicKeyCredentialRequestOptions options) {
        // Chrome renders its own UI when there are no credentials when using CredMan. However, this
        // is not true for WebView or Chrome 3rd party PWM mode - there are no other UIs. Thus
        // they never ask CredMan to skip its UI.
        if (is(mAuthenticationContextProvider.getWebContents(), WebauthnMode.CHROME)) {
            return !options.isConditional;
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
            PublicKeyCredentialRequestOptions options,
            String originString,
            byte[] clientDataHash,
            boolean requestPasswords,
            boolean preferImmediatelyAvailable,
            boolean ignoreGpm) {
        final String requestAsJson =
                Fido2CredentialRequestJni.get().getOptionsToJson(options.serialize());

        boolean hasAllowCredentials =
                options.allowCredentials != null && options.allowCredentials.length != 0;
        CredManGetCredentialRequestHelper helper =
                new CredManGetCredentialRequestHelper.Builder(
                                requestAsJson,
                                clientDataHash,
                                preferImmediatelyAvailable,
                                hasAllowCredentials,
                                requestPasswords)
                        .setOrigin(originString)
                        .setPlayServicesAvailable(mPlayServicesAvailable)
                        .setIgnoreGpm(ignoreGpm)
                        .setRenderFrameHost(mAuthenticationContextProvider.getRenderFrameHost())
                        .build();
        return helper.getGetCredentialRequest(mCredManRequestDecorator);
    }

    private static void logDeserializationException(Throwable e) {
        Log.e(
                TAG,
                "Failed to parse Mojo object. If this is happening in a test, and"
                    + " authenticator.mojom was updated, then you'll need to update the fake Mojo"
                    + " structures in Fido2ApiTestHelper. Robolectric doesn't support JNI calls so"
                    + " the JNI calls to translate from JSON -> serialized Mojo are mocked out and"
                    + " the responses are hard-coded. If the Mojo structure is updated then the"
                    + " responses also need to be updated. Flip `kUpdateRobolectricTests` in"
                    + " `value_conversions_unittest.cc`, run `component_unittests"
                    + " --gtest_filter=\"WebAuthnentication*\"` and it'll print out updated Java"
                    + " literals for `Fido2ApiTestHelper.java`.",
                e);
    }
}
