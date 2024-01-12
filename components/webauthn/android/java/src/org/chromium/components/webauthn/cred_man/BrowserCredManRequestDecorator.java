// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.CreateCredentialRequest.Builder;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.RequiresApi;

public class BrowserCredManRequestDecorator implements CredManRequestDecorator {
    private static BrowserCredManRequestDecorator sInstance;

    public static BrowserCredManRequestDecorator getInstance() {
        if (sInstance == null) {
            sInstance = new BrowserCredManRequestDecorator();
        }
        return sInstance;
    }

    @Override
    public void updateCreateCredentialRequestBundle(
            Bundle input, CredManCreateCredentialRequestHelper helper) {}

    @Override
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void updateCreateCredentialRequestBuilder(
            Builder builder, CredManCreateCredentialRequestHelper helper) {
        builder.setOrigin(helper.getOrigin());
    }

    @Override
    public void updateGetCredentialRequestBundle(
            Bundle bundle, CredManGetCredentialRequestHelper helper) {}

    @Override
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void updateGetCredentialRequestBuilder(
            android.credentials.GetCredentialRequest.Builder builder,
            CredManGetCredentialRequestHelper helper) {
        builder.setOrigin(helper.getOrigin());
    }

    @Override
    public void updatePublicKeyCredentialOptionBundle(
            Bundle bundle, CredManGetCredentialRequestHelper helper) {}

    @Override
    public void updatePublicKeyCredentialOptionBuilder(
            android.credentials.CredentialOption.Builder builder,
            CredManGetCredentialRequestHelper helper) {}

    @Override
    public void updatePasswordCredentialOptionBundle(
            Bundle bundle, CredManGetCredentialRequestHelper helper) {}

    @Override
    public void updatePasswordCredentialOptionBuilder(
            android.credentials.CredentialOption.Builder builder,
            CredManGetCredentialRequestHelper helper) {}

    private BrowserCredManRequestDecorator() {}
}
