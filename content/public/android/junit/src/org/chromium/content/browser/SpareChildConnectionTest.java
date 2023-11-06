// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.TestChildProcessConnection;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for the SpareChildConnection class. */
@Config(manifest = Config.NONE)
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class SpareChildConnectionTest {
    @Mock private ChildProcessConnection.ServiceCallback mServiceCallback;

    // A connection allocator not used to create connections.
    private final ChildConnectionAllocator mWrongConnectionAllocator =
            ChildConnectionAllocator.createFixedForTesting(
                    null,
                    "org.chromium.test",
                    "TestServiceName",
                    /* serviceCount= */ 3,
                    /* bindToCaller= */ false,
                    /* bindAsExternalService= */ false,
                    /* useStrongBinding= */ false);

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
                boolean bindToCaller,
                boolean bindAsExternalService,
                Bundle serviceBundle,
                String instanceName) {
            // We expect to create only one connection in these tests.
            assert mConnection == null;
            mConnection =
                    new TestChildProcessConnection(
                            serviceName, bindToCaller, bindAsExternalService, serviceBundle);
            return mConnection;
        }

        public void simulateConnectionBindingSuccessfully() {
            mConnection.getServiceCallback().onChildStarted();
        }

        public void simulateConnectionFailingToBind() {
            mConnection.getServiceCallback().onChildStartFailed(mConnection);
        }

        public void simulateConnectionDied() {
            mConnection.getServiceCallback().onChildProcessDied(mConnection);
        }
    }
    ;

    private final TestConnectionFactory mTestConnectionFactory = new TestConnectionFactory();

    private SpareChildConnection mSpareConnection;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // The tests run on only one thread. Pretend that is the launcher thread so LauncherThread
        // asserts are not triggered.
        LauncherThread.setCurrentThreadAsLauncherThread();

        mConnectionAllocator =
                ChildConnectionAllocator.createFixedForTesting(
                        null,
                        "org.chromium.test.spare_connection",
                        "TestServiceName",
                        /* serviceCount= */ 5,
                        /* bindToCaller= */ false,
                        /* bindAsExternalService= */ false,
                        /* useStrongBinding= */ false);
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
                mSpareConnection.getConnection(mWrongConnectionAllocator, mServiceCallback);
        assertNull(connection);

        // And with the right one.
        connection = mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);

        // The connection has been used, subsequent calls should return null.
        connection = mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNull(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackNotCalledWhenNoConnection() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();

        // Retrieve the wrong connection, no callback should be fired.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mWrongConnectionAllocator, mServiceCallback);
        assertNull(connection);
        ShadowLooper.runUiThreadTasks();
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed(any());
        verify(mServiceCallback, times(0)).onChildProcessDied(any());
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackCalledConnectionReady() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();

        assertFalse(mSpareConnection.isEmpty());

        // Now retrieve the connection, the callback should be invoked.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);

        // No more connections are available.
        assertTrue(mSpareConnection.isEmpty());

        ShadowLooper.runUiThreadTasks();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed(any());
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackCalledConnectionNotReady() {
        assertFalse(mSpareConnection.isEmpty());

        // Retrieve the connection before it's bound.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);
        ShadowLooper.runUiThreadTasks();
        // No callbacks are called.
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed(any());

        // No more connections are available.
        assertTrue(mSpareConnection.isEmpty());

        // Simulate the connection getting bound, it should trigger the callback.
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed(any());
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testUnretrievedConnectionFailsToBind() {
        mTestConnectionFactory.simulateConnectionFailingToBind();

        // We should not have a spare connection.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNull(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRetrievedConnectionFailsToBind() {
        // Retrieve the spare connection before it's bound.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);

        mTestConnectionFactory.simulateConnectionFailingToBind();

        // We should get a failure callback.
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(1)).onChildStartFailed(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRetrievedConnectionStops() {
        // Retrieve the spare connection before it's bound.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);

        mTestConnectionFactory.simulateConnectionDied();

        // We should get a failure callback.
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testConnectionFreeing() {
        // Simulate the connection dying.
        mTestConnectionFactory.simulateConnectionDied();

        // Connection should be gone.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNull(connection);
    }
}
