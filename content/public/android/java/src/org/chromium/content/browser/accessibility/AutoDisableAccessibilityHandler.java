// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.view.View;

/**
 * Helper class that handles the logic and state behind the "Auto Disable" accessibility feature.
 * This class will start a timer with a {@link WAIT_TIME_BEFORE_DISABLING_ACCESSIBILITY_MS} delay.
 * When the timer is up, the class will notify the provided Client that the underlying renderer
 * accessibility engine can be disabled in the C++ code.
 *
 * Clients need to cancel/reset the timer based on their implementation (e.g. on a user action).
 *
 * Only one timer per instance can exist.
 */
public class AutoDisableAccessibilityHandler {
    private static final int WAIT_TIME_BEFORE_DISABLING_ACCESSIBILITY_MS = 60 * 1000;

    /**
     * Interface for any Client of this handler.
     */
    interface Client {
        /**
         * View of the Client. This View will be used to post a delayed Runnable for the
         * associated Handler's timer.
         * @return View of this Client
         */
        View getView();

        /**
         * Callback that is triggered when the running timer has expired for this Client.
         */
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

    /**
     * Starts running the timer for this instance.
     */
    public void startDisableTimer() {
        startDisableTimer(WAIT_TIME_BEFORE_DISABLING_ACCESSIBILITY_MS);
    }

    /**
     * Cancels the running timer for this instance.
     */
    public void cancelDisableTimer() {
        if (!mHasPendingTimer) return;

        mClient.getView().removeCallbacks(mRunnable);
        mHasPendingTimer = false;
    }

    /**
     * Resets the running timer for this instance.
     */
    public void resetPendingTimer() {
        if (!mHasPendingTimer) return;

        mClient.getView().removeCallbacks(mRunnable);
        mClient.getView().postDelayed(mRunnable, WAIT_TIME_BEFORE_DISABLING_ACCESSIBILITY_MS);
    }

    /**
     * Helper method to notify Client and reset local state when the timer has expired.
     */
    private void notifyDisable() {
        mClient.onDisabled();
        mHasPendingTimer = false;
    }
}