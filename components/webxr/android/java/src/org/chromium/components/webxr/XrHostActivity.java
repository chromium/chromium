// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.SurfaceView;

// TODO(crbug.com/40904930): Investigate if this would benefit from
// extending ChromeBaseAppCompatActivity

/**
 * An activity for which will host an OpenXR instance. Having the OpenXR instance be created with a
 * separate activity than that of the main browser makes lifetime tracking and returning to the 2D
 * browser when done cleaner.
 */
public class XrHostActivity extends Activity {
    /**
     * Creates an Intent to start the {@link XrHostActivity}.
     * @param context  Context to use when constructing the Intent.
     * @return Intent that can be used to begin presenting with OpenXR.
     */
    public static Intent createIntent(Context context) {
        Intent intent = new Intent();
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClassName(context.getPackageName(), XrHostActivity.class.getName());
        return intent;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // The activity should only be created by the XrSessionCoordinator during an active session.
        // If it was created by any other means, finish the activity immediately.
        if (!XrSessionCoordinator.hasActiveSession()) {
            finish();
            return;
        }

        SurfaceView defaultView = new SurfaceView(this);
        setContentView(defaultView);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        boolean result = XrSessionCoordinator.onXrHostActivityReady(this);
        assert result;
    }

    @Override
    public void onStop() {
        super.onStop();

        XrSessionCoordinator.endActiveSessionFromXrHost();

        finishAndRemoveTask();
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();

        XrSessionCoordinator.endActiveSessionFromXrHost();
    }
}
