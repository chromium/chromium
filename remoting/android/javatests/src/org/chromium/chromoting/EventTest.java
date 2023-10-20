// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.os.Handler;
import android.os.Looper;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chromoting.test.util.MutableReference;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link Event}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class EventTest {
    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testBasicScenario() {
        Event.Raisable<Void> event = new Event.Raisable<>();
        final MutableReference<Integer> callTimes = new MutableReference<Integer>(0);
        Object eventId1 =
                event.add(
                        new Event.ParameterRunnable<Void>() {
                            @Override
                            public void run(Void nil) {
                                callTimes.set(callTimes.get() + 1);
                            }
                        });
        Object eventId2 =
                event.add(
                        new Event.ParameterRunnable<Void>() {
                            @Override
                            public void run(Void nil) {
                                callTimes.set(callTimes.get() + 1);
                            }
                        });
        Object eventId3 =
                event.add(
                        new Event.ParameterRunnable<Void>() {
                            @Override
                            public void run(Void nil) {
                                // Should not reach.
                                Assert.fail();
                                callTimes.set(callTimes.get() + 1);
                            }
                        });
        Assert.assertNotNull(eventId1);
        Assert.assertNotNull(eventId2);
        Assert.assertNotNull(eventId3);
        Assert.assertTrue(event.remove(eventId3));
        for (int i = 0; i < 100; i++) {
            Assert.assertEquals(event.raise(null), 2);
            Assert.assertEquals(callTimes.get().intValue(), (i + 1) << 1);
        }
        Assert.assertTrue(event.remove(eventId1));
        Assert.assertTrue(event.remove(eventId2));
        Assert.assertFalse(event.remove(eventId3));
    }

    private static class MultithreadingTestRunner extends Thread {
        private final Event.Raisable<Void> mEvent;
        private final MutableReference<Boolean> mError;

        public MultithreadingTestRunner(
                Event.Raisable<Void> event, MutableReference<Boolean> error) {
            Preconditions.notNull(event);
            Preconditions.notNull(error);
            mEvent = event;
            mError = error;
        }

        @Override
        public void run() {
            for (int i = 0; i < 100; i++) {
                final MutableReference<Boolean> called = new MutableReference<>();
                Object id =
                        mEvent.add(
                                new Event.ParameterRunnable<Void>() {
                                    @Override
                                    public void run(Void nil) {
                                        called.set(true);
                                    }
                                });
                if (id == null) {
                    mError.set(true);
                }
                for (int j = 0; j < 100; j++) {
                    called.set(false);
                    if (mEvent.raise(null) <= 0) {
                        mError.set(true);
                    }
                    if (!called.get()) {
                        mError.set(true);
                    }
                }
                if (!mEvent.remove(id)) {
                    mError.set(true);
                }
            }
        }
    }

    @Test
    @MediumTest
    @Feature({"Chromoting"})
    public void testMultithreading() {
        Event.Raisable<Void> event = new Event.Raisable<>();
        final int threadCount = 10;
        final MutableReference<Boolean> error = new MutableReference<>();
        Thread[] threads = new Thread[threadCount];
        for (int i = 0; i < threadCount; i++) {
            threads[i] = new MultithreadingTestRunner(event, error);
            threads[i].start();
        }
        for (int i = 0; i < threadCount; i++) {
            try {
                threads[i].join();
            } catch (InterruptedException exception) {
                Assert.fail();
            }
        }
        Assert.assertNull(error.get());
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testSelfRemovable() {
        Event.Raisable<Void> event = new Event.Raisable<>();
        final MutableReference<Boolean> called = new MutableReference<>();
        final MutableReference<Boolean> nextReturn = new MutableReference<>();
        nextReturn.set(true);
        event.addSelfRemovable(
                new Event.ParameterCallback<Boolean, Void>() {
                    @Override
                    public Boolean run(Void nil) {
                        called.set(true);
                        return nextReturn.get();
                    }
                });
        Assert.assertEquals(event.raise(null), 1);
        Assert.assertTrue(called.get());
        Assert.assertFalse(event.isEmpty());
        called.set(false);
        nextReturn.set(false);
        Assert.assertEquals(event.raise(null), 1);
        Assert.assertTrue(called.get());
        Assert.assertTrue(event.isEmpty());
        called.set(false);
        nextReturn.set(true);
        Assert.assertEquals(event.raise(null), 0);
        Assert.assertFalse(called.get());
        Assert.assertTrue(event.isEmpty());
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testPromisedEvent() {
        Thread thread =
                new Thread(
                        new Runnable() {
                            @Override
                            public void run() {
                                Assert.assertNull(Looper.myLooper());
                                Event.Raisable<Object> event = new Event.PromisedRaisable<>();
                                final List<Object> called1 = new ArrayList<>();
                                final List<Object> called2 = new ArrayList<>();
                                final List<Object> called3 = new ArrayList<>();
                                final List<Object> called4 = new ArrayList<>();
                                final List<Object> parameters = new ArrayList<>();
                                event.add(
                                        new Event.ParameterRunnable<Object>() {
                                            @Override
                                            public void run(Object obj) {
                                                called1.add(obj);
                                            }
                                        });
                                Object parameter = new Object();
                                event.raise(parameter);
                                parameters.add(parameter);
                                event.add(
                                        new Event.ParameterRunnable<Object>() {
                                            @Override
                                            public void run(Object obj) {
                                                called2.add(obj);
                                            }
                                        });
                                parameter = new Object();
                                event.raise(parameter);
                                parameters.add(parameter);
                                event.add(
                                        new Event.ParameterRunnable<Object>() {
                                            @Override
                                            public void run(Object obj) {
                                                called3.add(obj);
                                            }
                                        });
                                parameter = new Object();
                                event.raise(parameter);
                                parameters.add(parameter);
                                event.add(
                                        new Event.ParameterRunnable<Object>() {
                                            @Override
                                            public void run(Object obj) {
                                                called4.add(obj);
                                            }
                                        });

                                Assert.assertEquals(called1.size(), 3);
                                Assert.assertEquals(called2.size(), 3);
                                Assert.assertEquals(called3.size(), 2);
                                Assert.assertEquals(called4.size(), 1);

                                for (int i = 0; i < 3; i++) {
                                    Assert.assertTrue(called1.get(i) == parameters.get(i));
                                    Assert.assertTrue(called2.get(i) == parameters.get(i));
                                }
                                for (int i = 0; i < 2; i++) {
                                    Assert.assertTrue(called3.get(i) == parameters.get(i + 1));
                                }
                                Assert.assertTrue(called4.get(0) == parameters.get(2));
                            }
                        });
        thread.setUncaughtExceptionHandler(
                new Thread.UncaughtExceptionHandler() {
                    @Override
                    public void uncaughtException(Thread t, Throwable e) {
                        // Forward exceptions from test thread.
                        Assert.assertFalse(true);
                    }
                });
        thread.start();
        try {
            thread.join();
        } catch (InterruptedException ex) {
        }
    }

    @Test
    @SmallTest
    @Feature({"Chromoting"})
    public void testPromisedEventWithLooper() {
        Looper.prepare();
        Assert.assertNotNull(Looper.myLooper());
        Event.Raisable<Object> event = new Event.PromisedRaisable<>();
        final List<Object> called1 = new ArrayList<>();
        final List<Object> called2 = new ArrayList<>();
        final List<Object> called3 = new ArrayList<>();
        final List<Object> called4 = new ArrayList<>();
        final List<Object> parameters = new ArrayList<>();
        event.add(
                new Event.ParameterRunnable<Object>() {
                    @Override
                    public void run(Object obj) {
                        called1.add(obj);
                    }
                });
        Object parameter = new Object();
        event.raise(parameter);
        parameters.add(parameter);
        event.add(
                new Event.ParameterRunnable<Object>() {
                    @Override
                    public void run(Object obj) {
                        called2.add(obj);
                    }
                });
        parameter = new Object();
        event.raise(parameter);
        parameters.add(parameter);
        event.add(
                new Event.ParameterRunnable<Object>() {
                    @Override
                    public void run(Object obj) {
                        called3.add(obj);
                    }
                });
        parameter = new Object();
        event.raise(parameter);
        parameters.add(parameter);
        event.add(
                new Event.ParameterRunnable<Object>() {
                    @Override
                    public void run(Object obj) {
                        called4.add(obj);
                    }
                });

        Handler h = new Handler(Looper.myLooper());
        h.post(
                new Runnable() {
                    @Override
                    public void run() {
                        Looper.myLooper().quit();
                    }
                });
        Looper.loop();

        Assert.assertEquals(called1.size(), 3);
        Assert.assertEquals(called2.size(), 3);
        Assert.assertEquals(called3.size(), 2);
        Assert.assertEquals(called4.size(), 1);

        for (int i = 0; i < 3; i++) {
            Assert.assertTrue(called1.get(i) == parameters.get(i));
        }
        Assert.assertTrue(called2.get(0) == parameters.get(1));
        for (int i = 0; i < 2; i++) {
            Assert.assertTrue(called2.get(i + 1) == parameters.get(2));
            Assert.assertTrue(called3.get(i) == parameters.get(2));
        }
        Assert.assertTrue(called4.get(0) == parameters.get(2));
    }
}
