// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Helper methods for testing the UiThreadScheduler */
@JNINamespace("content")
public class UiThreadSchedulerTestUtils {
    /**
     * @param enabled Whether or not BrowserMainLoop::CreateStartupTasks should post tasks. This
     *        is useful because they will crash in in some testing scenarios despite not being
     *        needed for the test.
     */
    public static void postBrowserMainLoopStartupTasks(boolean enabled) {
        UiThreadSchedulerTestUtilsJni.get().postBrowserMainLoopStartupTasks(enabled);
    }

    @NativeMethods
    interface Natives {
        void postBrowserMainLoopStartupTasks(boolean enabled);
    }
}
