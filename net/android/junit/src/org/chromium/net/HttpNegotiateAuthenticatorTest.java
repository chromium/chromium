// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowAccountManager;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.net.HttpNegotiateAuthenticator.GetAccountsCallback;
import org.chromium.net.HttpNegotiateAuthenticator.RequestData;

import java.io.IOException;
import java.util.List;

/**
 * Robolectric tests for HttpNegotiateAuthenticator
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {HttpNegotiateAuthenticatorTest.ExtendedShadowAccountManager.class,
                ShadowMultiDex.class})
public class HttpNegotiateAuthenticatorTest {
    /**
     * User the AccountManager to inject a mock instance.
     * Note: Shadow classes need to be public and static.
     */
    @Implements(AccountManager.class)
    public static class ExtendedShadowAccountManager extends ShadowAccountManager {
        @Implementation
        public static AccountManager get(Context context) {
            return sMockAccountManager;
        }
    }

    @Rule
    public JniMocker mocker = new JniMocker();
    @Mock
    private static AccountManager sMockAccountManager;
    @Mock
    private HttpNegotiateAuthenticator.Natives mAuthenticatorJniMock;
    @Captor
    private ArgumentCaptor<AccountManagerCallback<Bundle>> mBundleCallbackCaptor;
    @Captor
    private ArgumentCaptor<AccountManagerCallback<Account[]>> mAccountCallbackCaptor;
    @Captor
    private ArgumentCaptor<Bundle> mBundleCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(HttpNegotiateAuthenticatorJni.TEST_HOOKS, mAuthenticatorJniMock);
    }

    /**
     * Test of {@link HttpNegotiateAuthenticator#getNextAuthToken}
     */
    @Test
    public void testGetNextAuthToken() {
        final String accountType = "Dummy_Account";
        HttpNegotiateAuthenticator authenticator = createAuthenticator(accountType);
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();

        authenticator.getNextAuthToken(0, "test_principal", "", true);

        verify(sMockAccountManager)
                .getAuthTokenByFeatures(eq(accountType), eq("SPNEGO:HOSTBASED:test_principal"),
                        eq(new String[] {"SPNEGO"}), any(Activity.class), (Bundle) isNull(),
                        mBundleCaptor.capture(), mBundleCallbackCaptor.capture(),
                        any(Handler.class));

        assertThat("There is no existing context",
                mBundleCaptor.getValue().get(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT),
                nullValue());
        assertThat("The existing token is empty",
                mBundleCaptor.getValue().getString(HttpNegotiateConstants.KEY_INCOMING_AUTH_TOKEN),
                equalTo(""));
        assertThat("Delegation is allowed",
                mBundleCaptor.getValue().getBoolean(HttpNegotiateConstants.KEY_CAN_DELEGATE),
                equalTo(true));
        assertThat("getAuthTokenByFeatures was called with a callback",
                mBundleCallbackCaptor.getValue(), notNullValue());
    }

    /**
     * Test of {@link HttpNegotiateAuthenticator#getNextAuthToken} without a visible activity.
     * This emulates the behavior with WebView, where the application is a generic one and doesn't
     * set up the ApplicationStatus the same way.
     */
    @Test
    @Config(application = Application.class)
    public void testGetNextAuthTokenWithoutActivity() {
        final String accountType = "Dummy_Account";
        final Account[] returnedAccount = {new Account("name", accountType)};
        HttpNegotiateAuthenticator authenticator = createAuthenticator(accountType);

        authenticator.getNextAuthToken(1234, "test_principal", "", true);

        Assert.assertNull(ApplicationStatus.getLastTrackedFocusedActivity());
        verify(sMockAccountManager).getAccountsByTypeAndFeatures(
                eq(accountType),
                eq(new String[]{"SPNEGO"}),
                mAccountCallbackCaptor.capture(),
                any(Handler.class));

        mAccountCallbackCaptor.getValue().run(makeFuture(returnedAccount));

        verify(sMockAccountManager).getAuthToken(
                any(Account.class),
                eq("SPNEGO:HOSTBASED:test_principal"),
                mBundleCaptor.capture(),
                eq(true),
                any(HttpNegotiateAuthenticator.GetTokenCallback.class),
                any(Handler.class));

        assertThat("There is no existing context",
                mBundleCaptor.getValue().get(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT),
                nullValue());
        assertThat("The existing token is empty",
                mBundleCaptor.getValue().getString(HttpNegotiateConstants.KEY_INCOMING_AUTH_TOKEN),
                equalTo(""));
        assertThat("Delegation is allowed",
                mBundleCaptor.getValue().getBoolean(HttpNegotiateConstants.KEY_CAN_DELEGATE),
                equalTo(true));
    }

    /** Tests the behavior of {@link HttpNegotiateAuthenticator.GetAccountsCallback} */
    @Test
    public void testGetAccountCallback() {
        String type = "Dummy_Account";
        HttpNegotiateAuthenticator authenticator = createAuthenticator(type);
        RequestData requestData = new RequestData();
        requestData.nativeResultObject = 42;
        requestData.accountManager = sMockAccountManager;
        GetAccountsCallback callback = authenticator.new GetAccountsCallback(requestData);

        // Should fail because there are no accounts
        callback.run(makeFuture(new Account[]{}));
        verify(mAuthenticatorJniMock)
                .setResult(eq(42L), eq(authenticator), eq(NetError.ERR_MISSING_AUTH_CREDENTIALS),
                        (String) isNull());

        // Should succeed, for a single account we use it for the AccountManager#getAuthToken call.
        Account testAccount = new Account("a", type);
        callback.run(makeFuture(new Account[]{testAccount}));
        verify(sMockAccountManager)
                .getAuthToken(eq(testAccount), (String) isNull(), (Bundle) isNull(), eq(true),
                        any(HttpNegotiateAuthenticator.GetTokenCallback.class), any(Handler.class));

        // Should fail because there is more than one account
        callback.run(makeFuture(new Account[]{new Account("a", type), new Account("b", type)}));
        verify(mAuthenticatorJniMock, times(2))
                .setResult(eq(42L), eq(authenticator), eq(NetError.ERR_MISSING_AUTH_CREDENTIALS),
                        (String) isNull());
    }

    /**
     * Tests the behavior of {@link HttpNegotiateAuthenticator.GetTokenCallback} when the result it
     * receives contains an intent rather than a token directly.
     */
    @Test
    public void testGetTokenCallbackWithIntent() {
        String type = "Dummy_Account";
        HttpNegotiateAuthenticator authenticator = createAuthenticator(type);
        RequestData requestData = new RequestData();
        requestData.nativeResultObject = 42;
        requestData.authTokenType = "foo";
        requestData.account = new Account("a", type);
        requestData.accountManager = sMockAccountManager;
        Bundle b = new Bundle();
        b.putParcelable(AccountManager.KEY_INTENT, new Intent());

        authenticator.new GetTokenCallback(requestData).run(makeFuture(b));
        verifyZeroInteractions(sMockAccountManager);

        // Verify that the broadcast receiver is registered
        Intent intent = new Intent(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION);
        ShadowApplication shadowApplication = ShadowApplication.getInstance();
        List<BroadcastReceiver> receivers = shadowApplication.getReceiversForIntent(intent);
        assertThat("There is one registered broadcast receiver", receivers.size(), equalTo(1));

        // Send the intent to the receiver.
        BroadcastReceiver receiver = receivers.get(0);
        receiver.onReceive(RuntimeEnvironment.application.getApplicationContext(), intent);

        // Verify that the auth token is properly requested from the account manager.
        verify(sMockAccountManager)
                .getAuthToken(eq(new Account("a", type)), eq("foo"), (Bundle) isNull(), eq(true),
                        any(HttpNegotiateAuthenticator.GetTokenCallback.class), (Handler) isNull());
    }

    /**
     * Test of callback called when getting the auth token completes.
     */
    @Test
    public void testAccountManagerCallbackRun() {
        HttpNegotiateAuthenticator authenticator = createAuthenticator("Dummy_Account");

        Robolectric.buildActivity(Activity.class).create().start().resume().visible();

        // Call getNextAuthToken to get the callback
        authenticator.getNextAuthToken(1234, "test_principal", "", true);
        verify(sMockAccountManager)
                .getAuthTokenByFeatures(any(String.class), any(String.class), any(String[].class),
                        any(Activity.class), (Bundle) isNull(), any(Bundle.class),
                        mBundleCallbackCaptor.capture(), any(Handler.class));

        Bundle resultBundle = new Bundle();
        Bundle context = new Bundle();
        context.putString("String", "test_context");
        resultBundle.putInt(HttpNegotiateConstants.KEY_SPNEGO_RESULT, HttpNegotiateConstants.OK);
        resultBundle.putBundle(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT, context);
        resultBundle.putString(AccountManager.KEY_AUTHTOKEN, "output_token");
        mBundleCallbackCaptor.getValue().run(makeFuture(resultBundle));
        verify(mAuthenticatorJniMock).setResult(1234, authenticator, 0, "output_token");

        // Check that the next call to getNextAuthToken uses the correct context
        authenticator.getNextAuthToken(5678, "test_principal", "", true);
        verify(sMockAccountManager, times(2))
                .getAuthTokenByFeatures(any(String.class), any(String.class), any(String[].class),
                        any(Activity.class), (Bundle) isNull(), mBundleCaptor.capture(),
                        mBundleCallbackCaptor.capture(), any(Handler.class));

        assertThat("The spnego context is preserved between calls",
                mBundleCaptor.getValue().getBundle(HttpNegotiateConstants.KEY_SPNEGO_CONTEXT),
                equalTo(context));

        // Test exception path
        mBundleCallbackCaptor.getValue().run(
                this.<Bundle>makeFuture(new OperationCanceledException()));
        verify(mAuthenticatorJniMock).setResult(5678, authenticator, NetError.ERR_UNEXPECTED, null);
    }

    @Test
    public void testPermissionDenied() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        HttpNegotiateAuthenticator authenticator = createAuthenticator("Dummy_Account", true);

        authenticator.getNextAuthToken(1234, "test_principal", "", true);
        verify(mAuthenticatorJniMock)
                .setResult(anyLong(), eq(authenticator),
                        eq(NetError.ERR_MISCONFIGURED_AUTH_ENVIRONMENT), (String) isNull());
    }

    @Test
    public void testAccountManagerCallbackNullErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(null, NetError.ERR_UNEXPECTED);
    }

    @Test
    public void testAccountManagerCallbackUnexpectedErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(HttpNegotiateConstants.ERR_UNEXPECTED, NetError.ERR_UNEXPECTED);
    }

    @Test
    public void testAccountManagerCallbackAbortedErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(HttpNegotiateConstants.ERR_ABORTED, NetError.ERR_ABORTED);
    }

    @Test
    public void testAccountManagerCallbackSecLibErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(HttpNegotiateConstants.ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS,
                NetError.ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS);
    }

    @Test
    public void testAccountManagerCallbackInvalidResponseErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(
                HttpNegotiateConstants.ERR_INVALID_RESPONSE, NetError.ERR_INVALID_RESPONSE);
    }

    @Test
    public void testAccountManagerCallbackInvalidAuthCredsErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(HttpNegotiateConstants.ERR_INVALID_AUTH_CREDENTIALS,
                NetError.ERR_INVALID_AUTH_CREDENTIALS);
    }

    @Test
    public void testAccountManagerCallbackUnsuppAutchSchemeErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(HttpNegotiateConstants.ERR_UNSUPPORTED_AUTH_SCHEME,
                NetError.ERR_UNSUPPORTED_AUTH_SCHEME);
    }

    @Test
    public void testAccountManagerCallbackMissingAuthCredsErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(HttpNegotiateConstants.ERR_MISSING_AUTH_CREDENTIALS,
                NetError.ERR_MISSING_AUTH_CREDENTIALS);
    }

    @Test
    public void testAccountManagerCallbackUndocSecLibErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(HttpNegotiateConstants.ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS,
                NetError.ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS);
    }

    @Test
    public void testAccountManagerCallbackMalformedIdentityErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        checkErrorReturn(
                HttpNegotiateConstants.ERR_MALFORMED_IDENTITY, NetError.ERR_MALFORMED_IDENTITY);
    }

    @Test
    public void testAccountManagerCallbackInvalidErrorReturns() {
        Robolectric.buildActivity(Activity.class).create().start().resume().visible();
        // 9999 is not a valid return value
        checkErrorReturn(9999, NetError.ERR_UNEXPECTED);
    }

    private void checkErrorReturn(Integer spnegoError, int expectedError) {
        HttpNegotiateAuthenticator authenticator = createAuthenticator("Dummy_Account");

        // Call getNextAuthToken to get the callback
        authenticator.getNextAuthToken(1234, "test_principal", "", true);
        verify(sMockAccountManager)
                .getAuthTokenByFeatures(any(String.class), any(String.class), any(String[].class),
                        any(Activity.class), (Bundle) isNull(), any(Bundle.class),
                        mBundleCallbackCaptor.capture(), any(Handler.class));

        Bundle resultBundle = new Bundle();
        if (spnegoError != null) {
            resultBundle.putInt(HttpNegotiateConstants.KEY_SPNEGO_RESULT, spnegoError);
        }
        mBundleCallbackCaptor.getValue().run(makeFuture(resultBundle));
        verify(mAuthenticatorJniMock)
                .setResult(anyLong(), eq(authenticator), eq(expectedError), (String) isNull());
    }

    /**
     * Returns a future that successfully returns the provided result.
     * Hides mocking related annoyances: compiler warnings and irrelevant catch clauses.
     */
    private <T> AccountManagerFuture<T> makeFuture(T result) {
        // Avoid warning when creating mock accountManagerFuture, can't take .class of an
        // instantiated generic type, yet compiler complains if I leave it uninstantiated.
        @SuppressWarnings("unchecked")
        AccountManagerFuture<T> accountManagerFuture = mock(AccountManagerFuture.class);
        try {
            when(accountManagerFuture.getResult()).thenReturn(result);
        } catch (OperationCanceledException | AuthenticatorException | IOException e) {
            // Can never happen - artifact of Mockito.
            fail();
        }
        return accountManagerFuture;
    }

    /**
     * Returns a future that fails with the provided exception when trying to get its result.
     * Hides mocking related annoyances: compiler warnings and irrelevant catch clauses.
     */
    private <T> AccountManagerFuture<T> makeFuture(Exception ex) {
        // Avoid warning when creating mock accountManagerFuture, can't take .class of an
        // instantiated generic type, yet compiler complains if I leave it uninstantiated.
        @SuppressWarnings("unchecked")
        AccountManagerFuture<T> accountManagerFuture = mock(AccountManagerFuture.class);
        try {
            when(accountManagerFuture.getResult()).thenThrow(ex);
        } catch (OperationCanceledException | AuthenticatorException | IOException e) {
            // Can never happen - artifact of Mockito.
            fail();
        }
        return accountManagerFuture;
    }

    /**
     * Returns a new authenticator with an overridden lacksPermission method.
     */
    private HttpNegotiateAuthenticator createAuthenticator(
            String accountType, boolean lacksPermission) {
        return new HttpNegotiateAuthenticator(accountType) {
            @Override
            boolean lacksPermission(Context context, String permission, boolean onlyPreM) {
                return lacksPermission;
            }
        };
    }

    private HttpNegotiateAuthenticator createAuthenticator(String accountType) {
        return createAuthenticator(accountType, false);
    }
}
