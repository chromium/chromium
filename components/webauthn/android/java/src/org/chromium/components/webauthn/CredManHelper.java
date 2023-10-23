// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.content.ComponentName;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.OutcomeReceiver;
import android.os.SystemClock;
import android.util.Base64;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManCreateRequestEnum;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManGetRequestEnum;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManPrepareRequestEnum;
import org.chromium.components.webauthn.Fido2CredentialRequest.ConditionalUiState;
import org.chromium.content_public.browser.ClientDataJson;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.url.Origin;

import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Set;

public class CredManHelper {
    // These two values are formed differently because they come from the
    // Jetpack library, not the framework.

    @VisibleForTesting
    public static final String CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_INVALID_STATE_ERROR =
            "androidx.credentials.TYPE_CREATE_PUBLIC_KEY_CREDENTIAL_DOM_EXCEPTION/androidx.credentials.TYPE_INVALID_STATE_ERROR";

    public static final String CRED_MAN_IS_AUTO_SELECT_ALLOWED =
            "androidx.credentials.BUNDLE_KEY_IS_AUTO_SELECT_ALLOWED";

    private static final String CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_USER_CANCEL =
            "android.credentials.CreateCredentialException.TYPE_USER_CANCELED";
    private static final String CRED_MAN_EXCEPTION_GET_CREDENTIAL_TYPE_USER_CANCEL =
            "android.credentials.GetCredentialException.TYPE_USER_CANCELED";
    private static final String CRED_MAN_EXCEPTION_GET_CREDENTIAL_TYPE_NO_CREDENTIAL =
            "android.credentials.GetCredentialException.TYPE_NO_CREDENTIAL";
    private static final String CHANNEL_KEY = "com.android.chrome.CHANNEL";
    private static final String INCOGNITO_KEY = "com.android.chrome.INCOGNITO";
    private static final String CRED_MAN_PREFIX = "androidx.credentials.";
    private static final ComponentName GPM_COMPONENT_NAME =
            ComponentName.createRelative("com.google.android.gms",
                    ".auth.api.credentials.credman.service.PasswordAndPasskeyService");
    private static final String PASSWORDS_ONLY_FOR_THE_CHANNEL =
            "com.android.chrome.PASSWORDS_ONLY_FOR_THE_CHANNEL";
    private static final String PASSWORDS_WITH_NO_USERNAME_INCLUDED =
            "com.android.chrome.PASSWORDS_WITH_NO_USERNAME_INCLUDED";
    private static final String IGNORE_GPM = "com.android.chrome.GPM_IGNORE";
    private static final String TAG = "CredManHelper";
    private static final String TYPE_PASSKEY = CRED_MAN_PREFIX + "TYPE_PUBLIC_KEY_CREDENTIAL";

    private Callback<Integer> mErrorCallback;
    private Barrier mBarrier;
    private boolean mIsCrossOrigin;
    private boolean mPlayServicesAvailable;
    private boolean mRequestPasswords;
    private BridgeProvider mBridgeProvider;
    private byte[] mClientDataJson;
    private Class mCredManCreateRequestBuilderClassForTesting;
    private Class mCredManGetRequestBuilderClassForTesting;
    private Class mCredManCredentialOptionBuilderClassForTesting;
    private ConditionalUiState mConditionalUiState = ConditionalUiState.NONE;
    private Context mContext;
    private CredManMetricsHelper mMetricsHelper;
    private Object mCredentialManagerServiceForTesting;
    private RenderFrameHost mFrameHost;
    private Runnable mNoCredentialsFallback;

    public interface BridgeProvider {
        WebAuthnBrowserBridge getBridge();
    }

    public CredManHelper(BridgeProvider bridgeProvider, boolean playServicesAvailable) {
        mMetricsHelper = new CredManMetricsHelper();
        mBridgeProvider = bridgeProvider;
        mPlayServicesAvailable = playServicesAvailable;
    }

    /**
     * Create a credential using the Android 14 CredMan API.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public int startMakeRequest(Context context, RenderFrameHost frameHost,
            PublicKeyCredentialCreationOptions options, String originString,
            byte[] maybeClientDataHash, MakeCredentialResponseCallback makeCallback,
            Callback<Integer> errorCallback) {
        mContext = context;
        mFrameHost = frameHost;
        final String requestAsJson =
                Fido2CredentialRequestJni.get().createOptionsToJson(options.serialize());
        final byte[] clientDataHash = maybeClientDataHash != null
                ? maybeClientDataHash
                : buildClientDataJsonAndComputeHash(ClientDataRequestType.WEB_AUTHN_CREATE,
                        originString, options.challenge,
                        /*isCrossOrigin=*/false, /*paymentOptions=*/null, options.relyingParty.id,
                        /*topOrigin=*/null);
        if (clientDataHash == null) {
            mMetricsHelper.recordCredManCreateRequestHistogram(
                    CredManCreateRequestEnum.COULD_NOT_SEND_REQUEST);
            return AuthenticatorStatus.NOT_ALLOWED_ERROR;
        }

        final Bundle requestBundle = new Bundle();
        requestBundle.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_SUBTYPE",
                CRED_MAN_PREFIX + "BUNDLE_VALUE_SUBTYPE_CREATE_PUBLIC_KEY_CREDENTIAL_REQUEST");
        requestBundle.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_JSON", requestAsJson);
        requestBundle.putByteArray(CRED_MAN_PREFIX + "BUNDLE_KEY_CLIENT_DATA_HASH", clientDataHash);

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
            public void onResult(Object createCredentialResponse) {
                Bundle data;
                try {
                    data = (Bundle) createCredentialResponse.getClass().getMethod("getData").invoke(
                            createCredentialResponse);
                } catch (ReflectiveOperationException e) {
                    Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
                    errorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                    mMetricsHelper.recordCredManCreateRequestHistogram(
                            CredManCreateRequestEnum.FAILURE);
                    return;
                }

                String json =
                        data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_REGISTRATION_RESPONSE_JSON");
                byte[] responseSerialized =
                        Fido2CredentialRequestJni.get().makeCredentialResponseFromJson(json);
                if (responseSerialized == null) {
                    Log.e(TAG, "Failed to convert response from CredMan to Mojo object: %s", json);
                    errorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                    mMetricsHelper.recordCredManCreateRequestHistogram(
                            CredManCreateRequestEnum.FAILURE);
                    return;
                }
                MakeCredentialAuthenticatorResponse response;
                try {
                    response = MakeCredentialAuthenticatorResponse.deserialize(
                            ByteBuffer.wrap(responseSerialized));
                } catch (org.chromium.mojo.bindings.DeserializationException e) {
                    logDeserializationException(e);
                    errorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                    mMetricsHelper.recordCredManCreateRequestHistogram(
                            CredManCreateRequestEnum.FAILURE);
                    return;
                }
                response.info.clientDataJson = mClientDataJson;
                response.echoCredProps = options.credProps;
                makeCallback.onRegisterResponse(AuthenticatorStatus.SUCCESS, response);
                mMetricsHelper.recordCredManCreateRequestHistogram(
                        CredManCreateRequestEnum.SUCCESS);
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
            builderClass.getMethod("setOrigin", String.class).invoke(builder, originString);
            final Object request = builderClass.getMethod("build").invoke(builder);
            final Object manager = credentialManagerService(mContext);
            manager.getClass()
                    .getMethod("createCredential", Context.class, request.getClass(),
                            android.os.CancellationSignal.class,
                            java.util.concurrent.Executor.class, OutcomeReceiver.class)
                    .invoke(manager, mContext, request, null, mContext.getMainExecutor(), receiver);
            mMetricsHelper.recordCredManCreateRequestHistogram(
                    CredManCreateRequestEnum.SENT_REQUEST);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
            mMetricsHelper.recordCredManCreateRequestHistogram(
                    CredManCreateRequestEnum.COULD_NOT_SEND_REQUEST);
            return AuthenticatorStatus.UNKNOWN_ERROR;
        }
        return AuthenticatorStatus.SUCCESS;
    }

    /**
     * Queries credential availability using the Android 14 CredMan API.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void startPrefetchRequest(Context context, RenderFrameHost frameHost,
            PublicKeyCredentialRequestOptions options, String originString, boolean isCrossOrigin,
            byte[] maybeClientDataHash, GetAssertionResponseCallback getCallback,
            Callback<Integer> errorCallback, Barrier barrier, boolean ignoreGpm) {
        long startTimeMs = SystemClock.elapsedRealtime();
        mContext = context;
        mFrameHost = frameHost;
        mErrorCallback = errorCallback;
        mIsCrossOrigin = isCrossOrigin;
        mBarrier = barrier;

        // The Android 14 APIs have to be called via reflection until Chromium
        // builds with the Android 14 SDK by default.
        OutcomeReceiver<Object, Throwable> receiver =
                new OutcomeReceiver<>() {
                    @Override
                    public void onError(Throwable e) {
                        assert mConditionalUiState != ConditionalUiState.WAITING_FOR_SELECTION;
                        // prepareGetCredential uses getCredentialException, but it cannot be user
                        // cancelled so all errors map to UNKNOWN_ERROR.
                        Log.e(
                                TAG,
                                "CredMan prepareGetCredential call failed: %s",
                                getCredManExceptionType(e) + " (" + e.getMessage() + ")");
                        mConditionalUiState = ConditionalUiState.NONE;
                        mBarrier.onCredManFailed(AuthenticatorStatus.UNKNOWN_ERROR);
                        mMetricsHelper.recordCredmanPrepareRequestHistogram(
                                CredManPrepareRequestEnum.FAILURE);
                    }

                    @Override
                    public void onResult(Object prepareGetCredentialResponse) {
                        if (mConditionalUiState == ConditionalUiState.CANCEL_PENDING) {
                            // The request was completed synchronously when the cancellation was
                            // received.
                            mConditionalUiState = ConditionalUiState.NONE;
                            mBridgeProvider.getBridge().cleanupCredManRequest(mFrameHost);
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
                        boolean hasPublicKeyCredentials;
                        boolean hasAuthenticationResults;
                        try {
                            Method hasCredentialResultsMethod =
                                    prepareGetCredentialResponse
                                            .getClass()
                                            .getMethod("hasCredentialResults", String.class);
                            hasPublicKeyCredentials =
                                    (Boolean)
                                            hasCredentialResultsMethod.invoke(
                                                    prepareGetCredentialResponse, TYPE_PASSKEY);
                            Method hasAuthenticationResultsMethod =
                                    prepareGetCredentialResponse
                                            .getClass()
                                            .getMethod("hasAuthenticationResults");
                            hasAuthenticationResults =
                                    (Boolean)
                                            hasAuthenticationResultsMethod.invoke(
                                                    prepareGetCredentialResponse);
                        } catch (ReflectiveOperationException e) {
                            Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
                            mConditionalUiState = ConditionalUiState.NONE;
                            mBarrier.onCredManFailed(AuthenticatorStatus.UNKNOWN_ERROR);
                            mMetricsHelper.recordCredmanPrepareRequestHistogram(
                                    CredManPrepareRequestEnum.FAILURE);
                            return;
                        }

                        mConditionalUiState = ConditionalUiState.WAITING_FOR_SELECTION;
                        mBarrier.onCredManSuccessful(
                                () -> {
                                    mBridgeProvider
                                            .getBridge()
                                            .onCredManConditionalRequestPending(
                                                    mFrameHost,
                                                    hasPublicKeyCredentials
                                                            || hasAuthenticationResults,
                                                    (requestPasswords) -> {
                                                        setRequestPasswords(requestPasswords);
                                                        startGetRequest(
                                                                mContext,
                                                                mFrameHost,
                                                                options,
                                                                originString,
                                                                isCrossOrigin,
                                                                maybeClientDataHash,
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
                                SystemClock.elapsedRealtime() - startTimeMs);
                    }
                };

        try {
            mConditionalUiState = ConditionalUiState.WAITING_FOR_CREDENTIAL_LIST;
            final Object getCredentialRequest =
                    buildGetCredentialRequest(options, originString, maybeClientDataHash,
                            /*requestPasswords=*/false, /*preferImmediatelyAvailable=*/false,
                            /*ignoreGpm=*/ignoreGpm);
            if (getCredentialRequest == null) {
                mConditionalUiState = ConditionalUiState.NONE;
                mMetricsHelper.recordCredmanPrepareRequestHistogram(
                        CredManPrepareRequestEnum.COULD_NOT_SEND_REQUEST);
                mBarrier.onCredManFailed(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                return;
            }

            final Object manager = credentialManagerService(mContext);
            manager.getClass()
                    .getMethod("prepareGetCredential", getCredentialRequest.getClass(),
                            android.os.CancellationSignal.class,
                            java.util.concurrent.Executor.class, OutcomeReceiver.class)
                    .invoke(manager, getCredentialRequest, null, mContext.getMainExecutor(),
                            receiver);
            mMetricsHelper.recordCredmanPrepareRequestHistogram(
                    CredManPrepareRequestEnum.SENT_REQUEST);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
            mConditionalUiState = ConditionalUiState.NONE;
            mMetricsHelper.recordCredmanPrepareRequestHistogram(
                    CredManPrepareRequestEnum.COULD_NOT_SEND_REQUEST);
            mBarrier.onCredManFailed(AuthenticatorStatus.UNKNOWN_ERROR);
        }
    }

    public void setNoCredentialsFallback(Runnable noCredentialsFallback) {
        mNoCredentialsFallback = noCredentialsFallback;
    }

    /**
     * Gets the credential using the Android 14 CredMan API.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public int startGetRequest(Context context, RenderFrameHost frameHost,
            PublicKeyCredentialRequestOptions options, String originString, boolean isCrossOrigin,
            byte[] maybeClientDataHash, GetAssertionResponseCallback getCallback,
            Callback<Integer> errorCallback, boolean ignoreGpm) {
        mContext = context;
        mFrameHost = frameHost;
        mErrorCallback = errorCallback;
        mIsCrossOrigin = isCrossOrigin;

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
                    mBridgeProvider.getBridge().cleanupCredManRequest(mFrameHost);
                    mBarrier.onCredManCancelled();
                    return;
                }
                if (errorType.equals(CRED_MAN_EXCEPTION_GET_CREDENTIAL_TYPE_USER_CANCEL)) {
                    if (mConditionalUiState == ConditionalUiState.NONE) {
                        mErrorCallback.onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                    }

                    mMetricsHelper.reportGetCredentialMetrics(
                            CredManGetRequestEnum.CANCELLED, mConditionalUiState);
                } else if (errorType.equals(CRED_MAN_EXCEPTION_GET_CREDENTIAL_TYPE_NO_CREDENTIAL)) {
                    // This was a modal request and no credentials were found.
                    // The UI that CredMan would show in this case is unsuitable
                    // so the request is forwarded to Play Services instead. Play
                    // Services shouldn't find any credentials either, but it
                    // will show a bottomsheet to that effect.
                    assert mConditionalUiState == ConditionalUiState.NONE;
                    assert !options.isConditional;

                    if (mNoCredentialsFallback != null) mNoCredentialsFallback.run();
                } else {
                    // Includes:
                    //  * GetCredentialException.TYPE_UNKNOWN
                    //  * GetCredentialException.TYPE_NO_CREATE_OPTIONS
                    //  * GetCredentialException.TYPE_INTERRUPTED
                    mErrorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                    mMetricsHelper.reportGetCredentialMetrics(
                            CredManGetRequestEnum.FAILURE, mConditionalUiState);
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
                    mBridgeProvider.getBridge().cleanupCredManRequest(mFrameHost);
                    mBarrier.onCredManCancelled();
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
                    mMetricsHelper.reportGetCredentialMetrics(
                            CredManGetRequestEnum.FAILURE, mConditionalUiState);
                    mConditionalUiState = options.isConditional
                            ? ConditionalUiState.WAITING_FOR_SELECTION
                            : ConditionalUiState.NONE;
                    notifyBrowserOnCredManClosed(false);
                    mErrorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }

                if (!TYPE_PASSKEY.equals(type)) {
                    mBridgeProvider.getBridge().onPasswordCredentialReceived(mFrameHost,
                            data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_ID"),
                            data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_PASSWORD"));
                    mMetricsHelper.reportGetCredentialMetrics(
                            CredManGetRequestEnum.SUCCESS_PASSWORD, mConditionalUiState);
                    return;
                }

                String json =
                        data.getString(CRED_MAN_PREFIX + "BUNDLE_KEY_AUTHENTICATION_RESPONSE_JSON");
                byte[] responseSerialized =
                        Fido2CredentialRequestJni.get().getCredentialResponseFromJson(json);
                if (responseSerialized == null) {
                    Log.e(TAG, "Failed to convert response from CredMan to Mojo object: %s", json);
                    mMetricsHelper.reportGetCredentialMetrics(
                            CredManGetRequestEnum.FAILURE, mConditionalUiState);
                    mConditionalUiState = options.isConditional
                            ? ConditionalUiState.WAITING_FOR_SELECTION
                            : ConditionalUiState.NONE;
                    notifyBrowserOnCredManClosed(false);
                    mErrorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }

                GetAssertionAuthenticatorResponse response;
                try {
                    response = GetAssertionAuthenticatorResponse.deserialize(
                            ByteBuffer.wrap(responseSerialized));
                } catch (org.chromium.mojo.bindings.DeserializationException e) {
                    logDeserializationException(e);
                    mMetricsHelper.reportGetCredentialMetrics(
                            CredManGetRequestEnum.FAILURE, mConditionalUiState);
                    mConditionalUiState = options.isConditional
                            ? ConditionalUiState.WAITING_FOR_SELECTION
                            : ConditionalUiState.NONE;
                    notifyBrowserOnCredManClosed(false);
                    mErrorCallback.onResult(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
                response.info.clientDataJson = mClientDataJson;
                response.extensions.echoAppidExtension = options.extensions.appid != null;
                mConditionalUiState = options.isConditional
                        ? ConditionalUiState.WAITING_FOR_SELECTION
                        : ConditionalUiState.NONE;
                notifyBrowserOnCredManClosed(true);
                mMetricsHelper.reportGetCredentialMetrics(
                        CredManGetRequestEnum.SUCCESS_PASSKEY, mConditionalUiState);
                if (mFrameHost != null) {
                    mFrameHost.notifyWebAuthnAssertionRequestSucceeded();
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
        try {
            final Object getCredentialRequest = buildGetCredentialRequest(options, originString,
                    maybeClientDataHash, mRequestPasswords,
                    /*preferImmediatelyAvailable=*/!options.isConditional, ignoreGpm);
            if (getCredentialRequest == null) {
                mMetricsHelper.reportGetCredentialMetrics(
                        CredManGetRequestEnum.COULD_NOT_SEND_REQUEST, mConditionalUiState);
                mConditionalUiState = options.isConditional
                        ? ConditionalUiState.WAITING_FOR_SELECTION
                        : ConditionalUiState.NONE;
                return AuthenticatorStatus.NOT_ALLOWED_ERROR;
            }
            final Object manager = credentialManagerService(mContext);
            manager.getClass()
                    .getMethod("getCredential", Context.class, getCredentialRequest.getClass(),
                            android.os.CancellationSignal.class,
                            java.util.concurrent.Executor.class, OutcomeReceiver.class)
                    .invoke(manager, mContext, getCredentialRequest, null,
                            mContext.getMainExecutor(), receiver);
            mMetricsHelper.reportGetCredentialMetrics(
                    CredManGetRequestEnum.SENT_REQUEST, mConditionalUiState);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Reflection failed; are you running on Android 14?", e);
            mMetricsHelper.reportGetCredentialMetrics(
                    CredManGetRequestEnum.COULD_NOT_SEND_REQUEST, mConditionalUiState);
            mConditionalUiState = options.isConditional ? ConditionalUiState.WAITING_FOR_SELECTION
                                                        : ConditionalUiState.NONE;
            return AuthenticatorStatus.UNKNOWN_ERROR;
        }
        return AuthenticatorStatus.SUCCESS;
    }

    public void cancelConditionalGetAssertion(RenderFrameHost frameHost) {
        switch (mConditionalUiState) {
            case WAITING_FOR_CREDENTIAL_LIST:
                mConditionalUiState = ConditionalUiState.CANCEL_PENDING;
                mBarrier.onCredManCancelled();
                break;
            case WAITING_FOR_SELECTION:
                mBridgeProvider.getBridge().cleanupCredManRequest(frameHost);
                mConditionalUiState = ConditionalUiState.NONE;
                mBarrier.onCredManCancelled();
                break;
            default:
                // No action
        }
    }

    public void setCredManClassesForTesting(Object credentialManager, Class createRequestBuilder,
            Class getRequestBuilder, Class credentialOptionBuilder,
            CredManMetricsHelper metricsHelper) {
        mCredentialManagerServiceForTesting = credentialManager;
        mCredManCreateRequestBuilderClassForTesting = createRequestBuilder;
        mCredManGetRequestBuilderClassForTesting = getRequestBuilder;
        mCredManCredentialOptionBuilderClassForTesting = credentialOptionBuilder;
        mMetricsHelper = metricsHelper;
    }

    void setRequestPasswords(boolean requestPasswords) {
        mRequestPasswords = requestPasswords;
    }

    Object credentialManagerService(Context context) {
        if (mCredentialManagerServiceForTesting != null) {
            return mCredentialManagerServiceForTesting;
        }
        return context.getSystemService(Context.CREDENTIAL_SERVICE);
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

    private void notifyBrowserOnCredManClosed(boolean success) {
        if (mBridgeProvider.getBridge() == null) return;
        mBridgeProvider.getBridge().onCredManUiClosed(mFrameHost, success);
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
     * Construct a CredMan request for credentials.
     *
     * @param options The WebAuthn get() call.
     * @param originString The origin that made the WebAuthn request.
     * @param maybeClientDataHash Either null, to have the ClientDataJSON built by this function
     *         and populated in `mClientDataJson`, or else an explicit ClientDataJSON hash.
     * @param requestPasswords True if password credentials should also be requested.
     * @param preferImmediatelyAvailable True to make the eventual request fail with a
     *         `NO_CREDENTIAL` error if there are no credentials found.
     * @param ignoreGpm True if Google Password Manager should ignore CredMan requests.
     */
    private Object buildGetCredentialRequest(PublicKeyCredentialRequestOptions options,
            String originString, byte[] maybeClientDataHash, boolean requestPasswords,
            boolean preferImmediatelyAvailable, boolean ignoreGpm)
            throws ReflectiveOperationException {
        final String requestAsJson =
                Fido2CredentialRequestJni.get().getOptionsToJson(options.serialize());
        final byte[] clientDataHash = maybeClientDataHash != null
                ? maybeClientDataHash
                : buildClientDataJsonAndComputeHash(ClientDataRequestType.WEB_AUTHN_GET,
                        originString, options.challenge, mIsCrossOrigin,
                        /*paymentOptions=*/null, options.relyingPartyId, /*topOrigin=*/null);
        if (clientDataHash == null) {
            Log.e(TAG, "ClientDataJson generation failed.");
            return null;
        }

        boolean hasAllowCredentials =
                options.allowCredentials != null && options.allowCredentials.length != 0;
        Bundle publicKeyCredentialOptionBundle =
                buildPublicKeyCredentialOptionBundle(
                        requestAsJson,
                        clientDataHash,
                        ignoreGpm,
                        /* allowAutoSelect= */ hasAllowCredentials);

        // Build the CredentialOption for passkeys:
        Object credentialOption;
        final Class<?> credentialOptionBuilderClass = credManCredentialOptionBuilderClass();
        final Object credentialOptionBuilder =
                credentialOptionBuilderClass
                        .getConstructor(String.class, Bundle.class, Bundle.class)
                        .newInstance(TYPE_PASSKEY, publicKeyCredentialOptionBundle,
                                publicKeyCredentialOptionBundle);
        credentialOption =
                credentialOptionBuilderClass.getMethod("build").invoke(credentialOptionBuilder);

        // Build the GetCredentialRequest:
        final Class<?> getCredentialRequestBuilderClass = credManGetRequestBuilderClass();
        Bundle getCredentialRequestBundle = new Bundle();
        if (!ignoreGpm) {
            getCredentialRequestBundle.putParcelable(
                    CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_UI_BRANDING_COMPONENT_NAME",
                    GPM_COMPONENT_NAME);
        }
        // The CredMan UI for the case where there aren't any credentials isn't
        // suitable for the modal case. This bundle key requests that the
        // request fail immediately if there aren't any credentials. It'll fail
        // with a `CRED_MAN_EXCEPTION_GET_CREDENTIAL_TYPE_NO_CREDENTIAL` error
        // which is handled above by calling Play Services to render the error.
        getCredentialRequestBundle.putBoolean(
                CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS",
                preferImmediatelyAvailable && mPlayServicesAvailable);
        final Object getCredentialRequestBuilderObject =
                getCredentialRequestBuilderClass.getConstructor(Bundle.class)
                        .newInstance(getCredentialRequestBundle);
        getCredentialRequestBuilderClass
                .getMethod("addCredentialOption", credentialOption.getClass())
                .invoke(getCredentialRequestBuilderObject, credentialOption);
        if (requestPasswords) {
            Object passwordCredentialOption = buildPasswordOption(ignoreGpm);
            if (passwordCredentialOption != null) {
                getCredentialRequestBuilderClass
                        .getMethod("addCredentialOption", passwordCredentialOption.getClass())
                        .invoke(getCredentialRequestBuilderObject, passwordCredentialOption);
            }
        }
        getCredentialRequestBuilderClass.getMethod("setOrigin", String.class)
                .invoke(getCredentialRequestBuilderObject, originString);
        return getCredentialRequestBuilderClass.getMethod("build").invoke(
                getCredentialRequestBuilderObject);
    }

    private Bundle buildPublicKeyCredentialOptionBundle(
            String requestAsJson,
            byte[] clientDataHash,
            boolean ignoreGpm,
            boolean allowAutoSelect) {
        final Bundle publicKeyCredentialOptionBundle = new Bundle();
        publicKeyCredentialOptionBundle.putString(CRED_MAN_PREFIX + "BUNDLE_KEY_SUBTYPE",
                CRED_MAN_PREFIX + "BUNDLE_VALUE_SUBTYPE_GET_PUBLIC_KEY_CREDENTIAL_OPTION");
        publicKeyCredentialOptionBundle.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_JSON", requestAsJson);
        publicKeyCredentialOptionBundle.putByteArray(
                CRED_MAN_PREFIX + "BUNDLE_KEY_CLIENT_DATA_HASH", clientDataHash);

        if (allowAutoSelect) {
            // Auto-select means that, when an allowlist is present and one of
            // the providers matches with it, the account selector can be
            // skipped. (However, if two or more providers match with the
            // allowlist then the selector will, sadly, still be shown.)
            publicKeyCredentialOptionBundle.putBoolean(CRED_MAN_IS_AUTO_SELECT_ALLOWED, true);
        }

        publicKeyCredentialOptionBundle.putString(CHANNEL_KEY, getChannel());
        publicKeyCredentialOptionBundle.putBoolean(INCOGNITO_KEY, isIncognito());
        publicKeyCredentialOptionBundle.putBoolean(IGNORE_GPM, ignoreGpm);
        return publicKeyCredentialOptionBundle;
    }

    private Object buildPasswordOption(boolean ignoreGpm) throws ReflectiveOperationException {
        Object passwordCredentialOption;
        Bundle passwordOptionBundle = new Bundle();
        passwordOptionBundle.putString(CHANNEL_KEY, getChannel());
        passwordOptionBundle.putBoolean(INCOGNITO_KEY, isIncognito());
        passwordOptionBundle.putBoolean(PASSWORDS_ONLY_FOR_THE_CHANNEL, true);
        passwordOptionBundle.putBoolean(PASSWORDS_WITH_NO_USERNAME_INCLUDED, true);
        passwordOptionBundle.putBoolean(IGNORE_GPM, ignoreGpm);

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

        return passwordCredentialOption;
    }

    private final boolean isIncognito() {
        if (mFrameHost == null) return false;
        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mFrameHost);
        return webContents == null ? false : webContents.isIncognito();
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
