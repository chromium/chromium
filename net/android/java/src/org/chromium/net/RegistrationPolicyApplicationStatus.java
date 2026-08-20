// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.app.Activity;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.NullMarked;

/** Registration policy which depends on the ApplicationState. */
@NullMarked
public class RegistrationPolicyApplicationStatus
        extends NetworkChangeNotifierAutoDetect.RegistrationPolicy
        implements ApplicationStatus.ApplicationStateListener,
                ApplicationStatus.WindowFocusChangedListener {
    private boolean mDestroyed;

    @Override
    protected void init(NetworkChangeNotifierAutoDetect notifier) {
        super.init(notifier);
        ApplicationStatus.registerApplicationStateListener(this);
        ApplicationStatus.registerWindowFocusChangedListener(this);
        onApplicationStateChange(ApplicationState.UNKNOWN);
    }

    @Override
    protected void destroy() {
        if (mDestroyed) return;
        ApplicationStatus.unregisterApplicationStateListener(this);
        ApplicationStatus.unregisterWindowFocusChangedListener(this);
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

    // ApplicationStatus.WindowFocusChangedListener
    @Override
    public void onWindowFocusChanged(Activity activity, boolean hasFocus) {
        // When waking from overnight Doze mode, callback registration can fail
        // with transient SecurityExceptions while the app was backgrounded.
        // Self-heal when the user interacts with the app by gaining window focus.
        if (hasFocus && isRegistrationFailed()) {
            register();
        }
    }
}
