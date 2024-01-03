// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.CreateCredentialRequest.Builder;
import android.os.Bundle;

public class AppCredManRequestDecorator implements CredManRequestDecorator {
    private static AppCredManRequestDecorator sInstance;

    public static AppCredManRequestDecorator getInstance() {
        if (sInstance == null) {
            sInstance = new AppCredManRequestDecorator();
        }
        return sInstance;
    }

    @Override
    public void updateCreateCredentialRequestBundle(
            Bundle input, CredManCreateCredentialRequestHelper helper) {}

    @Override
    public void updateCreateCredentialRequestBuilder(
            Builder builder, CredManCreateCredentialRequestHelper helper) {}

    @Override
    public void updateGetCredentialRequestBundle(
            Bundle bundle, CredManGetCredentialRequestHelper helper) {}

    @Override
    public void updateGetCredentialRequestBuilder(
            android.credentials.GetCredentialRequest.Builder builder,
            CredManGetCredentialRequestHelper helper) {}

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

    private AppCredManRequestDecorator() {}
}
