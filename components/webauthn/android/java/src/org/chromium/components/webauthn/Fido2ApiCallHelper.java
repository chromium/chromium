// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.app.PendingIntent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Parcel;
import android.os.ResultReceiver;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.Task;

import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.webauthn.Fido2ApiCall.Fido2ApiCallParams;

import java.security.NoSuchAlgorithmException;
import java.util.List;

/**
 * Provides helper methods to wrap Fido2ApiCall invocations. This class is useful to override GMS
 * Core API interactions from Fido2CredentialRequest in tests.
 */
public class Fido2ApiCallHelper {
    private static Fido2ApiCallHelper sInstance;

    public static void overrideInstanceForTesting(Fido2ApiCallHelper instance) {
        sInstance = instance;
    }

    /**
     * @return The Fido2ApiCallHelper for use during the lifetime of the browser process.
     */
    public static Fido2ApiCallHelper getInstance() {
        if (sInstance == null) {
            sInstance = new Fido2ApiCallHelper();
        }
        return sInstance;
    }

    public boolean arePlayServicesAvailable() {
        return ExternalAuthUtils.getInstance()
                .canUseGooglePlayServices(new UserRecoverableErrorHandler.Silent());
    }

    public void invokeFido2GetCredentials(
            AuthenticationContextProvider authenticationContextProvider,
            String relyingPartyId,
            OnSuccessListener<List<WebauthnCredentialDetails>> successCallback,
            OnFailureListener failureCallback) {
        Fido2ApiCallParams params =
                WebauthnModeProvider.getInstance()
                        .getFido2ApiCallParams(authenticationContextProvider.getWebContents());
        Fido2ApiCall call = new Fido2ApiCall(authenticationContextProvider.getContext(), params);
        Parcel args = call.start();
        Fido2ApiCall.WebauthnCredentialDetailsListResult result =
                new Fido2ApiCall.WebauthnCredentialDetailsListResult();
        args.writeStrongBinder(result);
        args.writeString(relyingPartyId);

        Task<List<WebauthnCredentialDetails>> task =
                call.run(
                        Fido2ApiCall.METHOD_BROWSER_GETCREDENTIALS,
                        Fido2ApiCall.TRANSACTION_GETCREDENTIALS,
                        args,
                        result);
        task.addOnSuccessListener(successCallback);
        task.addOnFailureListener(failureCallback);
    }

    public void invokeFido2MakeCredential(
            AuthenticationContextProvider authenticationContextProvider,
            PublicKeyCredentialCreationOptions options,
            Uri uri,
            byte[] clientDataHash,
            Bundle browserOptions,
            ResultReceiver resultReceiver,
            OnSuccessListener<PendingIntent> successCallback,
            OnFailureListener failureCallback)
            throws NoSuchAlgorithmException {
        Fido2ApiCallParams params =
                WebauthnModeProvider.getInstance()
                        .getFido2ApiCallParams(authenticationContextProvider.getWebContents());
        Fido2ApiCall call = new Fido2ApiCall(authenticationContextProvider.getContext(), params);
        Parcel args = call.start();
        Fido2ApiCall.PendingIntentResult result =
                new Fido2ApiCall.PendingIntentResult(params.mCallbackDescriptor);
        args.writeStrongBinder(result);
        args.writeInt(1); // This indicates that the following options are present.

        params.mMethodInterfaces.makeCredential(
                options, uri, clientDataHash, browserOptions, resultReceiver, args);

        Task<PendingIntent> task =
                call.run(params.mRegisterMethodId, Fido2ApiCall.TRANSACTION_REGISTER, args, result);
        task.addOnSuccessListener(successCallback);
        task.addOnFailureListener(failureCallback);
    }

    public void invokeFido2GetAssertion(
            AuthenticationContextProvider authenticationContextProvider,
            PublicKeyCredentialRequestOptions options,
            Uri uri,
            byte[] clientDataHash,
            ResultReceiver resultReceiver,
            OnSuccessListener<PendingIntent> successCallback,
            OnFailureListener failureCallback) {
        Fido2ApiCallParams params =
                WebauthnModeProvider.getInstance()
                        .getFido2ApiCallParams(authenticationContextProvider.getWebContents());
        Fido2ApiCall call = new Fido2ApiCall(authenticationContextProvider.getContext(), params);
        Parcel args = call.start();
        Fido2ApiCall.PendingIntentResult result =
                new Fido2ApiCall.PendingIntentResult(params.mCallbackDescriptor);
        args.writeStrongBinder(result);
        args.writeInt(1); // This indicates that the following options are present.

        params.mMethodInterfaces.getAssertion(
                options, uri, clientDataHash, /* tunnelId= */ null, resultReceiver, args);
        Task<PendingIntent> task =
                call.run(params.mSignMethodId, Fido2ApiCall.TRANSACTION_SIGN, args, result);
        task.addOnSuccessListener(successCallback);
        task.addOnFailureListener(failureCallback);
    }
}
