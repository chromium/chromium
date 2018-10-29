// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.scheduler;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import android.annotation.TargetApi;
import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;
import org.chromium.base.test.task.TaskSchedulerTestHelpers;
import org.chromium.base.test.util.MinAndroidSdkLevel;
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
@MinAndroidSdkLevel(23)
@TargetApi(Build.VERSION_CODES.M)
public class NativePostTaskTest {
    @Rule
    public NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @After
    public void tearDown() {
        TaskSchedulerTestHelpers.disableTaskSchedulerExecutionForTesting();
    }

    @Test
    @MediumTest
    public void testNativePostTask() throws Exception {
        startNativeScheduler();

        // This should not timeout.
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        PostTask.postTask(new TaskTraits(), new Runnable() {
            @Override
            public void run() {
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notify();
                }
            }
        });
        synchronized (lock) {
            try {
                while (!taskExecuted.get()) {
                    lock.wait();
                }
            } catch (InterruptedException ie) {
                ie.printStackTrace();
            }
        }
    }

    @Test
    @MediumTest
    public void testCreateTaskRunner() throws Exception {
        startNativeScheduler();
        TaskRunner taskQueue = PostTask.createTaskRunner(new TaskTraits());
        // This should not time out.
        SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);
    }

    @Test
    @MediumTest
    public void testCreateSequencedTaskRunner() throws Exception {
        startNativeScheduler();
        TaskRunner taskQueue = PostTask.createSequencedTaskRunner(new TaskTraits());
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 2);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 3);
        SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);

        assertThat(orderList, contains(1, 2, 3));
    }

    @Test
    @MediumTest
    public void testCreateSingleThreadSequencedTaskRunner() throws Exception {
        startNativeScheduler();
        TaskRunner taskQueue = PostTask.createSingleThreadTaskRunner(new TaskTraits());
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 2);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 3);
        SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);

        assertThat(orderList, contains(1, 2, 3));
    }

    @Test
    @MediumTest
    public void testCreateTaskRunnerMigrationToNative() throws Exception {
        TaskRunner taskQueue = PostTask.createTaskRunner(new TaskTraits());
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);

        postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(taskQueue, new Runnable() {
            @Override
            public void run() {
                orderList.add(2);
            }
        });

        assertThat(orderList, contains(1, 2));
    }

    @Test
    @MediumTest
    public void testCreateSequencedTaskRunnerMigrationToNative() throws Exception {
        TaskRunner taskQueue = PostTask.createSequencedTaskRunner(new TaskTraits());
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);

        postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(taskQueue, new Runnable() {
            @Override
            public void run() {
                orderList.add(2);
            }
        });

        assertThat(orderList, contains(1, 2));
    }

    @Test
    @MediumTest
    public void testCreateSingleThreadSequencedTaskRunnerMigrationToNative() throws Exception {
        TaskRunner taskQueue = PostTask.createSingleThreadTaskRunner(new TaskTraits());
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);

        postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(taskQueue, new Runnable() {
            @Override
            public void run() {
                orderList.add(2);
            }
        });

        assertThat(orderList, contains(1, 2));
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
            try {
                while (!taskRun.get()) {
                    lock.wait();
                }
            } catch (InterruptedException ie) {
                ie.printStackTrace();
            }
        }
    }

    private void startNativeScheduler() throws Exception {
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();
        ContentMain.start(/* startServiceManagerOnly */ true);
        TaskSchedulerTestHelpers.enableTaskSchedulerExecutionForTesting();
    }
}
