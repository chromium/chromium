// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.accountswitcher;

import android.content.Intent;
import android.view.View;

/** Common interface for account-switcher implementations. */
public interface AccountSwitcher {
    /** Interface for receiving notifications from the account-switcher. */
    public interface Callback {
        /**
         * Called when the user has selected an account, or after the initial loading of the
         * device's accounts list, to notify the app that a current valid account is selected.
         * @param accountName The e-mail identifier of the selected account.
         */
        public void onAccountSelected(String accountName);

        /**
         * Called when the device's account-list has been loaded and no accounts were found on the
         * device.
         */
        public void onAccountsListEmpty();

        /**
         * Called to request the client to close any navigation drawer that contains the
         * account-switcher. This is a request that the client may ignore, if the client has some
         * other reason for keeping the drawer open.
         */
        public void onRequestCloseDrawer();
    }

    /**
     * Returns the account-switcher as a View that can be added to the UI. The caller will need to
     * set the appropriate LayoutParams (depending on the type of the parent View) before adding
     * this View to a container.
     */
    View getView();

    /**
     * Sets the view to be shown in the navigation portion of the drawer. All events and layout
     * are controlled by the caller. The caller should provide the list of navigation-drawer items
     * as an unattached View, and the implementation will attach this view as a child.
     */
    void setNavigation(View view);

    /**
     * Sets the view that holds the navigation drawer. The account-switcher may register with
     * {@link View.setOnApplyWindowInsetsListener} in order to render into the status-bar area.
     */
    void setDrawer(View drawerView);

    /**
     * Sets the user preferences for the currently-selected and most-recently-used accounts.
     * The caller is responsible for loading these preferences when the activity is started. This
     * should be called before calling {@link reloadAccounts} to load the device's active accounts.
     * The account-switching implementation will use the data from these methods to adjust the UI
     * so that only valid selections can be made.
     * @param selected Currently-selected account name (email) - can be null.
     * @param recents List of 0 or more most-recently-selected accounts.
     */
    void setSelectedAndRecentAccounts(String selected, String[] recents);

    /**
     * Gets the list of most-recently-used accounts. This should be called by the activity's
     * onPause() method, and the caller should save the results (together with the
     * currently-selected account) as persistent user preferences.
     * @return A list (maybe empty) of the most recently selected accounts. If the system
     * accounts-list was loaded from the device, all entries in the returned list will be valid
     * accounts.
     */
    String[] getRecentAccounts();

    /**
     * Reloads the list of accounts on the device. Called from the activity's onStart() method
     * (or whenever the device's accounts-list might have changed). This will notify the callback
     * that a valid account is selected, or that the list is empty.
     */
    void reloadAccounts();

    /**
     * This should be called from the controlling activity's onActivityResult() method. It allows
     * the account-switcher implementation to launch a child Activity and handle the result.
     */
    void onActivityResult(int requestCode, int resultCode, Intent data);

    /**
     * Releases any resources used by the account-switcher. Should be called by the activity's
     * onDestroy() method.
     */
    void destroy();
}
