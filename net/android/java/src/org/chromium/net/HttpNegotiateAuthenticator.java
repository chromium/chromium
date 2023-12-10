// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.Manifest;
import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Process;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.io.IOException;

/**
 * Class to get Auth Tokens for HTTP Negotiate authentication (typically used for Kerberos) An
 * instance of this class is created for each separate negotiation.
 *
 * Please keep the documentation from the chromium.org page (https://goo.gl/46hmKx) in sync.
 * ================================================================================================
 * In addition to the error codes that can be forwarded from the authenticator app, the following
 * errors can be displayed when trying to authenticate a request:
 *
 *  - ERR_UNEXPECTED: An unexpected error happened and the request has been terminated.
 *
 *  - ERR_MISSING_AUTH_CREDENTIALS: The account information is not usable. It can be raised for
 *    example if the user did not log in to the authenticator app and no eligible account is found,
 *    if the account information can't be obtained because the current app does not have the
 *    required permissions, or if there is more than one eligible account and we can't obtain a
 *    selection from the user.
 *
 *  - ERR_MISCONFIGURED_AUTH_ENVIRONMENT: The authentication can't be completed because of some
 *    issues in the configuration of the app. Some permissions may be missing.
 *
 * Please search for the "cr_net_auth" tag in logcat to have more information about the cause of
 * these errors.
 * ================================================================================================
 */
@JNINamespace("net::android")
public class HttpNegotiateAuthenticator {
    private static final String TAG = "net_auth";
    private Bundle mSpnegoContext;
    private final String mAccountType;

    /**
     * Structure designed to hold the data related to a specific request across the various
     * callbacks needed to complete it.
     */
    static class RequestData {
        /** Native object to post the result to. */
        public long nativeResultObject;

        /** Reference to the account manager to use for the various requests. */
        public AccountManager accountManager;

        /** Authenticator-specific options for the request, used for AccountManager#getAuthToken. */
        public Bundle options;

        /** Desired token type, used for AccountManager#getAuthToken. */
        public String authTokenType;

        /** Account to fetch an auth token for. */
        public Account account;
    }

    /**
     * Expects to receive a single account as result, and uses that account to request a token
     * from the {@link AccountManager} provided via the {@link RequestData}
     */
    @VisibleForTesting
    class GetAccountsCallback implements AccountManagerCallback<Account[]> {
        private final RequestData mRequestData;

        public GetAccountsCallback(RequestData requestData) {
            mRequestData = requestData;
        }

        @Override
        public void run(AccountManagerFuture<Account[]> future) {
            Account[] accounts;
            try {
                accounts = future.getResult();
            } catch (OperationCanceledException | AuthenticatorException | IOException e) {
                Log.w(TAG, "ERR_UNEXPECTED: Error while attempting to retrieve accounts.", e);
                HttpNegotiateAuthenticatorJni.get()
                        .setResult(
                                mRequestData.nativeResultObject,
                                HttpNegotiateAuthenticator.this,
                                NetError.ERR_UNEXPECTED,
                                null);
                return;
            }

            if (accounts.length == 0) {
                Log.w(
                        TAG,
                        "ERR_MISSING_AUTH_CREDENTIALS: No account provided for the kerberos "
                                + "authentication. Please verify the configuration policies and "
                                + "that the CONTACTS runtime permission is granted. ");
                HttpNegotiateAuthenticatorJni.get()
                        .setResult(
                                mRequestData.nativeResultObject,
                                HttpNegotiateAuthenticator.this,
                                NetError.ERR_MISSING_AUTH_CREDENTIALS,
                                null);
                return;
            }

            if (accounts.length > 1) {
                Log.w(
                        TAG,
                        "ERR_MISSING_AUTH_CREDENTIALS: Found %d accounts eligible for the "
                                + "kerberos authentication. Please fix the configuration by "
                                + "providing a single account.",
                        accounts.length);
                HttpNegotiateAuthenticatorJni.get()
                        .setResult(
                                mRequestData.nativeResultObject,
                                HttpNegotiateAuthenticator.this,
                                NetError.ERR_MISSING_AUTH_CREDENTIALS,
                                null);
                return;
            }

            if (lacksPermission(
                    ContextUtils.getApplicationContext(),
                    "android.permission.USE_CREDENTIALS",
                    true)) {
                // Protecting the AccountManager#getAuthToken call.
                // API  < 23 Requires the USE_CREDENTIALS permission or throws an exception.
                // API >= 23 USE_CREDENTIALS permission is removed
                Log.e(
                        TAG,
                        "ERR_MISCONFIGURED_AUTH_ENVIRONMENT: USE_CREDENTIALS permission not "
                                + "granted. Aborting authentication.");
                HttpNegotiateAuthenticatorJni.get()
                        .setResult(
                                mRequestData.nativeResultObject,
                                HttpNegotiateAuthenticator.this,
                                NetError.ERR_MISCONFIGURED_AUTH_ENVIRONMENT,
                                null);
                return;
            }
            mRequestData.account = accounts[0];
            mRequestData.accountManager.getAuthToken(
                    mRequestData.account,
                    mRequestData.authTokenType,
                    mRequestData.options,
                    /* notifyAuthFailure= */ true,
                    new GetTokenCallback(mRequestData),
                    new Handler(ThreadUtils.getUiThreadLooper()));
        }
    }

    @VisibleForTesting
    class GetTokenCallback implements AccountManagerCallback<Bundle> {
        private final RequestData mRequestData;

        public GetTokenCallback(RequestData requestData) {
            mRequestData = requestData;
        }

        @Override
        public void run(AccountManagerFuture<Bundle> future) {
            Bundle result;
            try {
                result = future.getResult();
            } catch (OperationCanceledException | AuthenticatorException | IOException e) {
                Log.w(TAG, "ERR_UNEXPECTED: Error while attempting to obtain a token.", e);
                HttpNegotiateAuthenticatorJni.get()
                        .setResult(
                                mRequestData.nativeResultObject,
                                HttpNegotiateAuthenticator.this,
                                NetError.ERR_UNEXPECTED,
                                null);
                return;
            }

            if (result.containsKey(AccountManager.KEY_INTENT)) {
                final Context appContext = ContextUtils.getApplicationContext();

                // We wait for a broadcast that should be sent once the user is done interacting
                // with the notification
                // TODO(dgn) We currently hang around if the notification is swiped away, until
                // a LOGIN_ACCOUNTS_CHANGED_ACTION filter is received. It might be for something
                // unrelated then we would wait again here. Maybe we should limit the number of
                // retries in some way?
                BroadcastReceiver broadcastReceiver =
                        new BroadcastReceiver() {

                            @Override
                            public void onReceive(Context context, Intent intent) {
                                appContext.unregisterReceiver(this);
                                mRequestData.accountManager.getAuthToken(
                                        mRequestData.account,
                                        mRequestData.authTokenType,
                                        mRequestData.options,
                                        /* notifyAuthFailure= */ true,
                                        new GetTokenCallback(mRequestData),
                                        null);
                            }
                        };
                ContextUtils.registerProtectedBroadcastReceiver(
                        appContext,
                        broadcastReceiver,
                        new IntentFilter(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION));
            } else {
                processResult(result, mRequestData);
            }
        }
    }

    protected HttpNegotiateAuthenticator(String accountType) {
        assert !android.text.TextUtils.isEmpty(accountType);
        mAccountType = accountType;
    }

    /**
     * @param accountType The Android account type to use.
     */
    @VisibleForTesting
    @CalledByNative
    static HttpNegotiateAuthenticator create(String accountType) {
        return new HttpNegotiateAuthenticator(accountType);
    }

    /**
     * @param nativeResultObject The C++ object used to return the result. For correct C++ memory
     *            management we must call HttpNegotiateAuthenticatorJni.get().setResult precisely
     * once with this object.
     * @param principal The principal (must be host based).
     * @param authToken The incoming auth token.
     * @param canDelegate True if we can delegate.
     */
    @VisibleForTesting
    @CalledByNative
    void getNextAuthToken(
            final long nativeResultObject,
            final String principal,
            String authToken,
            boolean canDelegate) {
        assert principal != null;

        Context applicationContext = ContextUtils.getApplicationContext();
        RequestData requestData = new RequestData();
        requestData.authTokenType = HttpNegotiateConstants.SPNEGO_TOKEN_TYPE_BASE + principal;
        requestData.accountManager = AccountManager.get(applicationContext);
        requestData.nativeResultObject = nativeResultObject;
        String features[] = {HttpNegotiateConstants.SPNEGO_FEATURE};

        requestData.options = new Bundle();
        if (authToken != null) {
            requestData.options.putString(
                    HttpNegotiateConstants.KEY_INCOMING_AUTH_TOKEN, authToken);
        }
        if (mSpnegoContext != null) {
            requestData.options.putBundle(
                    HttpNegotiateConstants.KEY_SPNEGO_CONTEXT, mSpnegoContext);
        }
        requestData.options.putBoolean(HttpNegotiateConstants.KEY_CAN_DELEGATE, canDelegate);

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            requestTokenWithoutActivity(applicationContext, requestData, features);
        } else {
            requestTokenWithActivity(applicationContext, activity, requestData, features);
        }
    }

    /**
     * Process a result bundle from a completed token request, communicating its content back to
     * the native code.
     */
    private void processResult(Bundle result, RequestData requestData) {
        mSpnegoContext = result.getBundle(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT);
        @NetError int status;
        switch (result.getInt(
                HttpNegotiateConstants.KEY_SPNEGO_RESULT, HttpNegotiateConstants.ERR_UNEXPECTED)) {
            case HttpNegotiateConstants.OK:
                status = NetError.OK;
                break;
            case HttpNegotiateConstants.ERR_UNEXPECTED:
                status = NetError.ERR_UNEXPECTED;
                break;
            case HttpNegotiateConstants.ERR_ABORTED:
                status = NetError.ERR_ABORTED;
                break;
            case HttpNegotiateConstants.ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS:
                status = NetError.ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
                break;
            case HttpNegotiateConstants.ERR_INVALID_RESPONSE:
                status = NetError.ERR_INVALID_RESPONSE;
                break;
            case HttpNegotiateConstants.ERR_INVALID_AUTH_CREDENTIALS:
                status = NetError.ERR_INVALID_AUTH_CREDENTIALS;
                break;
            case HttpNegotiateConstants.ERR_UNSUPPORTED_AUTH_SCHEME:
                status = NetError.ERR_UNSUPPORTED_AUTH_SCHEME;
                break;
            case HttpNegotiateConstants.ERR_MISSING_AUTH_CREDENTIALS:
                status = NetError.ERR_MISSING_AUTH_CREDENTIALS;
                break;
            case HttpNegotiateConstants.ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS:
                status = NetError.ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
                break;
            case HttpNegotiateConstants.ERR_MALFORMED_IDENTITY:
                status = NetError.ERR_MALFORMED_IDENTITY;
                break;
            default:
                status = NetError.ERR_UNEXPECTED;
        }
        HttpNegotiateAuthenticatorJni.get()
                .setResult(
                        requestData.nativeResultObject,
                        HttpNegotiateAuthenticator.this,
                        status,
                        result.getString(AccountManager.KEY_AUTHTOKEN));
    }

    /**
     * Requests an authentication token. If the account is not properly setup, it will result in
     * a failure.
     *
     * @param ctx The application context
     * @param requestData The object holding the data related to the current request
     * @param features An array of the account features to require, may be null or empty
     */
    private void requestTokenWithoutActivity(
            Context ctx, RequestData requestData, String[] features) {
        if (lacksPermission(ctx, Manifest.permission.GET_ACCOUNTS, /* onlyPreM= */ true)) {
            // Protecting the AccountManager#getAccountsByTypeAndFeatures call.
            // API  < 23 Requires the GET_ACCOUNTS permission or throws an exception.
            // API >= 23 Requires the GET_ACCOUNTS permission (CONTACTS permission group) or
            //           returns only the accounts whose authenticator has a signature that
            //           matches our app. Working with this restriction and not requesting
            //           the permission is a valid use case in the context of WebView, so we
            //           don't require it on M+
            Log.e(
                    TAG,
                    "ERR_MISCONFIGURED_AUTH_ENVIRONMENT: GET_ACCOUNTS permission not "
                            + "granted. Aborting authentication.");
            HttpNegotiateAuthenticatorJni.get()
                    .setResult(
                            requestData.nativeResultObject,
                            HttpNegotiateAuthenticator.this,
                            NetError.ERR_MISCONFIGURED_AUTH_ENVIRONMENT,
                            null);
            return;
        }
        requestData.accountManager.getAccountsByTypeAndFeatures(
                mAccountType,
                features,
                new GetAccountsCallback(requestData),
                new Handler(ThreadUtils.getUiThreadLooper()));
    }

    /**
     * Requests an authentication token. Handles the account selection/creation and the credentials
     * confirmation if that is needed.
     * If there is more than one account, it will show an account picker dialog for
     * each query (e.g. html file, then favicon...)
     * Same if the credentials need to be confirmed.
     *
     * @param ctx The application context
     * @param activity The Activity context to use for launching new sub-Activities to prompt to
     *                 add an account, select an account, and/or enter a password, as necessary;
     *                 used only to call startActivity(); should not be null
     * @param requestData The object holding the data related to the current request
     * @param features An array of the account features to require, may be null or empty
     */
    private void requestTokenWithActivity(
            Context ctx, Activity activity, RequestData requestData, String[] features) {
        boolean isPreM = Build.VERSION.SDK_INT < Build.VERSION_CODES.M;
        String permission =
                isPreM ? "android.permission.MANAGE_ACCOUNTS" : Manifest.permission.GET_ACCOUNTS;

        // Check if the AccountManager#getAuthTokenByFeatures call can be made.
        // API  < 23 Requires the MANAGE_ACCOUNTS permission.
        // API >= 23 Requires the GET_ACCOUNTS permission to behave properly. When it's not granted,
        //           accounts not managed by the current application can't be retrieved. Depending
        //           on the authenticator implementation, it might prompt to create an account, but
        //           that won't be saved. This would be a bad user experience, so we also consider
        //           it a failure case.
        if (lacksPermission(ctx, permission, isPreM)) {
            Log.e(
                    TAG,
                    "ERR_MISCONFIGURED_AUTH_ENVIRONMENT: %s permission not granted. "
                            + "Aborting authentication",
                    permission);
            HttpNegotiateAuthenticatorJni.get()
                    .setResult(
                            requestData.nativeResultObject,
                            HttpNegotiateAuthenticator.this,
                            NetError.ERR_MISCONFIGURED_AUTH_ENVIRONMENT,
                            null);
            return;
        }

        requestData.accountManager.getAuthTokenByFeatures(
                mAccountType,
                requestData.authTokenType,
                features,
                activity,
                null,
                requestData.options,
                new GetTokenCallback(requestData),
                new Handler(ThreadUtils.getUiThreadLooper()));
    }

    /**
     * Returns whether the current context lacks a given permission. Skips the check on M+ systems
     * if {@code onlyPreM} is {@code true}, and just returns {@code false}.
     */
    @VisibleForTesting
    boolean lacksPermission(Context context, String permission, boolean onlyPreM) {
        if (onlyPreM && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return false;

        int permissionResult =
                context.checkPermission(permission, Process.myPid(), Process.myUid());
        return permissionResult != PackageManager.PERMISSION_GRANTED;
    }

    @NativeMethods
    interface Natives {
        void setResult(
                long nativeJavaNegotiateResultWrapper,
                HttpNegotiateAuthenticator caller,
                int status,
                String authToken);
    }
}
