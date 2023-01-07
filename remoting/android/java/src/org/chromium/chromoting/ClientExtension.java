// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;

/**
 * Interface to extend the Android client's functionality by providing a way to communicate with
 * the Chromoting host.
 */
public interface ClientExtension {

    /** Returns the capability supported by this extension, or an empty string. */
    public String getCapability();

    /**
     * Called when the client receives an extension message from the host through JniInterface. It
     * returns true if the message was handled appropriately, and false otherwise.
     */
    public boolean onExtensionMessage(String type, String data);

    /**
     * Called when an activity offers to accept an ActivityListener for its lifecycle events.
     * This gives Extensions the option to hook into an existing Activity, get notified about
     * changes in its state and modify its behavior. Returns the extension's activity listener,
     * or null.
     */
    public ActivityLifecycleListener onActivityAcceptingListener(Activity activity);

}
