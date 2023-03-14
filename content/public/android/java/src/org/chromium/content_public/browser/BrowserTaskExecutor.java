// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.os.Handler;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.SingleThreadTaskRunner;
import org.chromium.base.task.SingleThreadTaskRunnerImpl;
import org.chromium.base.task.TaskExecutor;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.content.browser.UiThreadTaskTraitsImpl;

/**
 * This {@link TaskExecutor} is for tasks posted with {@link UiThreadTaskTraits}. It maps directly
 * to content::BrowserTaskExecutor except only UI thread posting is supported from java.
 *
 * NB if you wish to post to the thread pool then use {@link TaskTraits} instead of {@link
 * UiThreadTaskTraits}.
 */
public class BrowserTaskExecutor implements TaskExecutor {
    private static boolean sRegistered;

    private final SingleThreadTaskRunner mDefaultTaskRunner;
    private final SingleThreadTaskRunner mBestEffortTaskRunner;
    private final SingleThreadTaskRunner mUserVisibleTaskRunner;
    private final SingleThreadTaskRunner mUserBlockingTaskRunner;

    public BrowserTaskExecutor() {
        Handler handler = ThreadUtils.getUiThreadHandler();
        mDefaultTaskRunner =
                new SingleThreadTaskRunnerImpl(handler, UiThreadTaskTraitsImpl.DEFAULT);
        mBestEffortTaskRunner =
                new SingleThreadTaskRunnerImpl(handler, UiThreadTaskTraitsImpl.BEST_EFFORT);
        mUserVisibleTaskRunner =
                new SingleThreadTaskRunnerImpl(handler, UiThreadTaskTraitsImpl.USER_VISIBLE);
        mUserBlockingTaskRunner =
                new SingleThreadTaskRunnerImpl(handler, UiThreadTaskTraitsImpl.USER_BLOCKING);
    }

    @Override
    public TaskRunner createTaskRunner(TaskTraits taskTraits) {
        return createSingleThreadTaskRunner(taskTraits);
    }

    @Override
    public SequencedTaskRunner createSequencedTaskRunner(TaskTraits taskTraits) {
        return createSingleThreadTaskRunner(taskTraits);
    }

    @Override
    public SingleThreadTaskRunner createSingleThreadTaskRunner(TaskTraits taskTraits) {
        if (UiThreadTaskTraitsImpl.DEFAULT.equals(taskTraits)) {
            return mDefaultTaskRunner;
        } else if (UiThreadTaskTraitsImpl.BEST_EFFORT.equals(taskTraits)) {
            return mBestEffortTaskRunner;
        } else if (UiThreadTaskTraitsImpl.USER_VISIBLE.equals(taskTraits)) {
            return mUserVisibleTaskRunner;
        } else if (UiThreadTaskTraitsImpl.USER_BLOCKING.equals(taskTraits)) {
            return mUserBlockingTaskRunner;
        } else {
            // Add support for additional TaskTraits here if encountering this exception.
            throw new RuntimeException();
        }
    }

    @Override
    public void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
        createSingleThreadTaskRunner(taskTraits).postDelayedTask(task, delay);
    }

    @Override
    public boolean canRunTaskImmediately(TaskTraits traits) {
        return createSingleThreadTaskRunner(traits).belongsToCurrentThread();
    }

    public static void register() {
        // In some tests we will get called multiple times.
        if (sRegistered) return;
        sRegistered = true;

        PostTask.registerTaskExecutor(
                UiThreadTaskTraitsImpl.DESCRIPTOR.getId(), new BrowserTaskExecutor());
    }
}
