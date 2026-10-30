// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.content.Context;
import android.credentials.CreateCredentialException;
import android.credentials.CreateCredentialRequest;
import android.credentials.CreateCredentialResponse;
import android.credentials.CredentialManager;
import android.credentials.GetCredentialException;
import android.credentials.GetCredentialRequest;
import android.credentials.GetCredentialResponse;
import android.credentials.PrepareGetCredentialResponse;
import android.os.CancellationSignal;
import android.os.OutcomeReceiver;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import java.util.concurrent.Executor;

/** Shadow of the CredentialManager object. */
@Implements(value = CredentialManager.class)
public class ShadowCredentialManager {
    private CreateCredentialRequest mCreateCredentialRequest;
    private OutcomeReceiver<CreateCredentialResponse, CreateCredentialException>
            mCreateCredentialCallback;
    private GetCredentialRequest mGetCredentialRequest;
    private OutcomeReceiver<GetCredentialResponse, GetCredentialException> mGetCredentialCallback;
    private OutcomeReceiver<PrepareGetCredentialResponse, GetCredentialException>
            mPrepareGetCredentialCallback;
    private RuntimeException mExceptionToThrow;

    @Implementation
    protected void __constructor__() {}

    @Implementation
    protected void createCredential(
            Context context,
            CreateCredentialRequest request,
            CancellationSignal cancellationSignal,
            Executor executor,
            OutcomeReceiver<CreateCredentialResponse, CreateCredentialException> callback) {
        throwIfRequested();
        mCreateCredentialCallback = callback;
        mCreateCredentialRequest = request;
    }

    @Implementation
    protected void getCredential(
            Context context,
            GetCredentialRequest request,
            CancellationSignal cancellationSignal,
            Executor executor,
            OutcomeReceiver<GetCredentialResponse, GetCredentialException> callback) {
        throwIfRequested();
        mGetCredentialRequest = request;
        mGetCredentialCallback = callback;
    }

    @Implementation
    protected void prepareGetCredential(
            GetCredentialRequest request,
            CancellationSignal cancellationSignal,
            Executor executor,
            OutcomeReceiver<PrepareGetCredentialResponse, GetCredentialException> callback) {
        throwIfRequested();
        mGetCredentialRequest = request;
        mPrepareGetCredentialCallback = callback;
    }

    /** Makes every subsequent CredentialManager call throw {@code exception} synchronously. */
    protected void setExceptionToThrow(RuntimeException exception) {
        mExceptionToThrow = exception;
    }

    private void throwIfRequested() {
        if (mExceptionToThrow != null) {
            throw mExceptionToThrow;
        }
    }

    protected CreateCredentialRequest getCreateCredentialRequest() {
        return mCreateCredentialRequest;
    }

    protected GetCredentialRequest getGetCredentialRequest() {
        return mGetCredentialRequest;
    }

    protected OutcomeReceiver<CreateCredentialResponse, CreateCredentialException>
            getCreateCredentialCallback() {
        return mCreateCredentialCallback;
    }

    protected OutcomeReceiver<GetCredentialResponse, GetCredentialException>
            getGetCredentialCallback() {
        return mGetCredentialCallback;
    }

    protected OutcomeReceiver<PrepareGetCredentialResponse, GetCredentialException>
            getPrepareGetCredentialCallback() {
        return mPrepareGetCredentialCallback;
    }
}
