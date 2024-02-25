// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.view.View;

import androidx.annotation.VisibleForTesting;

/**
 * Helper class that handles the logic and state behind the "Auto Disable" accessibility feature.
 * Clients need to cancel/reset the timer based on their implementation (e.g. on a user action).
 * Only one timer per instance can exist.
 */
public class AutoDisableAccessibilityHandler {
    /** Interface for any Client of this handler. */
    interface Client {
        /**
         * View of the Client. This View will be used to post a delayed Runnable for the
         * associated Handler's timer.
         * @return View of this Client
         */
        View getView();

        /** Callback that is triggered when the running timer has expired for this Client. */
        void onDisabled();
    }

    private final Client mClient;
    private final Runnable mRunnable;
    private boolean mHasPendingTimer;

    public AutoDisableAccessibilityHandler(Client client) {
        this.mClient = client;
        mRunnable = this::notifyDisable;
    }

    /**
     * Starts running the timer for this instance that will run for the given duration in ms.
     * @param duration of the timer, in ms.
     */
    public void startDisableTimer(int duration) {
        if (mHasPendingTimer) return;

        mClient.getView().postDelayed(mRunnable, duration);
        mHasPendingTimer = true;
    }

    /** Cancels the running timer for this instance. */
    public void cancelDisableTimer() {
        if (!mHasPendingTimer) return;

        mClient.getView().removeCallbacks(mRunnable);
        mHasPendingTimer = false;
    }

    /** Helper method to notify Client and reset local state when the timer has expired. */
    @VisibleForTesting()
    public void notifyDisable() {
        mClient.onDisabled();
        mHasPendingTimer = false;
    }

    /** Return true when there is a pending timer. */
    @VisibleForTesting
    public boolean hasPendingTimer() {
        return mHasPendingTimer;
    }
}
