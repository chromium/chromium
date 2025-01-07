// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import static androidx.core.app.ActivityCompat.startIntentSenderForResult;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.IntentSender.SendIntentException;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ResultReceiver;

import com.google.android.gms.identitycredentials.CredentialOption;
import com.google.android.gms.identitycredentials.GetCredentialException;
import com.google.android.gms.identitycredentials.GetCredentialRequest;
import com.google.android.gms.identitycredentials.IdentityCredentialClient;
import com.google.android.gms.identitycredentials.IdentityCredentialManager;
import com.google.android.gms.identitycredentials.IntentHelper;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ServiceLoaderUtil;

import java.util.Arrays;

public class IdentityCredentialsDelegate {
    private static final String TAG = "IdentityCredentials";

    // Arbitrary request code that is used when invoking the GMSCore API.
    private static final int REQUEST_CODE_DIGITAL_CREDENTIALS = 777;

    public Promise<String> get(String origin, String request) {
        // TODO(crbug.com/40257092): implement this.
        return null;
    }

    public Promise<byte[]> get(Activity window, String origin, String request) {
        final IdentityCredentialClient client;
        try {
            client = IdentityCredentialManager.Companion.getClient(window);
        } catch (Exception e) {
            // Thrown when running in phones without the most current GMS
            // version.
            return Promise.rejected();
        }

        final Promise<byte[]> result = new Promise<byte[]>();

        ResultReceiver resultReceiver =
                new ResultReceiver(new Handler(Looper.getMainLooper())) {
                    // android.credentials.GetCredentialException requires API level 34
                    @SuppressLint("NewApi")
                    @Override
                    protected void onReceiveResult(int code, Bundle data) {
                        Log.d(TAG, "Received a response");
                        try {
                            var response = IntentHelper.extractGetCredentialResponse(code, data);
                            var token =
                                    response.getCredential()
                                            .getData()
                                            .getByteArray("identityToken");
                            result.fulfill(token);
                        } catch (Exception e) {
                            Log.e(TAG, e.toString());

                            if (e instanceof GetCredentialException
                                    && Build.VERSION.SDK_INT
                                            >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                                String exceptionType = ((GetCredentialException) e).getType();
                                result.reject(
                                        new android.credentials.GetCredentialException(
                                                exceptionType, e.getMessage()));
                            } else {
                                result.reject(e);
                            }
                        }
                    }
                };

        client.getCredential(
                        new GetCredentialRequest(
                                Arrays.asList(
                                        new CredentialOption(
                                                "com.credman.IdentityCredential",
                                                new Bundle(),
                                                new Bundle(),
                                                request,
                                                "",
                                                "")),
                                new Bundle(),
                                origin,
                                resultReceiver))
                .addOnSuccessListener(
                        response -> {
                            try {
                                Log.d(TAG, "Sending an intent for sender");
                                Log.d(TAG, request);
                                startIntentSenderForResult(
                                        /* activity= */ window,
                                        /* intent= */ response.getPendingIntent().getIntentSender(),
                                        REQUEST_CODE_DIGITAL_CREDENTIALS,
                                        /* fillInIntent= */ null,
                                        /* flagsMask= */ 0,
                                        /* flagsValues= */ 0,
                                        /* extraFlags= */ 0,
                                        /* options= */ null);
                            } catch (SendIntentException e) {
                                Log.e(TAG, "Sending an intent for sender failed");
                                result.reject(e);
                            }
                        });

        return result;
    }

    public Promise<String> create(Activity window, String origin, String request) {
        DigitalCredentialsCreationDelegate delegateImpl =
                ServiceLoaderUtil.maybeCreate(DigitalCredentialsCreationDelegate.class);
        if (delegateImpl != null) {
            return delegateImpl.create(window, origin, request);
        }
        return Promise.rejected();
    }
}
