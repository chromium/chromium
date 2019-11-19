// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.base.annotations.JNINamespace;

/**
 * Helper methods for testing the UiThreadScheduler
 */
@JNINamespace("content")
public class UiThreadSchedulerTestUtils {
    /**
     * @param enabled Whether or not BrowserMainLoop::CreateStartupTasks should post tasks. This
     *        is useful because they will crash in in some testing scenarios despite not being
     *        needed for the test.
     */
    public static void postBrowserMainLoopStartupTasks(boolean enabled) {
        nativePostBrowserMainLoopStartupTasks(enabled);
    }

    private static native void nativePostBrowserMainLoopStartupTasks(boolean enabled);
}
