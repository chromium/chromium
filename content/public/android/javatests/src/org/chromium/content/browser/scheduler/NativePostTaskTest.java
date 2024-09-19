// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.scheduler;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PowerMonitor;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.AsyncTask.Status;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;
import org.chromium.base.test.task.ThreadPoolTestHelpers;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.content.app.ContentMain;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CancellationException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test class for {@link PostTask}.
 *
 * <p>Due to layering concerns we can't test native backed task posting in base, so we do it here
 * instead.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class NativePostTaskTest {
    private static class BlockedTask extends BackgroundOnlyAsyncTask<Integer> {
        private Object mStartLock = new Object();
        private AtomicInteger mValue = new AtomicInteger(0);
        private AtomicBoolean mStarted = new AtomicBoolean(false);
        private Thread mBackgroundThread;

        @Override
        protected Integer doInBackground() {
            synchronized (mStartLock) {
                mBackgroundThread = Thread.currentThread();
                mStarted.set(true);
                mStartLock.notify();
            }
            while (mValue.get() == 0) {
                // Busy wait because interrupting waiting on a lock or sleeping will clear the
                // interrupt.
            }
            return mValue.get();
        }

        public void setValue(int value) {
            mValue.set(value);
        }

        public void blockUntilDoInBackgroundStarts() throws Exception {
            synchronized (mStartLock) {
                while (!mStarted.get()) {
                    mStartLock.wait();
                }
            }
        }

        public Thread getBackgroundThread() {
            return mBackgroundThread;
        }
    }

    private static boolean sNativeLoaded;
    private boolean mFenceCreated;

    @After
    public void tearDown() {
        if (mFenceCreated) {
            ThreadPoolTestHelpers.disableThreadPoolExecutionForTesting();
        }
    }

    @Test
    @MediumTest
    public void testNativePostTask() throws Exception {
        startNativeScheduler();

        // This should not timeout.
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        PostTask.postTask(
                TaskTraits.USER_BLOCKING,
                new Runnable() {
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
    @RequiresRestart
    public void testNativePostDelayedTask() throws Exception {
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        PostTask.postDelayedTask(
                TaskTraits.USER_BLOCKING,
                () -> {
                    synchronized (lock) {
                        taskExecuted.set(true);
                        lock.notify();
                    }
                },
                1);

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
        SchedulerTestHelpers.postDelayedTaskAndBlockUntilRun(taskQueue, 1);
    }

    private void testRunningTasksInSequence(TaskRunner taskQueue) {
        List<Integer> orderListImmediate = new ArrayList<>();
        List<Integer> orderListDelayed = new ArrayList<>();

        SchedulerTestHelpers.postThreeTasksInOrder(taskQueue, orderListImmediate);
        SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);

        assertThat(orderListImmediate, contains(1, 2, 3));

        SchedulerTestHelpers.postThreeDelayedTasksInOrder(taskQueue, orderListDelayed);
        SchedulerTestHelpers.postDelayedTaskAndBlockUntilRun(taskQueue, 1);

        assertThat(orderListDelayed, contains(1, 2, 3));
    }

    @Test
    @MediumTest
    public void testCreateSequencedTaskRunner() {
        startNativeScheduler();
        TaskRunner taskQueue = PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING);
        testRunningTasksInSequence(taskQueue);
    }

    private void performSequencedTestSchedulerMigration(
            TaskRunner taskQueue, List<Integer> orderListImmediate, List<Integer> orderListDelayed)
            throws Exception {
        SchedulerTestHelpers.postThreeTasksInOrder(taskQueue, orderListImmediate);
        SchedulerTestHelpers.postThreeDelayedTasksInOrder(taskQueue, orderListDelayed);

        postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(
                taskQueue,
                new Runnable() {
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
    @DisabledTest(message = "https://crbug.com/938316")
    public void testCreateTaskRunnerMigrationToNative() throws Exception {
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        TaskRunner taskQueue = PostTask.createTaskRunner(TaskTraits.USER_BLOCKING);

        postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(
                taskQueue,
                new Runnable() {
                    @Override
                    public void run() {
                        synchronized (lock) {
                            taskExecuted.set(true);
                            lock.notify();
                        }
                    }
                });

        // The task should run at some point after the migration, so the test shouldn't
        // time out.
        synchronized (lock) {
            while (!taskExecuted.get()) {
                lock.wait();
            }
        }
    }

    @Test
    @MediumTest
    @RequiresRestart
    public void testCreateSequencedTaskRunnerMigrationToNative() throws Exception {
        List<Integer> orderListImmediate = new ArrayList<>();
        List<Integer> orderListDelayed = new ArrayList<>();
        TaskRunner taskQueue = PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING);
        performSequencedTestSchedulerMigration(taskQueue, orderListImmediate, orderListDelayed);

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

        startNativeScheduler();
        nativeSchedulerStarted.set(true);

        synchronized (lock) {
            while (!taskRun.get()) {
                lock.wait();
            }
        }
    }

    @Test
    @MediumTest
    public void testNativeAsyncTask() throws Exception {
        startNativeScheduler();

        BlockedTask task = new BlockedTask();

        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        task.blockUntilDoInBackgroundStarts();
        Assert.assertEquals(Status.RUNNING, task.getStatus());

        final int value = 5;
        task.setValue(value);

        Assert.assertEquals(value, task.get().intValue());

        Assert.assertFalse(task.isCancelled());
        Assert.assertEquals(Status.FINISHED, task.getStatus());

        Assert.assertFalse(task.getBackgroundThread().isInterrupted());
    }

    @Test
    @MediumTest
    public void testNativeAsyncTaskInterruptIsCleared() throws Exception {
        startNativeScheduler();

        BlockedTask task = new BlockedTask();

        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        task.blockUntilDoInBackgroundStarts();
        Assert.assertEquals(Status.RUNNING, task.getStatus());

        Assert.assertTrue(task.cancel(/* mayInterruptIfRunning= */ true));

        // get() will raise an exception although the task is started.
        try {
            task.get();
            Assert.fail();
        } catch (CancellationException e) {
            // expected
        }

        // Set a value to unblock the task.
        task.setValue(3);

        // Wait for the AsyncTask to finish.
        while (task.getStatus() != Status.FINISHED) {
            Thread.sleep(50);
        }

        // Sleep a bit longer for the FutureTask to finish.
        Thread.sleep(500);

        Assert.assertTrue(task.isCancelled());
        Assert.assertEquals(Status.FINISHED, task.getStatus());

        Assert.assertFalse(task.getBackgroundThread().isInterrupted());
    }

    private void startNativeScheduler() {
        if (!sNativeLoaded) {
            NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
            PowerMonitor.createForTests();
            ContentMain.start(/* startMinimalBrowser= */ false);
            sNativeLoaded = true;
        }
        mFenceCreated = true;
        ThreadPoolTestHelpers.enableThreadPoolExecutionForTesting();
    }
}
