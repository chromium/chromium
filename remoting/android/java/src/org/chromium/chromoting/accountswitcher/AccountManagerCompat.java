// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.accountswitcher;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;

import java.util.ArrayList;

/** API compatibility wrapper for AccountManager methods. */
public class AccountManagerCompat {
    /**
     * @see android.accounts.AccountManager#newChooseAccountIntent()
     */
    @SuppressWarnings("deprecation")
    public static Intent newChooseAccountIntent(Account selectedAccount,
            ArrayList<Account> allowableAccounts, String[] allowableAccountTypes,
            String descriptionOverrideText, String addAccountAuthTokenType,
            String[] addAccountRequiredFeatures, Bundle addAccountOptions) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return AccountManager.newChooseAccountIntent(selectedAccount, allowableAccounts,
                    allowableAccountTypes, descriptionOverrideText, addAccountAuthTokenType,
                    addAccountRequiredFeatures, addAccountOptions);
        }
        return AccountManager.newChooseAccountIntent(selectedAccount, allowableAccounts,
                allowableAccountTypes, false, descriptionOverrideText, addAccountAuthTokenType,
                addAccountRequiredFeatures, addAccountOptions);
    }
}
