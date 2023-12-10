// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.common;

/** Class for information about the process it is running in. */
public final class ContentProcessInfo {
    private static boolean sIsChildProcess;

    // Static members only, prevent instantiation.
    private ContentProcessInfo() {}

    /**
     * Set this as a child process; should be called as early as possible in process startup.
     * @param inChildProcess true if in child process.
     */
    public static void setInChildProcess(boolean inChildProcess) {
        sIsChildProcess = inChildProcess;
    }

    /**
     * Is this a child process?
     * <p>
     * setInChildProcess is called from the child process service, so this will not be valid until
     * that has started. In particular, it should not be called (directly or indirectly) from
     * Application.onCreate() since this is called before the service is created.
     *
     * @return true if it is a child process.
     */
    public static boolean inChildProcess() {
        return sIsChildProcess;
    }
}
