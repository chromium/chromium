// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;

/** Registration policy which depends on the ApplicationState. */
public class RegistrationPolicyApplicationStatus
        extends NetworkChangeNotifierAutoDetect.RegistrationPolicy
        implements ApplicationStatus.ApplicationStateListener {
    private boolean mDestroyed;

    @Override
    protected void init(NetworkChangeNotifierAutoDetect notifier) {
        super.init(notifier);
        ApplicationStatus.registerApplicationStateListener(this);
        onApplicationStateChange(ApplicationState.UNKNOWN);
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
        // Use hasVisibleActivities() to determine if one of Chrome's activities
        // is visible. Using |newState| causes spurious unregister then register
        // events when flipping between Chrome's Activities, crbug.com/1030229.
        if (ApplicationStatus.hasVisibleActivities()) {
            register();
        } else {
            unregister();
        }
    }
}
