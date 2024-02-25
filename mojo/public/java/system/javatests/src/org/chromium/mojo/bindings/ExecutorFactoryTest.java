// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo.system.impl.CoreImpl;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BrokenBarrierException;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Testing the executor factory. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ExecutorFactoryTest {
    @Rule public MojoTestRule mTestRule = new MojoTestRule();

    private static final long RUN_LOOP_TIMEOUT_MS = 50;
    private static final int CONCURRENCY_LEVEL = 5;
    private static final ExecutorService WORKERS = Executors.newFixedThreadPool(CONCURRENCY_LEVEL);

    private Executor mExecutor;
    private List<Thread> mThreadContainer;

    /**
     * @see MojoTestCase#setUp()
     */
    @Before
    public void setUp() {
        mExecutor = ExecutorFactory.getExecutorForCurrentThread(CoreImpl.getInstance());
        mThreadContainer = new ArrayList<Thread>();
    }

    /** Testing the {@link Executor} when called from the executor thread. */
    @Test
    @SmallTest
    public void testExecutorOnCurrentThread() {
        Runnable action =
                new Runnable() {
                    @Override
                    public void run() {
                        mThreadContainer.add(Thread.currentThread());
                    }
                };
        mExecutor.execute(action);
        mExecutor.execute(action);
        Assert.assertEquals(0, mThreadContainer.size());
        mTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        Assert.assertEquals(2, mThreadContainer.size());
        for (Thread thread : mThreadContainer) {
            Assert.assertEquals(Thread.currentThread(), thread);
        }
    }

    /** Testing the {@link Executor} when called from another thread. */
    @Test
    @SmallTest
    public void testExecutorOnOtherThread() {
        final CyclicBarrier barrier = new CyclicBarrier(CONCURRENCY_LEVEL + 1);
        for (int i = 0; i < CONCURRENCY_LEVEL; ++i) {
            WORKERS.execute(
                    new Runnable() {
                        @Override
                        public void run() {
                            mExecutor.execute(
                                    new Runnable() {

                                        @Override
                                        public void run() {
                                            mThreadContainer.add(Thread.currentThread());
                                        }
                                    });
                            try {
                                barrier.await();
                            } catch (InterruptedException e) {
                                Assert.fail("Unexpected exception: " + e.getMessage());
                            } catch (BrokenBarrierException e) {
                                Assert.fail("Unexpected exception: " + e.getMessage());
                            }
                        }
                    });
        }
        try {
            barrier.await();
        } catch (InterruptedException e) {
            Assert.fail("Unexpected exception: " + e.getMessage());
        } catch (BrokenBarrierException e) {
            Assert.fail("Unexpected exception: " + e.getMessage());
        }
        Assert.assertEquals(0, mThreadContainer.size());
        mTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        Assert.assertEquals(CONCURRENCY_LEVEL, mThreadContainer.size());
        for (Thread thread : mThreadContainer) {
            Assert.assertEquals(Thread.currentThread(), thread);
        }
    }
}
