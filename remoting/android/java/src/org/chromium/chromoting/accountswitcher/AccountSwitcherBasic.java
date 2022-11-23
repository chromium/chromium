// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.accountswitcher;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chromoting.R;

/**
 * This class implements a basic UI for a user account switcher. This implementation works on
 * Android O where the app can only see a list of accounts that have already been authorised for
 * the app. The only way to present a list of all Google accounts is by launching the Intent from
 * {@link android.accounts.AccountManager#newChooseAccountIntent()}. Accounts only become
 * authorised after the user has selected them from that list. So instead of showing a drop-down
 * list of all accounts, this implementation simply provides a label showing the current account,
 * and a button for the user to launch the Intent to select a new account.
 * <p>
 * A consequence is that this implementation never calls
 * {@link AccountSwitcher.Callback#onAccountsListEmpty()} because there is no way to distinguish
 * "the device has no accounts" from "no accounts are authorised for the app".
 * <p>
 * This implementation needs no special Android permissions, as it only tries to access the list
 * of accounts via launching an Intent, which needs no permissions.
 */
public class AccountSwitcherBasic extends AccountSwitcherBase {
    /** Only accounts of this type will be selectable. */
    private static final String ACCOUNT_TYPE = "com.google";

    /**
     * Request code used for showing the choose-account dialog. It must be different from other
     * REQUEST_CODEs in the app.
     */
    private static final int REQUEST_CODE_CHOOSE_ACCOUNT = 200;

    /** The currently-selected account. Can be null if no account is selected yet. */
    private String mSelectedAccount;

    /**
     * UI which appears above the navigation menu, showing currently-selected account and button.
     */
    private View mAccountsUi;

    private LinearLayout mContainer;

    /** Label showing the currently selected account name. */
    private TextView mAccountName;

    private Activity mActivity;

    /** The registered callback instance. */
    private Callback mCallback;

    /**
     * Constructs an account-switcher, using the given Activity to create any Views. Called from
     * the activity's onCreate() method.
     * @param activity Activity used for creating Views and performing UI operations.
     * @param callback Callback for receiving notifications from the account-switcher.
     */
    public AccountSwitcherBasic(Activity activity, Callback callback) {
        mActivity = activity;
        mCallback = callback;
        mContainer = new LinearLayout(activity);
        mContainer.setOrientation(LinearLayout.VERTICAL);
        mAccountsUi = activity.getLayoutInflater().inflate(R.layout.account_ui, mContainer, false);
        mContainer.addView(mAccountsUi);
        mAccountName = (TextView) mAccountsUi.findViewById(R.id.account_name);
        Button chooseAccount = (Button) mAccountsUi.findViewById(R.id.choose_account);
        chooseAccount.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onChooseAccount();
            }
        });
    }

    @Override
    public View getView() {
        return mContainer;
    }

    @Override
    public void setNavigation(View view) {
        mContainer.removeAllViews();
        mContainer.addView(mAccountsUi);
        mContainer.addView(view);
    }

    @Override
    public void setDrawer(View drawerView) {
    }

    @Override
    public void setSelectedAndRecentAccounts(String selected, String[] recents) {
        // This implementation does not support recents.
        mSelectedAccount = selected;
        mAccountName.setText(selected);
    }

    @Override
    public String[] getRecentAccounts() {
        return new String[0];
    }

    @Override
    public void reloadAccounts() {
        // This implementation does not maintain a list of accounts, so there's nothing to reload.
        // Instead, trigger the app to reload the host-list for any currently-selected account.
        // This ensures the host-list gets loaded when the user launches the app.
        if (mSelectedAccount != null) {
            mCallback.onAccountSelected(mSelectedAccount);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_CODE_CHOOSE_ACCOUNT && resultCode == Activity.RESULT_OK) {
            mSelectedAccount = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
            mAccountName.setText(mSelectedAccount);
            mCallback.onAccountSelected(mSelectedAccount);
            mCallback.onRequestCloseDrawer();
        }
    }

    @Override
    public void destroy() {
    }

    /** Called when the choose-account button is pressed. */
    private void onChooseAccount() {
        Account selected = null;
        if (mSelectedAccount != null) {
            selected = new Account(mSelectedAccount, ACCOUNT_TYPE);
        }

        Intent intent = AccountManager.newChooseAccountIntent(selected, /*allowableAccounts=*/null,
                new String[] {ACCOUNT_TYPE},
                /*descriptionOverrideText=*/null, /*addAccountAuthTokenType=*/null,
                /*addAccountRequiredFeatures=*/null, /*addAccountOptions=*/null);

        mActivity.startActivityForResult(intent, REQUEST_CODE_CHOOSE_ACCOUNT);
    }
}
