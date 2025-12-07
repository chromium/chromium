// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import org.chromium.build.annotations.NullMarked;

import java.io.Closeable;

/** Definition of a run loop. */
@NullMarked
public interface RunLoop extends Closeable {
    /** Start the run loop. It will continue until quit() is called. */
    void run();

    /** Start the run loop and stop it as soon as no task is present in the work queue. */
    void runUntilIdle();

    /**
     * Add a runnable to the queue of tasks.
     *
     * @param runnable Callback to be executed by the run loop.
     * @param delay Delay, in MojoTimeTicks (microseconds) before the callback should be executed.
     */
    void postDelayedTask(Runnable runnable, long delay);

    /** Destroy the run loop and deregister it from Core. */
    @Override
    void close();
}
