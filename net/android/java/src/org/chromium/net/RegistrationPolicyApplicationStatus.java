// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;

/**
 * Regsitration policy which depends on the ApplicationState.
 */
public class RegistrationPolicyApplicationStatus
        extends NetworkChangeNotifierAutoDetect.RegistrationPolicy
        implements ApplicationStatus.ApplicationStateListener {
    private boolean mDestroyed;

    @Override
    protected void init(NetworkChangeNotifierAutoDetect notifier) {
        super.init(notifier);
        ApplicationStatus.registerApplicationStateListener(this);
        onApplicationStateChange(getApplicationState());
    }

    @Override
    protected void destroy() {
        if (mDestroyed) return;
        ApplicationStatus.unregisterApplicationStateListener(this);
        mDestroyed = true;
    }

    // ApplicationStatus.ApplicationStateListener
    @Override
    public void onApplicationStateChange(int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            register();
        } else if (newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            unregister();
        }
    }

    /**
     * Returns the activity's status.
     * @return an {@code int} that is one of {@code ApplicationState.HAS_*_ACTIVITIES}.
     */
    @VisibleForTesting
    int getApplicationState() {
        return ApplicationStatus.getStateForApplication();
    }
}
