// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import static org.chromium.components.webauthn.cred_man.CredManHelper.CRED_MAN_PREFIX;

import android.content.ComponentName;
import android.credentials.CreateCredentialRequest;
import android.credentials.CredentialOption;
import android.credentials.GetCredentialRequest.Builder;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;

import androidx.annotation.RequiresApi;

import org.chromium.components.webauthn.GpmBrowserOptionsHelper;

import java.util.Set;

/**
 * This decorator is responsible for decorating the CredMan requests with Google Password Manager
 * and Chrome specific values. The values may be used to theme CredMan UI with Google Password
 * Manager.
 */
public class GpmCredManRequestDecorator implements CredManRequestDecorator {
    private static final ComponentName GPM_COMPONENT_NAME =
            ComponentName.createRelative(
                    "com.google.android.gms",
                    ".auth.api.credentials.credman.service.PasswordAndPasskeyService");
    private static final String IGNORE_GPM_KEY = "com.android.chrome.GPM_IGNORE";

    private static final String PASSWORDS_ONLY_FOR_THE_CHANNEL =
            "com.android.chrome.PASSWORDS_ONLY_FOR_THE_CHANNEL";
    private static final String PASSWORDS_WITH_NO_USERNAME_INCLUDED =
            "com.android.chrome.PASSWORDS_WITH_NO_USERNAME_INCLUDED";

    private static GpmCredManRequestDecorator sInstance;

    public static GpmCredManRequestDecorator getInstance() {
        if (sInstance == null) {
            sInstance = new GpmCredManRequestDecorator();
        }
        return sInstance;
    }

    @Override
    public void updateCreateCredentialRequestBundle(
            Bundle input, CredManCreateCredentialRequestHelper helper) {
        // displayInfo bundle is required to theme the CredMan UI with Google Password Manager.
        final Bundle displayInfoBundle = new Bundle();
        displayInfoBundle.putCharSequence(
                CRED_MAN_PREFIX + "BUNDLE_KEY_USER_ID",
                Base64.encodeToString(
                        helper.getUserId(), Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP));
        displayInfoBundle.putString(
                CRED_MAN_PREFIX + "BUNDLE_KEY_DEFAULT_PROVIDER",
                GPM_COMPONENT_NAME.flattenToString());
        input.putBundle(CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_DISPLAY_INFO", displayInfoBundle);

        // Google Password Manager only: Specify the channel to save credential to the correct
        // account. When multiple Google accounts are present on the device, this will prioritize
        // the current account in Chrome.
        GpmBrowserOptionsHelper.addChannelExtraToOptions(input);
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void updateCreateCredentialRequestBuilder(
            CreateCredentialRequest.Builder builder, CredManCreateCredentialRequestHelper helper) {
        builder.setOrigin(helper.getOrigin());
    }

    @Override
    public void updateGetCredentialRequestBundle(
            Bundle getCredentialRequestBundle, CredManGetCredentialRequestHelper helper) {
        if (!helper.getIgnoreGpm()) {
            // Theme the CredMan UI with Google Password Manager:
            getCredentialRequestBundle.putParcelable(
                    CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_UI_BRANDING_COMPONENT_NAME",
                    GPM_COMPONENT_NAME);
        }
        // The CredMan UI for the case where there aren't any credentials isn't suitable for the
        // modal case. This bundle key requests that the request fail immediately if there aren't
        // any credentials. It'll fail with a `CRED_MAN_EXCEPTION_GET_CREDENTIAL_TYPE_NO_CREDENTIAL`
        // error which is handled by calling Play Services to render the error.
        getCredentialRequestBundle.putBoolean(
                CRED_MAN_PREFIX + "BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS",
                helper.getPreferImmediatelyAvailable() && helper.getPlayServicesAvailable());
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void updateGetCredentialRequestBuilder(
            Builder builder, CredManGetCredentialRequestHelper helper) {
        builder.setOrigin(helper.getOrigin());
    }

    @Override
    public void updatePublicKeyCredentialOptionBundle(
            Bundle publicKeyCredentialOptionBundle, CredManGetCredentialRequestHelper helper) {
        // The values below are specific to Google Password Manager.
        // Use the channel info to prioritize the credentials for the current account in Chrome.
        GpmBrowserOptionsHelper.addChannelExtraToOptions(publicKeyCredentialOptionBundle);
        // Specify if the tab is in incognito mode for user privacy.
        GpmBrowserOptionsHelper.addIncognitoExtraToOptions(
                publicKeyCredentialOptionBundle, helper.getRenderFrameHost());
        // Do not include any passkeys from GPM if `helper.getIgnoreGpm()` is true.
        publicKeyCredentialOptionBundle.putBoolean(IGNORE_GPM_KEY, helper.getIgnoreGpm());
    }

    @Override
    public void updatePublicKeyCredentialOptionBuilder(
            CredentialOption.Builder builder, CredManGetCredentialRequestHelper helper) {}

    @Override
    public void updatePasswordCredentialOptionBundle(
            Bundle passwordCredentialOptionBundle, CredManGetCredentialRequestHelper helper) {
        // The values below are specific to Google Password Manager.
        // Specify the channel so that GPM can return passwords only for that channel.
        GpmBrowserOptionsHelper.addChannelExtraToOptions(passwordCredentialOptionBundle);
        // Specify if the tab is in incognito mode for user privacy.
        GpmBrowserOptionsHelper.addIncognitoExtraToOptions(
                passwordCredentialOptionBundle, helper.getRenderFrameHost());
        // Requests passwords only for the current Chrome channel.
        passwordCredentialOptionBundle.putBoolean(PASSWORDS_ONLY_FOR_THE_CHANNEL, true);
        // If there are passwords with empty usernames, also return them in the response.
        passwordCredentialOptionBundle.putBoolean(PASSWORDS_WITH_NO_USERNAME_INCLUDED, true);
        // Do not include any passwords from GPM if `helper.getIgnoreGpm()` is true.
        passwordCredentialOptionBundle.putBoolean(IGNORE_GPM_KEY, helper.getIgnoreGpm());
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void updatePasswordCredentialOptionBuilder(
            CredentialOption.Builder builder, CredManGetCredentialRequestHelper helper) {
        builder.setAllowedProviders(Set.of(GPM_COMPONENT_NAME));
    }

    private GpmCredManRequestDecorator() {}
}
