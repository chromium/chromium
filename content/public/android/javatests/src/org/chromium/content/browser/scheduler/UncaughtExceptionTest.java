// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.scheduler;

import android.os.Looper;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PowerMonitor;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.content.app.ContentMain;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Tests that uncaught exceptions propagate to the Java uncaught exception handler. */
@DoNotBatch(reason = "Does crazy things to the UI thread, not safe to batch.")
@RunWith(ContentJUnit4ClassRunner.class)
public class UncaughtExceptionTest {
    private final CallbackHelper mExceptionHelper = new CallbackHelper();

    @Test
    @MediumTest
    public void testUncaughtException() throws Exception {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        PowerMonitor.createForTests();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            ContentMain.start(/* startMinimalBrowser= */ true);
                        });
        Thread.setDefaultUncaughtExceptionHandler(
                (thread, exception) -> {
                    mExceptionHelper.notifyCalled();
                    // Re-start the UI thread so test cleanup works.
                    Looper.loop();
                });
        // Slow task to ensure we queue up a few tasks at once.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    try {
                        Thread.sleep(10);
                    } catch (Exception ex) {
                    }
                });
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    throw new RuntimeException();
                });
        // If tasks are batched, this task will cause a JNI abort, which we don't want.
        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> {});
        mExceptionHelper.waitForOnly();
    }
}
