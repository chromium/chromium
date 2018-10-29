// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.SingleThreadTaskRunner;
import org.chromium.base.task.SingleThreadTaskRunnerImpl;
import org.chromium.base.task.TaskExecutor;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;

import java.util.WeakHashMap;

/**
 * This {@link TaskExecutor} is for tasks posted with {@link UiThreadTaskTraits}. It maps directly
 * to content::BrowserTaskExecutor except only UI thread posting is supported from java.
 *
 * NB if you wish to post to the thread pool then use {@link TaskTraits} instead of {@link
 * UiThreadTaskTraits}.
 */
public class BrowserTaskExecutor implements TaskExecutor {
    @Override
    public TaskRunner createTaskRunner(TaskTraits taskTraits) {
        return createSingleThreadTaskRunner(taskTraits);
    }

    @Override
    public SequencedTaskRunner createSequencedTaskRunner(TaskTraits taskTraits) {
        return createSingleThreadTaskRunner(taskTraits);
    }

    /**
     * This maps to a single thread within the native thread pool. Due to that contract we
     * can't run tasks posted on it until native has started.
     */
    @Override
    public SingleThreadTaskRunner createSingleThreadTaskRunner(TaskTraits taskTraits) {
        synchronized (mTaskRunners) {
            SingleThreadTaskRunner taskRunner = mTaskRunners.get(taskTraits);
            if (taskRunner != null) return taskRunner;

            // TODO(alexclarke): ThreadUtils.getUiThreadHandler shouldn't be in base.
            taskRunner =
                    new SingleThreadTaskRunnerImpl(ThreadUtils.getUiThreadHandler(), taskTraits);
            mTaskRunners.put(taskTraits, taskRunner);
            return taskRunner;
        }
    }

    @Override
    public void postTask(TaskTraits taskTraits, Runnable task) {
        createSingleThreadTaskRunner(taskTraits).postTask(task);
    }

    public static void register() {
        // In some tests we will get called multiple times.
        if (sRegistered) return;

        PostTask.registerTaskExecutor(UiThreadTaskTraits.EXTENSION_ID, new BrowserTaskExecutor());
        sRegistered = true;
    }

    private final WeakHashMap<TaskTraits, SingleThreadTaskRunner> mTaskRunners =
            new WeakHashMap<>();

    private static boolean sRegistered;
}
