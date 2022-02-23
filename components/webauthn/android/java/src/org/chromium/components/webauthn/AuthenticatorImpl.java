// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.util.Pair;

import androidx.annotation.RequiresApi;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.util.LinkedList;
import java.util.Queue;

/**
 * Android implementation of the authenticator.mojom interface.
 */
public final class AuthenticatorImpl implements Authenticator {
    private final WebAuthenticationDelegate.IntentSender mIntentSender;
    private final RenderFrameHost mRenderFrameHost;
    private final @WebAuthenticationDelegate.Support int mSupportLevel;

    private static final String GMSCORE_PACKAGE_NAME = "com.google.android.gms";

    /** Ensures only one request is processed at a time. */
    private boolean mIsOperationPending;

    /**
     * The origin of the request. This may be overridden by an internal request from the browser
     * process.
     */
    private Origin mOrigin;

    /** The payment information to be added to the "clientDataJson". */
    private PaymentOptions mPayment;

    private MakeCredential_Response mMakeCredentialCallback;
    private GetAssertion_Response mGetAssertionCallback;
    // A queue is used to store pending IsUserVerifyingPlatformAuthenticatorAvailable request
    // callbacks when there are multiple requests pending on the result from GMSCore. Noted that
    // the callbacks may not be invoked in the same order as the pending requests, which in this
    // situation does not matter because all pending requests will return the same value.
    private Queue<org.chromium.mojo.bindings.Callbacks.Callback1<Boolean>>
            mIsUserVerifyingPlatformAuthenticatorAvailableCallbackQueue = new LinkedList<>();

    /**
     * Builds the Authenticator service implementation.
     *
     * @param renderFrameHost The host of the frame that has invoked the API.
     * @param intentSender If present then an interface that will be used to start {@link Intent}s
     *         from Play Services.
     * @param supportLevel Whether this code should use the privileged or non-privileged Play
     *         Services API. (Note that a value of `NONE` is not allowed.)
     */
    public AuthenticatorImpl(WebAuthenticationDelegate.IntentSender intentSender,
            RenderFrameHost renderFrameHost, @WebAuthenticationDelegate.Support int supportLevel) {
        assert renderFrameHost != null;
        assert supportLevel != WebAuthenticationDelegate.Support.NONE;

        if (intentSender != null) {
            mIntentSender = intentSender;
        } else {
            mIntentSender = new WindowIntentSender(renderFrameHost);
        }

        mRenderFrameHost = renderFrameHost;
        mSupportLevel = supportLevel;
        mOrigin = mRenderFrameHost.getLastCommittedOrigin();
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process. Since the request is from the browser process, the
     * Relying Party ID may not correspond with the origin of the renderer.
     */
    public void setEffectiveOrigin(Origin origin) {
        mOrigin = origin;
    }

    /**
     * @param payment The payment information to be added to the "clientDataJson". Should be used
     * only if the user has confirmed the payment information that was displayed to the user.
     */
    public void setPaymentOptions(PaymentOptions payment) {
        mPayment = payment;
    }

    @Override
    public void makeCredential(
            PublicKeyCredentialCreationOptions options, MakeCredential_Response callback) {
        if (mIsOperationPending) {
            callback.call(AuthenticatorStatus.PENDING_REQUEST, null, null);
            return;
        }

        mMakeCredentialCallback = callback;
        mIsOperationPending = true;
        Context context = ContextUtils.getApplicationContext();
        if (PackageUtils.getPackageVersion(context, GMSCORE_PACKAGE_NAME)
                < Fido2ApiHandler.GMSCORE_MIN_VERSION) {
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        Fido2ApiHandler.getInstance().makeCredential(options, mIntentSender, mRenderFrameHost,
                mOrigin, mSupportLevel,
                (status, response)
                        -> onRegisterResponse(status, response),
                status -> onError(status));
    }

    @Override
    public void getAssertion(
            PublicKeyCredentialRequestOptions options, GetAssertion_Response callback) {
        if (mIsOperationPending) {
            callback.call(AuthenticatorStatus.PENDING_REQUEST, null, null);
            return;
        }

        mGetAssertionCallback = callback;
        mIsOperationPending = true;
        Context context = ContextUtils.getApplicationContext();

        if (PackageUtils.getPackageVersion(context, GMSCORE_PACKAGE_NAME)
                < Fido2ApiHandler.GMSCORE_MIN_VERSION) {
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        Fido2ApiHandler.getInstance().getAssertion(options, mIntentSender, mRenderFrameHost,
                mOrigin, mPayment, mSupportLevel,
                (status, response) -> onSignResponse(status, response), status -> onError(status));
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.N)
    public void isUserVerifyingPlatformAuthenticatorAvailable(
            final IsUserVerifyingPlatformAuthenticatorAvailable_Response callback) {
        IsUserVerifyingPlatformAuthenticatorAvailable_Response decoratedCallback = (isUvpaa) -> {
            RecordHistogram.recordBooleanHistogram(
                    "WebAuthentication.IsUVPlatformAuthenticatorAvailable2", isUvpaa);
            callback.call(isUvpaa);
        };

        Context context = ContextUtils.getApplicationContext();
        // ChromeActivity could be null.
        if (context == null) {
            decoratedCallback.call(false);
            return;
        }

        if (PackageUtils.getPackageVersion(context, GMSCORE_PACKAGE_NAME)
                < Fido2ApiHandler.GMSCORE_MIN_VERSION) {
            decoratedCallback.call(false);
            return;
        }

        mIsUserVerifyingPlatformAuthenticatorAvailableCallbackQueue.add(decoratedCallback);
        Fido2ApiHandler.getInstance().isUserVerifyingPlatformAuthenticatorAvailable(mIntentSender,
                mRenderFrameHost, mSupportLevel,
                isUvpaa -> onIsUserVerifyingPlatformAuthenticatorAvailableResponse(isUvpaa));
    }

    @Override
    public void cancel() {
        // Not implemented, ignored because request sent to gmscore fido cannot be cancelled.
        return;
    }

    /**
     * Callbacks for receiving responses from the internal handlers.
     */
    public void onRegisterResponse(Integer status, MakeCredentialAuthenticatorResponse response) {
        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending) return;

        assert mMakeCredentialCallback != null;
        assert status == AuthenticatorStatus.SUCCESS;
        mMakeCredentialCallback.call(status, response, null);
        close();
    }

    public void onSignResponse(Integer status, GetAssertionAuthenticatorResponse response) {
        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending) return;

        assert mGetAssertionCallback != null;
        mGetAssertionCallback.call(status, response, null);
        close();
    }

    public void onIsUserVerifyingPlatformAuthenticatorAvailableResponse(boolean isUVPAA) {
        assert !mIsUserVerifyingPlatformAuthenticatorAvailableCallbackQueue.isEmpty();
        mIsUserVerifyingPlatformAuthenticatorAvailableCallbackQueue.poll().call(isUVPAA);
    }

    public void onError(Integer status) {
        // In case mojo pipe is closed due to the page begin destroyed while waiting for response.
        if (!mIsOperationPending) return;

        assert ((mMakeCredentialCallback != null && mGetAssertionCallback == null)
                || (mMakeCredentialCallback == null && mGetAssertionCallback != null));
        assert status != AuthenticatorStatus.ERROR_WITH_DOM_EXCEPTION_DETAILS;
        if (mMakeCredentialCallback != null) {
            mMakeCredentialCallback.call(status, null, null);
        } else if (mGetAssertionCallback != null) {
            mGetAssertionCallback.call(status, null, null);
        }
        close();
    }

    @Override
    public void close() {
        mIsOperationPending = false;
        mMakeCredentialCallback = null;
        mGetAssertionCallback = null;
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    /**
     * Provides a default implementation of {@link IntentSender} when none is provided.
     */
    public static class WindowIntentSender implements WebAuthenticationDelegate.IntentSender {
        private final WindowAndroid mWindow;

        WindowIntentSender(RenderFrameHost renderFrameHost) {
            mWindow = WebContentsStatics.fromRenderFrameHost(renderFrameHost)
                              .getTopLevelNativeWindow();
        }

        @Override
        public boolean showIntent(PendingIntent intent, Callback<Pair<Integer, Intent>> callback) {
            return mWindow != null && mWindow.getActivity().get() != null
                    && mWindow.showCancelableIntent(intent, new CallbackWrapper(callback), null)
                    != WindowAndroid.START_INTENT_FAILURE;
        }

        private static class CallbackWrapper implements WindowAndroid.IntentCallback {
            private final Callback<Pair<Integer, Intent>> mCallback;

            CallbackWrapper(Callback<Pair<Integer, Intent>> callback) {
                mCallback = callback;
            }

            @Override
            public void onIntentCompleted(int resultCode, Intent data) {
                mCallback.onResult(new Pair(resultCode, data));
            }
        }
    }
}
