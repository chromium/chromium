// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.content.ComponentName;
import android.credentials.CreateCredentialRequest;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;

import androidx.annotation.RequiresApi;

import org.chromium.components.version_info.VersionInfo;

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
    private static final String CHANNEL_KEY = "com.android.chrome.CHANNEL";

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
                CredManHelper.CRED_MAN_PREFIX + "BUNDLE_KEY_USER_ID",
                Base64.encodeToString(
                        helper.getUserId(), Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP));
        displayInfoBundle.putString(
                CredManHelper.CRED_MAN_PREFIX + "BUNDLE_KEY_DEFAULT_PROVIDER",
                GPM_COMPONENT_NAME.flattenToString());
        input.putBundle(
                CredManHelper.CRED_MAN_PREFIX + "BUNDLE_KEY_REQUEST_DISPLAY_INFO",
                displayInfoBundle);

        // Google Password Manager only: Specify the channel to save credential to the correct
        // account. When multiple Google accounts are present on the device, this will prioritize
        // the current account in Chrome.
        input.putString(CHANNEL_KEY, getChannel());
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void updateCreateCredentialRequestBuilder(
            CreateCredentialRequest.Builder builder, CredManCreateCredentialRequestHelper helper) {
        builder.setOrigin(helper.getOrigin());
    }

    protected static final String getChannel() {
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

    private GpmCredManRequestDecorator() {}
}
