// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.scheduler;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import android.os.Handler;
import android.os.HandlerThread;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SingleThreadTaskRunner;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;
import org.chromium.content.app.ContentMain;
import org.chromium.content_public.browser.BrowserTaskExecutor;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.UiThreadSchedulerTestUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test class for scheduling on the UI Thread.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class UiThreadSchedulerTest {
    @Rule
    public NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() {
        // We don't load the browser process since we want tests to control when content
        // is started and hence the native secheduler is ready.
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();
        ThreadUtils.setUiThread(null);
        ThreadUtils.setWillOverrideUiThread(true);
        mUiThread = new HandlerThread("UiThreadForTest");
        mUiThread.start();
        ThreadUtils.setUiThread(mUiThread.getLooper());
        BrowserTaskExecutor.register();
        BrowserTaskExecutor.setShouldPrioritizeBootstrapTasks(true);
        mHandler = new Handler(mUiThread.getLooper());
        UiThreadSchedulerTestUtils.postBrowserMainLoopStartupTasks(false);
    }

    @After
    public void tearDown() {
        UiThreadSchedulerTestUtils.postBrowserMainLoopStartupTasks(true);
        mUiThread.quitSafely();
        ThreadUtils.setUiThread(null);
        ThreadUtils.setWillOverrideUiThread(false);
    }

    @Test
    @MediumTest
    public void testSimpleUiThreadPostingBeforeNativeLoaded() {
        TaskRunner uiThreadTaskRunner =
                PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.DEFAULT);
        try {
            List<Integer> orderList = new ArrayList<>();
            SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 1);
            SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 2);
            SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 3);
            SchedulerTestHelpers.postTaskAndBlockUntilRun(uiThreadTaskRunner);

            assertThat(orderList, contains(1, 2, 3));
        } finally {
            uiThreadTaskRunner.destroy();
        }
    }

    @Test
    @MediumTest
    public void testPrioritizationBeforeNativeLoaded() {
        TaskRunner defaultTaskRunner =
                PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.DEFAULT);
        TaskRunner bootstrapTaskRunner =
                PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.BOOTSTRAP);
        try {
            List<Integer> orderList = new ArrayList<>();
            // We want to enqueue these tasks atomically but we're not on the mUiThread. So
            // we post a task to enqueue them.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    SchedulerTestHelpers.postRecordOrderTask(defaultTaskRunner, orderList, 1);
                    SchedulerTestHelpers.postRecordOrderTask(defaultTaskRunner, orderList, 2);
                    SchedulerTestHelpers.postRecordOrderTask(defaultTaskRunner, orderList, 3);
                    SchedulerTestHelpers.postRecordOrderTask(bootstrapTaskRunner, orderList, 10);
                    SchedulerTestHelpers.postRecordOrderTask(bootstrapTaskRunner, orderList, 20);
                    SchedulerTestHelpers.postRecordOrderTask(bootstrapTaskRunner, orderList, 30);
                }
            });

            SchedulerTestHelpers.preNativeRunUntilIdle(mUiThread);
            assertThat(orderList, contains(10, 20, 30, 1, 2, 3));
        } finally {
            defaultTaskRunner.destroy();
            bootstrapTaskRunner.destroy();
        }
    }

    @Test
    @MediumTest
    public void testUiThreadTaskRunnerMigrationToNative() {
        TaskRunner uiThreadTaskRunner =
                PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.DEFAULT);
        try {
            List<Integer> orderList = new ArrayList<>();
            SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 1);

            postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(
                    uiThreadTaskRunner, new Runnable() {
                        @Override
                        public void run() {
                            orderList.add(2);
                        }
                    });
            assertThat(orderList, contains(1, 2));
        } finally {
            uiThreadTaskRunner.destroy();
        }
    }

    @Test
    @MediumTest
    public void testSimpleUiThreadPostingAfterNativeLoaded() {
        TaskRunner uiThreadTaskRunner =
                PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.DEFAULT);
        try {
            startContentMainOnUiThread();

            uiThreadTaskRunner.postTask(new Runnable() {
                @Override
                public void run() {
                    Assert.assertTrue(ThreadUtils.runningOnUiThread());
                }
            });
            SchedulerTestHelpers.postTaskAndBlockUntilRun(uiThreadTaskRunner);
        } finally {
            uiThreadTaskRunner.destroy();
        }
    }

    @Test
    @MediumTest
    public void testPostTaskCurrentThreadBeforeNativeLoaded() throws Exception {
        // This should not timeout.
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                PostTask.postTask(TaskTraits.CURRENT_THREAD, new Runnable() {
                    @Override
                    public void run() {
                        synchronized (lock) {
                            taskExecuted.set(true);
                            lock.notify();
                        }
                    }
                });
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
    public void testPostTaskCurrentThreadAfterNativeLoaded() throws Exception {
        startContentMainOnUiThread();

        // This should not timeout.
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                PostTask.postTask(TaskTraits.CURRENT_THREAD, new Runnable() {
                    @Override
                    public void run() {
                        synchronized (lock) {
                            taskExecuted.set(true);
                            lock.notify();
                        }
                    }
                });
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
    public void testPostTaskCurrentThreadInThreadpoolAfterNativeLoaded() throws Exception {
        startContentMainOnUiThread();
        SingleThreadTaskRunner threadpoolTaskRunner =
                PostTask.createSingleThreadTaskRunner(TaskTraits.THREAD_POOL_USER_BLOCKING);
        SingleThreadTaskRunner uiThreadTaskRunner =
                PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.DEFAULT);

        // This should not timeout.
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        threadpoolTaskRunner.postTask(new Runnable() {
            @Override
            public void run() {
                PostTask.postTask(TaskTraits.CURRENT_THREAD, new Runnable() {
                    @Override
                    public void run() {
                        synchronized (lock) {
                            Assert.assertTrue(threadpoolTaskRunner.belongsToCurrentThread());
                            Assert.assertFalse(uiThreadTaskRunner.belongsToCurrentThread());
                            taskExecuted.set(true);
                            lock.notify();
                        }
                    }
                });
            }
        });

        synchronized (lock) {
            while (!taskExecuted.get()) {
                lock.wait();
            }
        }

        uiThreadTaskRunner.destroy();
        threadpoolTaskRunner.destroy();
    }

    @Test
    @MediumTest
    public void testTaskNotRunOnUiThreadWithoutUiThreadTaskTraits() {
        TaskRunner uiThreadTaskRunner =
                PostTask.createSingleThreadTaskRunner(TaskTraits.USER_BLOCKING);
        try {
            // Test times out without this.
            UiThreadSchedulerTestUtils.postBrowserMainLoopStartupTasks(true);
            startContentMainOnUiThread();

            uiThreadTaskRunner.postTask(new Runnable() {
                @Override
                public void run() {
                    Assert.assertFalse(ThreadUtils.runningOnUiThread());
                }
            });

            SchedulerTestHelpers.postTaskAndBlockUntilRun(uiThreadTaskRunner);
        } finally {
            uiThreadTaskRunner.destroy();
        }
    }

    @Test
    @MediumTest
    public void testRunOrPostTask() throws InterruptedException {
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        List<Integer> orderList = new ArrayList<>();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            // We are running on the UI thread now. First, we post a task on the
            // UI thread; it will not run immediately because the UI thread is
            // busy running the current code:
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                orderList.add(1);
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notify();
                }
            });
            // Now, we runOrPost a task on the UI thread. We are on the UI thread,
            // so it will run immediately.
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> { orderList.add(2); });
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
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();

        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT, () -> {
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

        mHandler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    ContentMain.start(/* startServiceManagerOnly */ true);
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
