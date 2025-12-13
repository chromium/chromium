// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.content.browser.ChildProcessLauncherHelperImpl;
import org.chromium.content.browser.LauncherThread;

import java.util.concurrent.FutureTask;

// Utility class for testing child process related behavior.
public class ChildProcessUtils {
    private ChildProcessUtils() {}

    // Returns the number of sandboxed connection currently connected.
    public static int getConnectedSandboxedServicesCount() {
        try {
            FutureTask<Integer> task =
                    new FutureTask<>(
                            () ->
                                    ChildProcessLauncherHelperImpl
                                            .getConnectedSandboxedServicesCountForTesting());
            LauncherThread.post(task);
            return task.get();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
