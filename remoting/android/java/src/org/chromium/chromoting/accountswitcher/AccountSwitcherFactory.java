// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.accountswitcher;

import android.app.Activity;

/**
 * Factory class for creating AccountSwitcher implementations. This enables official builds of
 * the project to implement an alternative account-switching UI.
 */
public class AccountSwitcherFactory {
    /**
     * The singleton instance of this class. This is provided by the Application context, so that
     * different application builds can provide different implementations. It has to be
     * dependency-injected, because the Application context is defined in a higher-level build
     * target that depends on this code.
     */
    private static AccountSwitcherFactory sInstance;

    /** Returns the instance. Should be called on the main thread. */
    public static AccountSwitcherFactory getInstance() {
        return sInstance;
    }

    /**
     * Factory method to create an AccountSwitcher. This method returns the public implementation,
     * but it can be overridden to provide an alternative account-switcher implementation. This is
     * called during the Activity's onCreate() handler.
     * @param activity Activity used for UI operations.
     * @param callback Callback for receiving notifications from the account-switcher.
     */
    public AccountSwitcher createAccountSwitcher(Activity activity,
            AccountSwitcher.Callback callback) {
        return new AccountSwitcherBasic(activity, callback);
    }

    /**
     * Sets the global instance. Called by the Application context during initialization before any
     * Activity is created.
     */
    public static void setInstance(AccountSwitcherFactory instance) {
        sInstance = instance;
    }
}
