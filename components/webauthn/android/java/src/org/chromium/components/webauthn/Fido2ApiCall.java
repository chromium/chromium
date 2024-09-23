// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.app.PendingIntent;
import android.content.Context;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.IInterface;
import android.os.Looper;
import android.os.Parcel;
import android.os.ResultReceiver;

import androidx.annotation.Nullable;

import com.google.android.gms.common.api.Api;
import com.google.android.gms.common.api.Api.ApiOptions;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.GoogleApi;
import com.google.android.gms.common.api.GoogleApiClient.ConnectionCallbacks;
import com.google.android.gms.common.api.GoogleApiClient.OnConnectionFailedListener;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.common.api.internal.ApiExceptionMapper;
import com.google.android.gms.common.api.internal.TaskApiCall;
import com.google.android.gms.common.internal.ClientSettings;
import com.google.android.gms.common.internal.GmsClient;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.TaskCompletionSource;

import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;

import java.security.NoSuchAlgorithmException;
import java.util.List;

/**
 * Fido2ApiCall handles making Binder calls to Play Services' FIDO API.
 *
 * <p>There are two FIDO APIs, one for browsers that can assert any RP ID, and one for apps where
 * the RP ID is checked against assetlinks.json from the site. This class can handle both. API calls
 * are made directly, rather than using the Play Services SDK, to save code size and to allow new
 * features to be used more easily.
 *
 * <p>API calls consist of two Binder calls each. Binder calls are synchronous and the first
 * delivers arguments to Play Services plus a Binder object to receive the result. The second Binder
 * call is from Play Services back to Chromium where the result is returned. If the call requires
 * Play Services to collect user interaction then that result will be a {@link PendingIntent} which
 * needs to be started in order to actually run the operation. The real result is then delivered to
 * Chromium via {@link Activity.onActivityResult}. This class does not handle that part of the
 * operation, only the initial Binder calls.
 *
 * <p>Calls are started by constructing an instance of this class and calling {@link start} to get a
 * {@link Parcel} that arguments can be written to. The first argument is always the Binder object
 * that receives the result, for example an instance of {@link BooleanResult} or {@link
 * PendingIntentResult}. Following that are the inputs to the call.
 *
 * <p>Once the arguments are prepared, call {@link run} to perform the first Binder call. That
 * returns a {@link Task} that will resolve with the result when it's ready.
 *
 * <p>Here's an example:
 *
 * <pre>{@code
 * Fido2ApiCall call = new Fido2ApiCall(ContextUtils.getApplicationContext());
 * Parcel args = call.start();
 * Fido2ApiCall.PendingIntentResult result = new Fido2ApiCall.PendingIntentResult(call);
 * args.writeStrongBinder(result);
 * // Add parameter to `args`.
 *
 * Task<PendingIntent> task = call.run(
 *      Fido2ApiCall.METHOD_BROWSER_REGISTER, Fido2ApiCall.TRANSACTION_REGISTER, args, result);
 * }</pre>
 */
public final class Fido2ApiCall extends GoogleApi<ApiOptions.NoOptions> {
    public static class Fido2ApiCallParams {
        public final Api<ApiOptions.NoOptions> mApi;
        public final String mDescriptor;
        public final @Nullable String mCallbackDescriptor;
        public final int mRegisterMethodId;
        public final int mSignMethodId;
        public final int mIsUserVerifyingPlatformAuthenticatorAvailableMethodId;
        public final @Nullable Fido2Api.Calls mMethodInterfaces;

        Fido2ApiCallParams(
                Api<ApiOptions.NoOptions> api,
                String descriptor,
                String callbackDescriptor,
                int registerMethodId,
                int signMethodId,
                int isUserVerifyingPlatformAuthenticatorAvailableMethodId,
                Fido2Api.Calls methodInterfaces) {
            mApi = api;
            mDescriptor = descriptor;
            mCallbackDescriptor = callbackDescriptor;
            mRegisterMethodId = registerMethodId;
            mSignMethodId = signMethodId;
            mIsUserVerifyingPlatformAuthenticatorAvailableMethodId =
                    isUserVerifyingPlatformAuthenticatorAvailableMethodId;
            mMethodInterfaces = methodInterfaces;
        }
    }

    public static final int METHOD_BROWSER_REGISTER = 5412;
    public static final int METHOD_BROWSER_SIGN = 5413;
    public static final int METHOD_BROWSER_ISUVPAA = 5416;
    public static final int METHOD_BROWSER_GETCREDENTIALS = 5430;
    public static final int METHOD_BROWSER_HYBRID_SIGN = 5442;
    public static final int METHOD_GET_LINK_INFO = 5450;

    public static final int METHOD_APP_REGISTER = 5407;
    public static final int METHOD_APP_SIGN = 5408;
    public static final int METHOD_APP_ISUVPAA = 5411;

    public static final int TRANSACTION_REGISTER = IBinder.FIRST_CALL_TRANSACTION + 0;
    public static final int TRANSACTION_SIGN = IBinder.FIRST_CALL_TRANSACTION + 1;
    public static final int TRANSACTION_ISUVPAA = IBinder.FIRST_CALL_TRANSACTION + 2;
    public static final int TRANSACTION_GETCREDENTIALS = IBinder.FIRST_CALL_TRANSACTION + 3;
    public static final int TRANSACTION_HYBRID_SIGN = IBinder.FIRST_CALL_TRANSACTION + 4;
    public static final int TRANSACTION_GET_LINK_INFO = IBinder.FIRST_CALL_TRANSACTION + 0;

    private static final String APP_DESCRIPTOR =
            "com.google.android.gms.fido.fido2.internal.regular.IFido2AppService";
    private static final String APP_CALLBACK_DESCRIPTOR =
            "com.google.android.gms.fido.fido2.internal.regular.IFido2AppCallbacks";
    private static final String APP_START_SERVICE_ACTION =
            "com.google.android.gms.fido.fido2.regular.START";
    private static final int APP_API_ID = 148;
    private static final Api.ClientKey<FidoClient> APP_CLIENT_KEY = new Api.ClientKey<>();
    private static final Fido2Api.Calls APP_INTERFACES =
            new Fido2Api.Calls() {
                @Override
                public void makeCredential(
                        PublicKeyCredentialCreationOptions options,
                        @Nullable Uri uri,
                        @Nullable byte[] clientDataHash,
                        @Nullable Bundle browserOptions,
                        ResultReceiver resultReceiver,
                        Parcel parcel)
                        throws NoSuchAlgorithmException {
                    Fido2Api.appendMakeCredentialOptionsToParcel(options, resultReceiver, parcel);
                }

                @Override
                public void getAssertion(
                        PublicKeyCredentialRequestOptions options,
                        Uri uri,
                        byte[] clientDataHash,
                        byte[] tunnelId,
                        ResultReceiver resultReceiver,
                        Parcel parcel) {
                    Fido2Api.appendGetAssertionOptionsToParcel(
                            options, clientDataHash, resultReceiver, parcel);
                }
            };

    private static final String BROWSER_DESCRIPTOR =
            "com.google.android.gms.fido.fido2.internal.privileged.IFido2PrivilegedService";
    private static final String BROWSER_CALLBACK_DESCRIPTOR =
            "com.google.android.gms.fido.fido2.internal.privileged.IFido2PrivilegedCallbacks";
    private static final String BROWSER_START_SERVICE_ACTION =
            "com.google.android.gms.fido.fido2.privileged.START";
    private static final int BROWSER_API_ID = 149;
    private static final Api.ClientKey<FidoClient> BROWSER_CLIENT_KEY = new Api.ClientKey<>();
    private static final Fido2Api.Calls BROWSER_INTERFACES =
            new Fido2Api.Calls() {
                @Override
                public void makeCredential(
                        PublicKeyCredentialCreationOptions options,
                        Uri uri,
                        byte[] clientDataHash,
                        Bundle browserOptions,
                        ResultReceiver resultReceiver,
                        Parcel parcel)
                        throws NoSuchAlgorithmException {
                    Fido2Api.appendBrowserMakeCredentialOptionsToParcel(
                            options, uri, clientDataHash, browserOptions, resultReceiver, parcel);
                }

                @Override
                public void getAssertion(
                        PublicKeyCredentialRequestOptions options,
                        @Nullable Uri uri,
                        @Nullable byte[] clientDataHash,
                        @Nullable byte[] tunnelId,
                        ResultReceiver resultReceiver,
                        Parcel parcel) {
                    Fido2Api.appendBrowserGetAssertionOptionsToParcel(
                            options, uri, clientDataHash, tunnelId, resultReceiver, parcel);
                }
            };

    private static final String FIRSTPARTY_DESCRIPTOR =
            "com.google.android.gms.fido.fido2.internal.firstparty.IFido2FirstPartyService";
    private static final String FIRSTPARTY_START_SERVICE_ACTION =
            "com.google.android.gms.fido.fido2.firstparty.START";
    private static final int FIRSTPARTY_API_ID = 347;
    private static final Api.ClientKey<FidoClient> FIRSTPARTY_CLIENT_KEY = new Api.ClientKey<>();

    static final Fido2ApiCallParams APP_API =
            new Fido2ApiCallParams(
                    new Api<>(
                            "Fido.FIDO2_API",
                            new FidoClient.Builder(
                                    APP_DESCRIPTOR, APP_START_SERVICE_ACTION, APP_API_ID),
                            APP_CLIENT_KEY),
                    APP_DESCRIPTOR,
                    APP_CALLBACK_DESCRIPTOR,
                    METHOD_APP_REGISTER,
                    METHOD_APP_SIGN,
                    METHOD_APP_ISUVPAA,
                    APP_INTERFACES);

    static final Fido2ApiCallParams BROWSER_API =
            new Fido2ApiCallParams(
                    new Api<>(
                            "Fido.FIDO2_PRIVILEGED_API",
                            new FidoClient.Builder(
                                    BROWSER_DESCRIPTOR,
                                    BROWSER_START_SERVICE_ACTION,
                                    BROWSER_API_ID),
                            BROWSER_CLIENT_KEY),
                    BROWSER_DESCRIPTOR,
                    BROWSER_CALLBACK_DESCRIPTOR,
                    METHOD_BROWSER_REGISTER,
                    METHOD_BROWSER_SIGN,
                    METHOD_BROWSER_ISUVPAA,
                    BROWSER_INTERFACES);

    public static final Fido2ApiCallParams FIRST_PARTY_API =
            new Fido2ApiCallParams(
                    new Api<>(
                            "Fido.FIDO2_FIRSTPARTY_API",
                            new FidoClient.Builder(
                                    FIRSTPARTY_DESCRIPTOR,
                                    FIRSTPARTY_START_SERVICE_ACTION,
                                    FIRSTPARTY_API_ID),
                            FIRSTPARTY_CLIENT_KEY),
                    FIRSTPARTY_DESCRIPTOR,
                    /* callbackDescriptor */ null,
                    /* registerMethodId */ 0,
                    /* signMethodId */ 0,
                    /* isUserVerifyingPlatformAuthenticatorAvailable */ 0,
                    /* methodInterfaces */ null);

    private final String mDescriptor;

    /**
     * Construct an instance.
     *
     * @param context the Android {@link Context} for the current process.
     * @param api the service to call. One of the public static Api objects from this class.
     */
    public Fido2ApiCall(Context context, Fido2ApiCallParams apiParams) {
        super(context, apiParams.mApi, ApiOptions.NO_OPTIONS, new ApiExceptionMapper());
        mDescriptor = apiParams.mDescriptor;
    }

    public Parcel start() {
        Parcel p = Parcel.obtain();
        p.writeInterfaceToken(mDescriptor);
        return p;
    }

    /**
     * Make a Binder call to Play Services.
     *
     * @param methodId one of the METHOD_* constants.
     * @param transactionId one of the TRANSACTION_* constants.
     * @param args a {@link Parcel}, created by {@link start}, that the callback and arguments have
     *     been written to.
     * @param callback the callback {@link Binder} that was added to args.
     */
    public <Result> Task<Result> run(
            int methodId, int transactionId, Parcel args, Callback<Result> callback) {
        return doRead(
                TaskApiCall.<FidoClient, Result>builder()
                        .run(
                                (impl, completionSource) -> {
                                    callback.setCompletionSource(completionSource);

                                    Parcel out = Parcel.obtain();
                                    try {
                                        impl.getService()
                                                .asBinder()
                                                .transact(transactionId, args, out, 0);
                                        out.readException();
                                    } finally {
                                        args.recycle();
                                        out.recycle();
                                    }
                                })
                        .setMethodKey(methodId)
                        // It's possible to call `.setFeatures` here with a Feature[].
                        // However, the version of play-services-basement used at the time of
                        // writing crashes if a feature is missing in the target Play Services
                        // process. Thus we'll need to check the version number explicitly.
                        // (Which is a good idea anyway because we might not want to direct
                        // the user to the Play Store to update Play Services.)
                        .build());
    }

    public static final class BooleanResult extends Binder implements Callback<Boolean> {
        private TaskCompletionSource<Boolean> mCompletionSource;

        @Override
        public void setCompletionSource(TaskCompletionSource<Boolean> cs) {
            mCompletionSource = cs;
        }

        @Override
        public boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
            data.enforceInterface("com.google.android.gms.fido.fido2.api.IBooleanCallback");
            switch (code) {
                case IBinder.FIRST_CALL_TRANSACTION + 0:
                    mCompletionSource.setResult(data.readInt() != 0);
                    break;
                case IBinder.FIRST_CALL_TRANSACTION + 1:
                    Status status = null;
                    if (data.readInt() != 0) {
                        status = Status.CREATOR.createFromParcel(data);
                    }
                    mCompletionSource.setException(new ApiException(status));
                    break;
                default:
                    return false;
            }

            reply.writeNoException();
            return true;
        }
    }

    public static final class ByteArrayResult extends Binder implements Callback<byte[]> {
        private TaskCompletionSource<byte[]> mCompletionSource;

        @Override
        public void setCompletionSource(TaskCompletionSource<byte[]> cs) {
            mCompletionSource = cs;
        }

        @Override
        public boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
            data.enforceInterface("com.google.android.gms.fido.fido2.api.IByteArrayCallback");
            switch (code) {
                case IBinder.FIRST_CALL_TRANSACTION + 0:
                    mCompletionSource.setResult(data.createByteArray());
                    break;
                case IBinder.FIRST_CALL_TRANSACTION + 1:
                    Status status = null;
                    if (data.readInt() != 0) {
                        status = Status.CREATOR.createFromParcel(data);
                    }
                    mCompletionSource.setException(new ApiException(status));
                    break;
                default:
                    return false;
            }

            reply.writeNoException();
            return true;
        }
    }

    public static final class WebauthnCredentialDetailsListResult extends Binder
            implements Callback<List<WebauthnCredentialDetails>> {
        private TaskCompletionSource<List<WebauthnCredentialDetails>> mCompletionSource;

        @Override
        public void setCompletionSource(TaskCompletionSource<List<WebauthnCredentialDetails>> cs) {
            mCompletionSource = cs;
        }

        @Override
        public boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
            data.enforceInterface("com.google.android.gms.fido.fido2.api.ICredentialListCallback");
            switch (code) {
                case IBinder.FIRST_CALL_TRANSACTION + 0:
                    try {
                        mCompletionSource.setResult(Fido2Api.parseCredentialList(data));
                    } catch (IllegalArgumentException e) {
                        mCompletionSource.setException(e);
                    }
                    break;
                case IBinder.FIRST_CALL_TRANSACTION + 1:
                    Status status = null;
                    if (data.readInt() != 0) {
                        status = Status.CREATOR.createFromParcel(data);
                    }
                    mCompletionSource.setException(new ApiException(status));
                    break;
                default:
                    return false;
            }

            reply.writeNoException();
            return true;
        }
    }

    public static final class PendingIntentResult extends Binder
            implements Callback<PendingIntent> {
        private final String mCallbackDescriptor;
        private TaskCompletionSource<PendingIntent> mCompletionSource;

        public PendingIntentResult(String callbackDescriptor) {
            mCallbackDescriptor = callbackDescriptor;
        }

        @Override
        public void setCompletionSource(TaskCompletionSource<PendingIntent> cs) {
            mCompletionSource = cs;
        }

        @Override
        public boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
            switch (code) {
                case IBinder.FIRST_CALL_TRANSACTION + 0:
                    data.enforceInterface(mCallbackDescriptor);

                    Status status = null;
                    if (data.readInt() != 0) {
                        status = Status.CREATOR.createFromParcel(data);
                    }

                    PendingIntent intent = null;
                    if (data.readInt() != 0) {
                        intent = PendingIntent.CREATOR.createFromParcel(data);
                    }

                    if (status.isSuccess()) {
                        mCompletionSource.setResult(intent);
                    } else {
                        mCompletionSource.setException(new ApiException(status));
                    }
                    break;
                default:
                    return false;
            }

            reply.writeNoException();
            return true;
        }
    }

    private interface Callback<Result> {
        void setCompletionSource(TaskCompletionSource<Result> cs);
    }

    private static class Interface implements IInterface {
        final IBinder mRemote;

        public Interface(IBinder remote) {
            mRemote = remote;
        }

        @Override
        public IBinder asBinder() {
            return mRemote;
        }
    }

    private static final class FidoClient extends GmsClient<Interface> {
        private final String mDescriptor;
        private final String mStartServiceAction;

        FidoClient(
                String descriptor,
                String startServiceAction,
                int apiId,
                Context context,
                Looper looper,
                ClientSettings clientSettings,
                ConnectionCallbacks callbacks,
                OnConnectionFailedListener failedListener) {
            super(context, looper, apiId, clientSettings, callbacks, failedListener);
            mDescriptor = descriptor;
            mStartServiceAction = startServiceAction;
        }

        @Override
        protected String getStartServiceAction() {
            return mStartServiceAction;
        }

        @Override
        protected String getServiceDescriptor() {
            return mDescriptor;
        }

        @Override
        protected Interface createServiceInterface(IBinder binder) {
            return new Interface(binder);
        }

        @Override
        protected Bundle getGetServiceRequestExtraArgs() {
            Bundle args = new Bundle();
            args.putString("FIDO2_ACTION_START_SERVICE", getStartServiceAction());
            return args;
        }

        @Override
        public int getMinApkVersion() {
            // This minimum should be moot because it's enforced in `AuthenticatorImpl`.
            return GmsCoreUtils.GMSCORE_MIN_VERSION;
        }

        public static class Builder
                extends Api.AbstractClientBuilder<FidoClient, ApiOptions.NoOptions> {
            private final String mDescriptor;
            private final String mStartServiceAction;
            private final int mApiId;

            Builder(String descriptor, String startServiceAction, int apiId) {
                mDescriptor = descriptor;
                mStartServiceAction = startServiceAction;
                mApiId = apiId;
            }

            @Override
            public FidoClient buildClient(
                    Context context,
                    Looper looper,
                    ClientSettings clientSettings,
                    ApiOptions.NoOptions options,
                    ConnectionCallbacks callbacks,
                    OnConnectionFailedListener failedListener) {
                return new FidoClient(
                        mDescriptor,
                        mStartServiceAction,
                        mApiId,
                        context,
                        looper,
                        clientSettings,
                        callbacks,
                        failedListener);
            }
        }
    }
}
