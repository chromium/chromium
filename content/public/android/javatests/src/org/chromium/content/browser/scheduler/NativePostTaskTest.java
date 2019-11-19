// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.scheduler;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;
import org.chromium.base.test.task.ThreadPoolTestHelpers;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content.app.ContentMain;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test class for {@link PostTask}.
 *
 * Due to layering concerns we can't test native backed task posting in base, so we do it here
 * instead.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class NativePostTaskTest {
    @Rule
    public NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @After
    public void tearDown() {
        ThreadPoolTestHelpers.disableThreadPoolExecutionForTesting();
    }

    @Test
    @MediumTest
    public void testNativePostTask() throws Exception {
        startNativeScheduler();

        // This should not timeout.
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        PostTask.postTask(TaskTraits.USER_BLOCKING, new Runnable() {
            @Override
            public void run() {
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notify();
                }
            }
        });

        synchronized (lock) {
            while (!taskExecuted.get()) {
                lock.wait();
            }
        }
    }

    @Test
    @MediumTest
    public void testNativePostDelayedTask() throws Exception {
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        PostTask.postDelayedTask(TaskTraits.USER_BLOCKING, () -> {
            synchronized (lock) {
                taskExecuted.set(true);
                lock.notify();
            }
        }, 1);

        // We verify that the task didn't get scheduled before the native scheduler is initialised
        Assert.assertFalse(taskExecuted.get());
        startNativeScheduler();

        // The task should now be scheduled at some point after the delay, so the test shouldn't
        // time out.
        synchronized (lock) {
            while (!taskExecuted.get()) {
                lock.wait();
            }
        }
    }

    @Test
    @MediumTest
    public void testCreateTaskRunner() {
        startNativeScheduler();
        TaskRunner taskQueue = PostTask.createTaskRunner(TaskTraits.USER_BLOCKING);
        // This should not time out.
        try {
            SchedulerTestHelpers.postDelayedTaskAndBlockUntilRun(taskQueue, 1);
        } finally {
            taskQueue.destroy();
        }
    }

    private void testRunningTasksInSequence(TaskRunner taskQueue) {
        try {
            List<Integer> orderListImmediate = new ArrayList<>();
            List<Integer> orderListDelayed = new ArrayList<>();

            SchedulerTestHelpers.postThreeTasksInOrder(taskQueue, orderListImmediate);
            SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);

            assertThat(orderListImmediate, contains(1, 2, 3));

            SchedulerTestHelpers.postThreeDelayedTasksInOrder(taskQueue, orderListDelayed);
            SchedulerTestHelpers.postDelayedTaskAndBlockUntilRun(taskQueue, 1);

            assertThat(orderListDelayed, contains(1, 2, 3));
        } finally {
            taskQueue.destroy();
        }
    }

    @Test
    @MediumTest
    public void testCreateSequencedTaskRunner() {
        startNativeScheduler();
        TaskRunner taskQueue = PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING);
        testRunningTasksInSequence(taskQueue);
    }

    @Test
    @MediumTest
    public void testCreateSingleThreadSequencedTaskRunner() {
        startNativeScheduler();
        TaskRunner taskQueue = PostTask.createSingleThreadTaskRunner(TaskTraits.USER_BLOCKING);
        testRunningTasksInSequence(taskQueue);
    }

    private void performSequencedTestSchedulerMigration(TaskRunner taskQueue,
            List<Integer> orderListImmediate, List<Integer> orderListDelayed) throws Exception {
        SchedulerTestHelpers.postThreeTasksInOrder(taskQueue, orderListImmediate);
        SchedulerTestHelpers.postThreeDelayedTasksInOrder(taskQueue, orderListDelayed);

        postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(taskQueue, new Runnable() {
            @Override
            public void run() {
                orderListImmediate.add(4);
            }
        });
        // We wait until all the delayed tasks have been scheduled.
        SchedulerTestHelpers.postDelayedTaskAndBlockUntilRun(taskQueue, 1);
    }

    @Test
    @MediumTest
    @DisabledTest
    public void testCreateTaskRunnerMigrationToNative() throws Exception {
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        TaskRunner taskQueue = PostTask.createTaskRunner(TaskTraits.USER_BLOCKING);

        postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(taskQueue, new Runnable() {
            @Override
            public void run() {
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notify();
                }
            }
        });

        try {
            // The task should run at some point after the migration, so the test shouldn't
            // time out.
            synchronized (lock) {
                while (!taskExecuted.get()) {
                    lock.wait();
                }
            }
        } finally {
            taskQueue.destroy();
        }
    }

    @Test
    @MediumTest
    public void testCreateSequencedTaskRunnerMigrationToNative() throws Exception {
        List<Integer> orderListImmediate = new ArrayList<>();
        List<Integer> orderListDelayed = new ArrayList<>();
        TaskRunner taskQueue = PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING);
        try {
            performSequencedTestSchedulerMigration(taskQueue, orderListImmediate, orderListDelayed);
        } finally {
            taskQueue.destroy();
        }

        assertThat(orderListImmediate, contains(1, 2, 3, 4));
        assertThat(orderListDelayed, contains(1, 2, 3));
    }

    @Test
    @MediumTest
    public void testCreateSingleThreadSequencedTaskRunnerMigrationToNative() throws Exception {
        List<Integer> orderListImmediate = new ArrayList<>();
        List<Integer> orderListDelayed = new ArrayList<>();
        TaskRunner taskQueue = PostTask.createSingleThreadTaskRunner(TaskTraits.USER_BLOCKING);
        try {
            performSequencedTestSchedulerMigration(taskQueue, orderListImmediate, orderListDelayed);
        } finally {
            taskQueue.destroy();
        }

        assertThat(orderListImmediate, contains(1, 2, 3, 4));
        assertThat(orderListDelayed, contains(1, 2, 3));
    }

    private void postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(
            TaskRunner taskQueue, Runnable taskToRunAfterNativeSchedulerLoaded) throws Exception {
        final Object lock = new Object();
        final AtomicBoolean taskRun = new AtomicBoolean();
        final AtomicBoolean nativeSchedulerStarted = new AtomicBoolean();

        // Post a task that reposts itself until nativeSchedulerStarted is set to true.  This tests
        // that tasks posted before the native library is loaded still run afterwards.
        taskQueue.postTask(new Runnable() {
            @Override
            public void run() {
                if (nativeSchedulerStarted.compareAndSet(true, true)) {
                    taskToRunAfterNativeSchedulerLoaded.run();
                    synchronized (lock) {
                        taskRun.set(true);
                        lock.notify();
                    }
                } else {
                    taskQueue.postTask(this);
                }
            }
        });

        startNativeScheduler();
        nativeSchedulerStarted.set(true);

        synchronized (lock) {
            while (!taskRun.get()) {
                lock.wait();
            }
        }
    }

    private void startNativeScheduler() {
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();
        ContentMain.start(/* startServiceManagerOnly */ false);
        ThreadPoolTestHelpers.enableThreadPoolExecutionForTesting();
    }
}
