// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;

/**
 * A dummy implementation of ActivityListener that will be passed to any activity requesting
 * a capability that is not currently enabled.
 */
public class DummyActivityLifecycleListener implements ActivityLifecycleListener {

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {}

    @Override
    public void onActivityDestroyed(Activity activity) {}

    @Override
    public void onActivityPaused(Activity activity) {}

    @Override
    public void onActivityResumed(Activity activity) {}

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {}

    @Override
    public void onActivityStarted(Activity activity) {}

    @Override
    public void onActivityStopped(Activity activity) {}

    @Override
    public boolean onActivityCreatedOptionsMenu(Activity activity, Menu menu) {
        return false;
    }

    @Override
    public boolean onActivityOptionsItemSelected(Activity activity, MenuItem item) {
        return false;
    }
}
