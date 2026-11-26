// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.ComponentName;
import android.content.Context;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ChildBindingState;
import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.TestChildProcessConnection;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content.browser.SpareChildConnection.LifecycleEvent;
import org.chromium.content.browser.SpareChildConnection.LoweredStatus;

/** Unit tests for the SpareChildConnection class. */
@RunWith(BaseRobolectricTestRunner.class)
public class SpareChildConnectionTest {
    @Mock private ChildProcessConnection.ServiceCallback mServiceCallback;

    // A connection allocator not used to create connections.
    private final ChildConnectionAllocator mWrongConnectionAllocator =
            ChildConnectionAllocator.createFixedForTesting(
                    null,
                    "org.chromium.test",
                    "TestServiceName",
                    /* serviceCount= */ 3,
                    /* bindAsExternalService= */ false,
                    /* fallbackToNextSlot= */ false,
                    /* isSandboxedForHistograms= */ false);

    // The allocator used to allocate the actual connection.
    private ChildConnectionAllocator mConnectionAllocator;

    private static class TestConnectionFactory
            implements ChildConnectionAllocator.ConnectionFactory {
        private TestChildProcessConnection mConnection;

        @Override
        public ChildProcessConnection createConnection(
                Context context,
                ComponentName serviceName,
                ComponentName fallbackServiceName,
                boolean bindAsExternalService,
                Bundle serviceBundle,
                String instanceName,
                boolean independentFallback,
                boolean allowBindFailure,
                boolean isSandboxedForHistograms) {
            // We expect to create only one connection after calling reset().
            assertThat(mConnection).isNull();
            mConnection =
                    new TestChildProcessConnection(
                            serviceName, bindAsExternalService, serviceBundle);
            return mConnection;
        }

        public void simulateConnectionBindingSuccessfully() {
            mConnection.getServiceCallback().onChildStarted();
            RobolectricUtil.runAllBackgroundAndUi();
        }

        public void simulateConnectionDied() {
            mConnection.getServiceCallback().onChildProcessDied(mConnection);
            RobolectricUtil.runAllBackgroundAndUi();
        }

        public void simulateServiceDisconnected() {
            mConnection.simulateServiceDisconnected();
            RobolectricUtil.runAllBackgroundAndUi();
        }

        public void simulateConnectionSetUp() {
            mConnection.setPid(123);
        }

        public void reset() {
            mConnection = null;
        }
    }

    private final TestConnectionFactory mTestConnectionFactory = new TestConnectionFactory();

    private SpareChildConnection mSpareConnection;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {

        // The tests run on only one thread. Pretend that is the launcher thread so LauncherThread
        // asserts are not triggered.
        LauncherThread.setCurrentThreadAsLauncherThread();

        mConnectionAllocator =
                ChildConnectionAllocator.createFixedForTesting(
                        null,
                        "org.chromium.test.spare_connection",
                        "TestServiceName",
                        /* serviceCount= */ 5,
                        /* bindAsExternalService= */ false,
                        /* fallbackToNextSlot= */ false,
                        /* isSandboxedForHistograms= */ false);
        mConnectionAllocator.setConnectionFactoryForTesting(mTestConnectionFactory);
        mSpareConnection =
                new SpareChildConnection(
                        /* context= */ null, mConnectionAllocator, /* serviceBundle= */ null);
    }

    @After
    public void tearDown() {
        LauncherThread.setLauncherThreadAsLauncherThread();
    }

    /** Test creation and retrieval of connection. */
    @Test
    @Feature({"ProcessManagement"})
    public void testCreateAndGet() {
        // Tests retrieving the connection with the wrong allocator.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(
                        mWrongConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        assertNull(connection);

        // And with the right one.
        connection =
                mSpareConnection.getConnection(
                        mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        assertNotNull(connection);

        // The connection has been used, subsequent calls should return null.
        connection =
                mSpareConnection.getConnection(
                        mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        assertNull(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackNotCalledWhenNoConnection() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();

        // Retrieve the wrong connection, no callback should be fired.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(
                        mWrongConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        assertNull(connection);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildProcessDied(any());
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackCalledConnectionReady() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();

        assertFalse(mSpareConnection.isEmpty());

        // Now retrieve the connection, the callback should be invoked.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(
                        mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        assertNotNull(connection);

        // No more connections are available.
        assertTrue(mSpareConnection.isEmpty());

        RobolectricUtil.runAllBackgroundAndUi();
        verify(mServiceCallback, times(1)).onChildStarted();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackCalledConnectionNotReady() {
        assertFalse(mSpareConnection.isEmpty());

        // Retrieve the connection before it's bound.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(
                        mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        assertNotNull(connection);
        RobolectricUtil.runAllBackgroundAndUi();
        // No callbacks are called.
        verify(mServiceCallback, times(0)).onChildStarted();

        // No more connections are available.
        assertTrue(mSpareConnection.isEmpty());

        // Simulate the connection getting bound, it should trigger the callback.
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();
        verify(mServiceCallback, times(1)).onChildStarted();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRetrievedConnectionStops() {
        // Retrieve the spare connection before it's bound.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(
                        mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        assertNotNull(connection);

        mTestConnectionFactory.simulateConnectionDied();

        // We should get a failure callback.
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testConnectionFreeing() {
        // Simulate the connection dying.
        mTestConnectionFactory.simulateConnectionDied();

        // Connection should be gone.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(
                        mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        assertNull(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testInitialBindingState() {
        @ChildBindingState
        int[] testBindingStates = {
            ChildBindingState.WAIVED,
            ChildBindingState.NOT_PERCEPTIBLE,
            ChildBindingState.VISIBLE,
            ChildBindingState.STRONG,
        };
        for (@ChildBindingState int testBindingState : testBindingStates) {
            // Create a new spare connection for each binding state.
            mTestConnectionFactory.reset();
            mSpareConnection =
                    new SpareChildConnection(
                            /* context= */ null, mConnectionAllocator, /* serviceBundle= */ null);
            ChildProcessConnection connection =
                    mSpareConnection.getConnection(
                            mConnectionAllocator, mServiceCallback, testBindingState);
            assertNotNull(connection);
            mTestConnectionFactory.simulateConnectionBindingSuccessfully();
            assertEquals(testBindingState, connection.bindingStateCurrent());
            switch (testBindingState) {
                case ChildBindingState.WAIVED -> {
                    assertEquals(0, connection.getStrongBindingCount());
                    assertEquals(0, connection.getVisibleBindingCount());
                    assertEquals(0, connection.getNotPerceptibleBindingCount());
                }
                case ChildBindingState.NOT_PERCEPTIBLE -> {
                    assertEquals(0, connection.getStrongBindingCount());
                    assertEquals(0, connection.getVisibleBindingCount());
                    assertEquals(1, connection.getNotPerceptibleBindingCount());
                }
                case ChildBindingState.VISIBLE -> {
                    assertEquals(0, connection.getStrongBindingCount());
                    assertEquals(1, connection.getVisibleBindingCount());
                    assertEquals(0, connection.getNotPerceptibleBindingCount());
                }
                case ChildBindingState.STRONG -> {
                    assertEquals(1, connection.getStrongBindingCount());
                    assertEquals(0, connection.getVisibleBindingCount());
                    assertEquals(0, connection.getNotPerceptibleBindingCount());
                }
                default -> {}
            }
        }
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRecordsConsumedConnection() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SpareChildConnection.LIFECYCLE_EVENT_HISTOGRAM,
                                LifecycleEvent.CONSUMED)
                        .expectBooleanRecord(
                                SpareChildConnection.ALIVE_WHEN_REQUESTED_HISTOGRAM, true)
                        .expectAnyRecord(SpareChildConnection.TIME_FROM_CONSUMED_TO_DEATH_HISTOGRAM)
                        .build();

        mSpareConnection.getConnection(
                mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        mTestConnectionFactory.simulateConnectionSetUp();
        mTestConnectionFactory.simulateServiceDisconnected();

        watcher.assertExpected();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRecordsUnusedDied() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SpareChildConnection.LIFECYCLE_EVENT_HISTOGRAM,
                                LifecycleEvent.UNUSED_DIED)
                        .expectBooleanRecord(
                                SpareChildConnection.ALIVE_WHEN_REQUESTED_HISTOGRAM, false)
                        .build();

        mTestConnectionFactory.simulateServiceDisconnected();
        mSpareConnection.getConnection(
                mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);

        watcher.assertExpected();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRecordsConsumedDiedWhileLaunching() {
        mSpareConnection.getConnection(
                mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SpareChildConnection.LIFECYCLE_EVENT_HISTOGRAM,
                        LifecycleEvent.CONSUMED_DIED_WHILE_LAUNCHING);

        mTestConnectionFactory.simulateServiceDisconnected();

        watcher.assertExpected();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testDoesNotRecordNormalShutdown() {
        mSpareConnection.getConnection(
                mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        mTestConnectionFactory.simulateConnectionSetUp();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(SpareChildConnection.LIFECYCLE_EVENT_HISTOGRAM)
                        .expectNoRecords(SpareChildConnection.TIME_FROM_CONSUMED_TO_DEATH_HISTOGRAM)
                        .build();

        // The browser stopping the process reports it as died, without a service disconnect.
        mTestConnectionFactory.simulateConnectionDied();

        watcher.assertExpected();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testLowerPriorityAfterDelay() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();
        ChildProcessConnection connection = mSpareConnection.getConnection();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                SpareChildConnection.LIFECYCLE_EVENT_HISTOGRAM,
                                LifecycleEvent.PRIORITY_LOWERED,
                                LifecycleEvent.CONSUMED)
                        .expectIntRecord(
                                SpareChildConnection.LOWERED_STATUS_HISTOGRAM,
                                LoweredStatus.CONSUMED_AFTER_LOWERED)
                        .build();

        mSpareConnection.lowerPriorityAfterDelay(1000);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(ChildBindingState.WAIVED, connection.bindingStateCurrent());

        // The launch that consumes it adds the binding it requests.
        mSpareConnection.getConnection(
                mConnectionAllocator, mServiceCallback, ChildBindingState.STRONG);
        assertEquals(ChildBindingState.STRONG, connection.bindingStateCurrent());
        watcher.assertExpected();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testDoesNotLowerPriorityOnceConsumed() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();
        mSpareConnection.lowerPriorityAfterDelay(1000);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SpareChildConnection.LIFECYCLE_EVENT_HISTOGRAM,
                                LifecycleEvent.CONSUMED)
                        .expectIntRecord(
                                SpareChildConnection.LOWERED_STATUS_HISTOGRAM,
                                LoweredStatus.CONSUMED_BEFORE_LOWERED)
                        .build();

        ChildProcessConnection connection =
                mSpareConnection.getConnection(
                        mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        assertEquals(ChildBindingState.VISIBLE, connection.bindingStateCurrent());
        watcher.assertExpected();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testLoweredThenConsumedBeforeReady() {
        ChildProcessConnection connection = mSpareConnection.getConnection();
        lowerPriority();
        assertEquals(ChildBindingState.WAIVED, connection.bindingStateCurrent());

        mSpareConnection.getConnection(
                mConnectionAllocator, mServiceCallback, ChildBindingState.VISIBLE);

        // The requested binding is applied once the process starts.
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();

        assertEquals(ChildBindingState.VISIBLE, connection.bindingStateCurrent());
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRecordsUnusedDiedAfterLowered() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();
        lowerPriority();
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SpareChildConnection.LOWERED_STATUS_HISTOGRAM,
                        LoweredStatus.UNUSED_DIED_AFTER_LOWERED);

        mTestConnectionFactory.simulateServiceDisconnected();

        watcher.assertExpected();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testDoesNotLowerPriorityOnceUnbound() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SpareChildConnection.LIFECYCLE_EVENT_HISTOGRAM,
                                LifecycleEvent.UNUSED_DIED)
                        .expectIntRecord(
                                SpareChildConnection.LOWERED_STATUS_HISTOGRAM,
                                LoweredStatus.UNUSED_DIED_BEFORE_LOWERED)
                        .build();

        // ChildConnectionAllocator posts the death callback, so the lowering runs after the
        // connection is unbound but before the spare connection knows it died.
        mSpareConnection.lowerPriorityAfterDelay(0);
        mTestConnectionFactory.simulateServiceDisconnected();

        watcher.assertExpected();
    }

    private void lowerPriority() {
        mSpareConnection.lowerPriorityAfterDelay(0);
        RobolectricUtil.runAllBackgroundAndUi();
    }
}
