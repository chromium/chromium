// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import org.chromium.mojo.system.Core.HandleSignals;

/** Watches a handle for signals being satisfied. */
public interface Watcher {
    /** Callback passed to {@link Watcher#start}. */
    public interface Callback {
        /** Called when the handle is ready. */
        public void onResult(int result);
    }

    /** Starts watching a handle. */
    int start(Handle handle, HandleSignals signals, Callback callback);

    /** Cancels an already-started watch. */
    void cancel();

    /**
     * Destroys the underlying implementation. Other methods will fail after destroy has been
     * called.
     */
    void destroy();
}
