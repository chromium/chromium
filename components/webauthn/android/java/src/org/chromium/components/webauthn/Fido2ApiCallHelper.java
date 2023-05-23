// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.os.Parcel;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.Task;

import org.chromium.base.ContextUtils;

import java.util.List;

/**
 * Provides helper methods to wrap Fido2ApiCall invocations.
 * This class is useful to override GMS Core API interactions from Fido2CredentialRequest in tests.
 */
public class Fido2ApiCallHelper {
    private static Fido2ApiCallHelper sInstance;

    @VisibleForTesting
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

    public void invokeFido2GetCredentials(String relyingPartyId,
            OnSuccessListener<List<WebAuthnCredentialDetails>> successCallback,
            OnFailureListener failureCallback) {
        Fido2ApiCall call = new Fido2ApiCall(ContextUtils.getApplicationContext());
        Parcel args = call.start();
        Fido2ApiCall.WebAuthnCredentialDetailsListResult result =
                new Fido2ApiCall.WebAuthnCredentialDetailsListResult();
        args.writeStrongBinder(result);
        args.writeString(relyingPartyId);

        Task<List<WebAuthnCredentialDetails>> task =
                call.run(Fido2ApiCall.METHOD_BROWSER_GETCREDENTIALS,
                        Fido2ApiCall.TRANSACTION_GETCREDENTIALS, args, result);
        task.addOnSuccessListener(successCallback);
        task.addOnFailureListener(failureCallback);
    }
}
