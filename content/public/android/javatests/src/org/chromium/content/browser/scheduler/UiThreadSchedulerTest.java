// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.scheduler;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import android.os.Handler;
import android.os.HandlerThread;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;
import org.chromium.content.app.ContentMain;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.UiThreadSchedulerTestUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Test class for scheduling on the UI Thread. */
@RunWith(BaseJUnit4ClassRunner.class)
public class UiThreadSchedulerTest {
    @Before
    public void setUp() {
        ThreadUtils.setWillOverrideUiThread();
        mUiThread = new HandlerThread("UiThreadForTest");
        mUiThread.start();
        ThreadUtils.setUiThread(mUiThread.getLooper());
        mHandler = new Handler(mUiThread.getLooper());
        // We don't load the browser process since we want tests to control when content
        // is started and hence the native secheduler is ready.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        UiThreadSchedulerTestUtils.postBrowserMainLoopStartupTasks(false);
    }

    @After
    public void tearDown() {
        UiThreadSchedulerTestUtils.postBrowserMainLoopStartupTasks(true);
        mUiThread.quitSafely();
    }

    @Test
    @MediumTest
    public void testSimpleUiThreadPostingBeforeNativeLoaded() {
        TaskRunner uiThreadTaskRunner = PostTask.createTaskRunner(TaskTraits.UI_DEFAULT);
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 1);
        SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 2);
        SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 3);
        SchedulerTestHelpers.postTaskAndBlockUntilRun(uiThreadTaskRunner);

        assertThat(orderList, contains(1, 2, 3));
    }

    @Test
    @MediumTest
    public void testUiThreadTaskRunnerMigrationToNative() {
        TaskRunner uiThreadTaskRunner = PostTask.createTaskRunner(TaskTraits.UI_DEFAULT);
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 1);

        postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(
                uiThreadTaskRunner,
                new Runnable() {
                    @Override
                    public void run() {
                        orderList.add(2);
                    }
                });
        assertThat(orderList, contains(1, 2));
    }

    @Test
    @MediumTest
    public void testSimpleUiThreadPostingAfterNativeLoaded() {
        TaskRunner uiThreadTaskRunner = PostTask.createTaskRunner(TaskTraits.UI_DEFAULT);
        startContentMainOnUiThread();

        uiThreadTaskRunner.execute(
                new Runnable() {
                    @Override
                    public void run() {
                        Assert.assertTrue(ThreadUtils.runningOnUiThread());
                    }
                });
        SchedulerTestHelpers.postTaskAndBlockUntilRun(uiThreadTaskRunner);
    }

    @Test
    @MediumTest
    public void testTaskNotRunOnUiThreadWithoutUiThreadTaskTraits() {
        TaskRunner uiThreadTaskRunner = PostTask.createTaskRunner(TaskTraits.USER_BLOCKING);
        // Test times out without this.
        UiThreadSchedulerTestUtils.postBrowserMainLoopStartupTasks(true);
        startContentMainOnUiThread();

        uiThreadTaskRunner.execute(
                new Runnable() {
                    @Override
                    public void run() {
                        Assert.assertFalse(ThreadUtils.runningOnUiThread());
                    }
                });

        SchedulerTestHelpers.postTaskAndBlockUntilRun(uiThreadTaskRunner);
    }

    @Test
    @MediumTest
    public void testRunOrPostTask() throws InterruptedException {
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        List<Integer> orderList = new ArrayList<>();
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // We are running on the UI thread now. First, we post a task on the
                    // UI thread; it will not run immediately because the UI thread is
                    // busy running the current code:
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                orderList.add(1);
                                synchronized (lock) {
                                    taskExecuted.set(true);
                                    lock.notify();
                                }
                            });
                    // Now, we runOrPost a task on the UI thread. We are on the UI thread,
                    // so it will run immediately.
                    PostTask.runOrPostTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                orderList.add(2);
                            });
                });
        synchronized (lock) {
            while (!taskExecuted.get()) {
                lock.wait();
            }
        }
        assertThat(orderList, contains(2, 1));
    }

    @Test
    @MediumTest
    public void testRunSynchronously() {
        final AtomicBoolean taskExecuted = new AtomicBoolean();

        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException ie) {
                        ie.printStackTrace();
                    }
                    taskExecuted.set(true);
                });
        // We verify that the current execution waited until the synchronous task completed.
        Assert.assertTrue(taskExecuted.get());
    }

    private void startContentMainOnUiThread() {
        final Object lock = new Object();
        final AtomicBoolean uiThreadInitalized = new AtomicBoolean();

        mHandler.post(
                new Runnable() {
                    @Override
                    public void run() {
                        try {
                            ContentMain.start(/* startMinimalBrowser= */ true);
                            synchronized (lock) {
                                uiThreadInitalized.set(true);
                                lock.notify();
                            }
                        } catch (Exception e) {
                        }
                    }
                });

        synchronized (lock) {
            try {
                while (!uiThreadInitalized.get()) {
                    lock.wait();
                }
            } catch (InterruptedException ie) {
                ie.printStackTrace();
            }
        }
    }

    private void postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(
            TaskRunner taskQueue, Runnable taskToRunAfterNativeSchedulerLoaded) {
        final Object lock = new Object();
        final AtomicBoolean taskRun = new AtomicBoolean();
        final AtomicBoolean nativeSchedulerStarted = new AtomicBoolean();

        // Post a task that reposts itself until nativeSchedulerStarted is set to true.  This tests
        // that tasks posted before the native library is loaded still run afterwards.
        taskQueue.execute(
                new Runnable() {
                    @Override
                    public void run() {
                        if (nativeSchedulerStarted.compareAndSet(true, true)) {
                            taskToRunAfterNativeSchedulerLoaded.run();
                            synchronized (lock) {
                                taskRun.set(true);
                                lock.notify();
                            }
                        } else {
                            taskQueue.execute(this);
                        }
                    }
                });

        startContentMainOnUiThread();
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

    private Handler mHandler;
    private HandlerThread mUiThread;
}
