// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;


/**
 * Interface to listen to receive events of an activity's lifecycle and options menu. This interface
 * is similar to Application.ActivityLifecycleCallbacks, but is inherently different. This interface
 * is intended to act as a listener for a specific Activity. The other is intended as a generic
 * listener to be registered at the Application level, for all Activities' lifecycles.
 */
public interface ActivityLifecycleListener {

    public void onActivityCreated(Activity activity, Bundle savedInstanceState);

    public boolean onActivityCreatedOptionsMenu(Activity activity, Menu menu);

    public void onActivityDestroyed(Activity activity);

    public boolean onActivityOptionsItemSelected(Activity activity, MenuItem item);

    public void onActivityPaused(Activity activity);

    public void onActivityResumed(Activity activity);

    public void onActivitySaveInstanceState(Activity activity, Bundle outState);

    public void onActivityStarted(Activity activity);

    public void onActivityStopped(Activity activity);
}
