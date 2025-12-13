// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.CredentialInfo;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.GetAssertionResponse;
import org.chromium.blink.mojom.GetCredentialResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Wrapper to manage the set of responses for a WebAuthn operation. */
@NullMarked
public class WebauthnRequestResponse {
    // Note: This is only used for `MakeCredential` and `Report` operations.
    // `GetCredential` uses `GetCredentialResponse.getAssertionResponse.status` instead.
    private int mAuthenticatorStatus;

    private @Nullable MakeCredentialAuthenticatorResponse mMakeCredentialResponse;

    private @Nullable GetCredentialResponse mGetCredentialResponse;
    private @Nullable RequestMetrics mRequestMetrics;

    private WebauthnRequestResponse() {}

    public static WebauthnRequestResponse forSuccessfulMakeCredential(
            MakeCredentialAuthenticatorResponse makeCredentialResponse, RequestMetrics result) {
        WebauthnRequestResponse response = new WebauthnRequestResponse();
        response.mAuthenticatorStatus = AuthenticatorStatus.SUCCESS;
        response.mMakeCredentialResponse = makeCredentialResponse;
        response.mRequestMetrics = result;
        return response;
    }

    public static WebauthnRequestResponse forFailedMakeCredential(
            int makeCredentialStatus, RequestMetrics result) {
        WebauthnRequestResponse response = new WebauthnRequestResponse();
        response.mAuthenticatorStatus = makeCredentialStatus;
        response.mRequestMetrics = result;
        return response;
    }

    public static WebauthnRequestResponse forSuccessfulGetCredential(
            GetCredentialResponse getCredentialResponse, RequestMetrics result) {
        WebauthnRequestResponse response = new WebauthnRequestResponse();
        response.mGetCredentialResponse = getCredentialResponse;
        response.mRequestMetrics = result;
        return response;
    }

    public static WebauthnRequestResponse forSuccessfulGetAssertion(
            GetAssertionAuthenticatorResponse getAssertionAuthenticatorResponse,
            RequestMetrics result) {
        GetAssertionResponse getAssertionResponse = new GetAssertionResponse();
        getAssertionResponse.status = AuthenticatorStatus.SUCCESS;
        getAssertionResponse.credential = getAssertionAuthenticatorResponse;

        GetCredentialResponse getCredentialResponse = new GetCredentialResponse();
        getCredentialResponse.setGetAssertionResponse(getAssertionResponse);

        WebauthnRequestResponse response = new WebauthnRequestResponse();
        response.mGetCredentialResponse = getCredentialResponse;
        response.mRequestMetrics = result;
        return response;
    }

    public static WebauthnRequestResponse forSuccessfulPassword(CredentialInfo credentialInfo) {
        WebauthnRequestResponse response = new WebauthnRequestResponse();
        response.mGetCredentialResponse = new GetCredentialResponse();
        response.mGetCredentialResponse.setPasswordResponse(credentialInfo);
        response.mRequestMetrics = new RequestMetrics.Builder().build();
        return response;
    }

    public static WebauthnRequestResponse forFailedGetCredential(
            Integer getCredentialStatus, RequestMetrics requestMetrics) {
        GetAssertionResponse assertionResponse = new GetAssertionResponse();
        assertionResponse.status = getCredentialStatus;

        GetCredentialResponse getCredentialResponse = new GetCredentialResponse();
        getCredentialResponse.setGetAssertionResponse(assertionResponse);

        WebauthnRequestResponse response = new WebauthnRequestResponse();
        response.mGetCredentialResponse = getCredentialResponse;
        response.mRequestMetrics = requestMetrics;

        return response;
    }

    public static WebauthnRequestResponse forReport(int status) {
        WebauthnRequestResponse response = new WebauthnRequestResponse();
        response.mAuthenticatorStatus = status;
        return response;
    }

    public int getAuthenticatorStatus() {
        return mAuthenticatorStatus;
    }

    public @Nullable MakeCredentialAuthenticatorResponse getMakeCredentialResponse() {
        return mMakeCredentialResponse;
    }

    public @Nullable GetCredentialResponse getGetCredentialResponse() {
        return mGetCredentialResponse;
    }

    public @Nullable RequestMetrics getRequestMetrics() {
        return mRequestMetrics;
    }

    public @Nullable @MakeCredentialOutcome Integer getMakeCredentialOutcomeMetricValue() {
        return mRequestMetrics != null ? mRequestMetrics.getMakeCredentialOutcome() : null;
    }

    public @Nullable @GetAssertionOutcome Integer getGetAssertionOutcomeMetricValue() {
        return mRequestMetrics != null ? mRequestMetrics.getGetAssertionOutcome() : null;
    }
}
