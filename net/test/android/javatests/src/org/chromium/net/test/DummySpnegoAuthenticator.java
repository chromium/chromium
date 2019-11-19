// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.content.Context;
import android.os.Bundle;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.net.HttpNegotiateConstants;

import java.io.IOException;

/**
 * Dummy Android authenticator, to test SPNEGO/Keberos support on Android. This is deliberately
 * minimal, and is not intended as an example of how to write a real SPNEGO Authenticator.
 */
@JNINamespace("net::android")
public class DummySpnegoAuthenticator extends AbstractAccountAuthenticator {
    private static final String ACCOUNT_TYPE = "org.chromium.test.DummySpnegoAuthenticator";
    private static final String ACCOUNT_NAME = "DummySpnegoAccount";
    private static int sResult;
    private static String sToken;
    private static boolean sCheckArguments;
    private static long sNativeDummySpnegoAuthenticator;
    private static final int GSS_S_COMPLETE = 0;
    private static final int GSS_S_CONTINUE_NEEDED = 1;
    private static final int GSS_S_FAILURE = 2;

    /**
     * @param context
     */
    public DummySpnegoAuthenticator(Context context) {
        super(context);
    }

    @Override
    public Bundle addAccount(AccountAuthenticatorResponse arg0, String accountType, String arg2,
            String[] arg3, Bundle arg4) {
        Bundle result = new Bundle();
        result.putInt(AccountManager.KEY_ERROR_CODE, AccountManager.ERROR_CODE_BAD_REQUEST);
        result.putString(AccountManager.KEY_ERROR_MESSAGE, "Can't add new SPNEGO accounts");
        return result;
    }

    @Override
    public Bundle confirmCredentials(AccountAuthenticatorResponse arg0, Account arg1, Bundle arg2) {
        Bundle result = new Bundle();
        result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, true);
        return result;
    }

    @Override
    public Bundle editProperties(AccountAuthenticatorResponse arg0, String arg1) {
        return new Bundle();
    }

    @Override
    public Bundle getAuthToken(AccountAuthenticatorResponse response, Account account,
            String authTokenType, Bundle options) {
        long nativeQuery = nativeGetNextQuery(sNativeDummySpnegoAuthenticator);
        String incomingToken = options.getString(HttpNegotiateConstants.KEY_INCOMING_AUTH_TOKEN);
        nativeCheckGetTokenArguments(nativeQuery, incomingToken);
        Bundle result = new Bundle();
        result.putString(AccountManager.KEY_ACCOUNT_NAME, account.name);
        result.putString(AccountManager.KEY_ACCOUNT_TYPE, account.type);
        result.putString(AccountManager.KEY_AUTHTOKEN, nativeGetTokenToReturn(nativeQuery));
        result.putInt(HttpNegotiateConstants.KEY_SPNEGO_RESULT,
                decodeResult(nativeGetResult(nativeQuery)));
        return result;
    }

    /**
     * @param nativeGetResult
     * @return
     */
    private int decodeResult(int gssApiResult) {
        // This only handles the result values currently used in the tests.
        switch (gssApiResult) {
            case GSS_S_COMPLETE:
            case GSS_S_CONTINUE_NEEDED:
                return 0;
            case GSS_S_FAILURE:
                return HttpNegotiateConstants.ERR_MISSING_AUTH_CREDENTIALS;
            default:
                return HttpNegotiateConstants.ERR_UNEXPECTED;
        }
    }

    @Override
    public String getAuthTokenLabel(String arg0) {
        return "Spnego " + arg0;
    }

    @Override
    public Bundle hasFeatures(AccountAuthenticatorResponse arg0, Account arg1, String[] features) {
        Bundle result = new Bundle();
        for (String feature : features) {
            if (!feature.equals(HttpNegotiateConstants.SPNEGO_FEATURE)) {
                result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, false);
                return result;
            }
        }
        result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, true);
        return result;
    }

    @Override
    public Bundle updateCredentials(
            AccountAuthenticatorResponse arg0, Account arg1, String arg2, Bundle arg3) {
        Bundle result = new Bundle();
        result.putInt(AccountManager.KEY_ERROR_CODE, AccountManager.ERROR_CODE_BAD_REQUEST);
        result.putString(AccountManager.KEY_ERROR_MESSAGE, "Can't add new SPNEGO accounts");
        return result;
    }

    /**
     * Called from tests, sets up the test account, if it doesn't already exist
     */
    @CalledByNative
    private static void ensureTestAccountExists() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        AccountManager am = AccountManager.get(activity);
        Account account = new Account(ACCOUNT_NAME, ACCOUNT_TYPE);
        am.addAccountExplicitly(account, null, null);
    }

    /**
     * Called from tests to tidy up test accounts.
     */
    @SuppressWarnings("deprecation")
    @CalledByNative
    private static void removeTestAccounts() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        AccountManager am = AccountManager.get(activity);
        String features[] = {HttpNegotiateConstants.SPNEGO_FEATURE};
        try {
            Account accounts[] =
                    am.getAccountsByTypeAndFeatures(ACCOUNT_TYPE, features, null, null).getResult();
            for (Account account : accounts) {
                // Deprecated, but the replacement not available on Android JB.
                am.removeAccount(account, null, null).getResult();
            }
        } catch (OperationCanceledException | AuthenticatorException | IOException e) {
            // Should never happen. This is tidy-up after the tests. Ignore.
        }
    }

    @CalledByNative
    private static void setNativeAuthenticator(long nativeDummySpnegoAuthenticator) {
        sNativeDummySpnegoAuthenticator = nativeDummySpnegoAuthenticator;
    }

    /**
     * Send the relevant decoded arguments of getAuthToken to C++ for checking by googletest checks
     * If the checks fail then the C++ unit test using this authenticator will fail.
     *
     * @param authTokenType
     * @param spn
     * @param incomingToken
     */
    @NativeClassQualifiedName("DummySpnegoAuthenticator::SecurityContextQuery")
    private native void nativeCheckGetTokenArguments(long nativeQuery, String incomingToken);

    @NativeClassQualifiedName("DummySpnegoAuthenticator::SecurityContextQuery")
    private native String nativeGetTokenToReturn(long nativeQuery);

    @NativeClassQualifiedName("DummySpnegoAuthenticator::SecurityContextQuery")
    private native int nativeGetResult(long nativeQuery);

    private native long nativeGetNextQuery(long nativeDummySpnegoAuthenticator);
}
