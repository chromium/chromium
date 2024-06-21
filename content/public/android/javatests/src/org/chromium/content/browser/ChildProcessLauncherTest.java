// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.ChildProcessLauncher;
import org.chromium.base.process_launcher.FileDescriptorInfo;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_shell_apk.ChildProcessLauncherTestUtils;
import org.chromium.content_shell_apk.IChildProcessTest;

import java.util.Arrays;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/** Instrumentation tests for ChildProcessLauncher. */
@RunWith(ContentJUnit4ClassRunner.class)
public class ChildProcessLauncherTest {
    private static final long CONDITION_WAIT_TIMEOUT_MS = 5000;

    private static final String SERVICE_PACKAGE_NAME = "org.chromium.content_shell_apk.tests";
    private static final String SERVICE_NAME =
            "org.chromium.content_shell_apk.TestChildProcessService";
    private static final String SERVICE_COUNT_META_DATA_KEY =
            "org.chromium.content.browser.NUM_TEST_SERVICES";

    private static final String EXTRA_SERVICE_PARAM = "org.chromium.content.browser.SERVICE_EXTRA";
    private static final String EXTRA_SERVICE_PARAM_VALUE = "SERVICE_EXTRA";

    private static final String EXTRA_CONNECTION_PARAM =
            "org.chromium.content.browser.CONNECTION_EXTRA";
    private static final String EXTRA_CONNECTION_PARAM_VALUE = "CONNECTION_EXTRA";

    private static final int CONNECTION_BLOCK_UNTIL_CONNECTED = 1;
    private static final int CONNECTION_BLOCK_UNTIL_SETUP = 2;

    private static class ServiceCallbackForwarder
            implements ChildProcessConnection.ServiceCallback {
        private ChildProcessConnection.ServiceCallback mServiceCallback;

        public void setServiceCallback(ChildProcessConnection.ServiceCallback serviceCallback) {
            assert mServiceCallback == null;
            mServiceCallback = serviceCallback;
        }

        @Override
        public void onChildStarted() {
            if (mServiceCallback != null) {
                mServiceCallback.onChildStarted();
            }
        }

        @Override
        public void onChildStartFailed(ChildProcessConnection connection) {
            if (mServiceCallback != null) {
                mServiceCallback.onChildStartFailed(connection);
            }
        }

        @Override
        public void onChildProcessDied(ChildProcessConnection connection) {
            if (mServiceCallback != null) {
                mServiceCallback.onChildProcessDied(connection);
            }
        }
    }

    private static class AlreadyBoundConnection {
        public final ChildProcessConnection connection;
        public final ServiceCallbackForwarder serviceCallbackForwarder;

        public AlreadyBoundConnection(
                ChildProcessConnection connection,
                ServiceCallbackForwarder serviceCallbackForwarder) {
            this.connection = connection;
            this.serviceCallbackForwarder = serviceCallbackForwarder;
        }
    }

    private ChildConnectionAllocator mConnectionAllocator;

    @Before
    public void setUp() {
        mConnectionAllocator =
                ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                        new Callable<ChildConnectionAllocator>() {
                            @Override
                            public ChildConnectionAllocator call() {
                                Context context = InstrumentationRegistry.getTargetContext();
                                return ChildConnectionAllocator.create(
                                        context,
                                        LauncherThread.getHandler(),
                                        null,
                                        SERVICE_PACKAGE_NAME,
                                        SERVICE_NAME,
                                        SERVICE_COUNT_META_DATA_KEY,
                                        /* bindToCaller= */ false,
                                        /* bindAsExternalService= */ false,
                                        /* useStrongBinding= */ false);
                            }
                        });
    }

    private static class IChildProcessBinder extends IChildProcessTest.Stub {
        private final CallbackHelper mOnConnectionSetupHelper = new CallbackHelper();
        private final CallbackHelper mOnLoadNativeHelper = new CallbackHelper();
        private final CallbackHelper mOnBeforeMainHelper = new CallbackHelper();
        private final CallbackHelper mOnRunMainHelper = new CallbackHelper();
        private final CallbackHelper mOnDestroyHelper = new CallbackHelper();

        // Can be accessed after mOnConnectionSetupCalled is signaled.
        private boolean mServiceCreated;
        private Bundle mServiceBundle;
        private Bundle mConnectionBundle;

        // Can be accessed after mOnLoadNativeCalled is signaled.
        private boolean mNativeLibraryLoaded;

        // Can be accessed after mOnBeforeMainCalled is signaled.
        private String[] mCommandLine;

        @Override
        public void onConnectionSetup(
                boolean serviceCreatedCalled, Bundle serviceBundle, Bundle connectionBundle) {
            mServiceCreated = serviceCreatedCalled;
            mServiceBundle = serviceBundle;
            mConnectionBundle = connectionBundle;
            Assert.assertEquals(0, mOnConnectionSetupHelper.getCallCount());
            mOnConnectionSetupHelper.notifyCalled();
        }

        @Override
        public void onLoadNativeLibrary(boolean loadedSuccessfully) {
            mNativeLibraryLoaded = loadedSuccessfully;
            Assert.assertEquals(0, mOnLoadNativeHelper.getCallCount());
            mOnLoadNativeHelper.notifyCalled();
        }

        @Override
        public void onBeforeMain(String[] commandLine) {
            mCommandLine = commandLine;
            Assert.assertEquals(0, mOnBeforeMainHelper.getCallCount());
            mOnBeforeMainHelper.notifyCalled();
        }

        @Override
        public void onRunMain() {
            Assert.assertEquals(0, mOnRunMainHelper.getCallCount());
            mOnRunMainHelper.notifyCalled();
        }

        public void waitForOnConnectionSetupCalled() throws TimeoutException {
            mOnConnectionSetupHelper.waitForCallback(/* currentCallCount= */ 0);
        }

        public void waitForOnNativeLibraryCalled() throws TimeoutException {
            mOnLoadNativeHelper.waitForCallback(/* currentCallCount= */ 0);
        }

        public void waitOnBeforeMainCalled() throws TimeoutException {
            mOnBeforeMainHelper.waitForCallback(/* currentCallCount= */ 0);
        }

        public void waitOnRunMainCalled() throws TimeoutException {
            mOnRunMainHelper.waitForCallback(/* currentCallCount= */ 0);
        }
    }
    ;

    /**
     * Creates a ChildProcessLauncher, using {@param boundConnectionToUse} if non null, and tests
     * that all callbacks on the client and in the service are called appropriately.
     * The service echos back the delegate calls through the IBinder callback so that the test can
     * validate them.
     */
    private void testProcessLauncher(final AlreadyBoundConnection boundConnectionToUse)
            throws TimeoutException {
        // ConditionVariables used to check the ChildProcessLauncher.Delegate methods get called.
        final CallbackHelper onBeforeConnectionAllocatedHelper = new CallbackHelper();
        final CallbackHelper onBeforeConnectionSetupHelper = new CallbackHelper();
        final CallbackHelper onConnectionEstablishedHelper = new CallbackHelper();
        final CallbackHelper onConnectionLostHelper = new CallbackHelper();

        final ChildProcessLauncher.Delegate delegate =
                new ChildProcessLauncher.Delegate() {
                    @Override
                    public ChildProcessConnection getBoundConnection(
                            ChildConnectionAllocator connectionAllocator,
                            ChildProcessConnection.ServiceCallback serviceCallback) {
                        if (boundConnectionToUse == null) {
                            return null;
                        }
                        boundConnectionToUse.serviceCallbackForwarder.setServiceCallback(
                                serviceCallback);
                        return boundConnectionToUse.connection;
                    }

                    @Override
                    public void onBeforeConnectionAllocated(Bundle serviceBundle) {
                        // Should only be called when the ChildProcessLauncher creates the
                        // connection.
                        Assert.assertNull(boundConnectionToUse);
                        Assert.assertEquals(0, onBeforeConnectionAllocatedHelper.getCallCount());
                        serviceBundle.putString(EXTRA_SERVICE_PARAM, EXTRA_SERVICE_PARAM_VALUE);
                        onBeforeConnectionAllocatedHelper.notifyCalled();
                    }

                    @Override
                    public void onBeforeConnectionSetup(Bundle connectionBundle) {
                        connectionBundle.putString(
                                EXTRA_CONNECTION_PARAM, EXTRA_CONNECTION_PARAM_VALUE);
                        Assert.assertEquals(0, onBeforeConnectionSetupHelper.getCallCount());
                        onBeforeConnectionSetupHelper.notifyCalled();
                    }

                    @Override
                    public void onConnectionEstablished(ChildProcessConnection connection) {
                        Assert.assertEquals(0, onConnectionEstablishedHelper.getCallCount());
                        onConnectionEstablishedHelper.notifyCalled();
                    }

                    @Override
                    public void onConnectionLost(ChildProcessConnection connection) {
                        Assert.assertEquals(0, onConnectionLostHelper.getCallCount());
                        onConnectionLostHelper.notifyCalled();
                    }
                };

        final String[] commandLine = new String[] {"--test-param1", "--test-param2"};
        final FileDescriptorInfo[] filesToBeMapped = new FileDescriptorInfo[0];

        final IChildProcessBinder childProcessBinder = new IChildProcessBinder();

        final ChildProcessLauncher processLauncher =
                ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                        new Callable<ChildProcessLauncher>() {
                            @Override
                            public ChildProcessLauncher call() {
                                ChildProcessLauncher processLauncher =
                                        new ChildProcessLauncher(
                                                LauncherThread.getHandler(),
                                                delegate,
                                                commandLine,
                                                filesToBeMapped,
                                                mConnectionAllocator,
                                                Arrays.asList(childProcessBinder),
                                                /* binderBox= */ null);
                                processLauncher.start(
                                        /* setupConnection= */ true,
                                        /* queueIfNoFreeConnection= */ false);
                                return processLauncher;
                            }
                        });

        Assert.assertNotNull(processLauncher);

        boolean allocatedConnection = boundConnectionToUse == null;
        if (allocatedConnection) {
            onBeforeConnectionAllocatedHelper.waitForCallback(/* currentCallback= */ 0);
        }

        onBeforeConnectionSetupHelper.waitForCallback(/* currentCallback= */ 0);

        // Wait for the service to notify its onConnectionSetup was called.
        childProcessBinder.waitForOnConnectionSetupCalled();
        Assert.assertTrue(childProcessBinder.mServiceCreated);
        Assert.assertNotNull(childProcessBinder.mServiceBundle);
        Assert.assertNotNull(childProcessBinder.mConnectionBundle);
        if (allocatedConnection) {
            Assert.assertEquals(
                    EXTRA_SERVICE_PARAM_VALUE,
                    childProcessBinder.mServiceBundle.getString(EXTRA_SERVICE_PARAM));
        }
        Assert.assertEquals(
                EXTRA_CONNECTION_PARAM_VALUE,
                childProcessBinder.mConnectionBundle.getString(EXTRA_CONNECTION_PARAM));

        // Wait for the client onConnectionEstablished call.
        onConnectionEstablishedHelper.waitForCallback(/* currentCallback= */ 0);

        // Wait for the service to notify its library got loaded.
        childProcessBinder.waitForOnNativeLibraryCalled();
        Assert.assertTrue(childProcessBinder.mNativeLibraryLoaded);

        // Wait for the service to notify its onBeforeMain was called.
        childProcessBinder.waitOnBeforeMainCalled();
        Assert.assertArrayEquals(commandLine, childProcessBinder.mCommandLine);

        // Wait for the service to notify its onRunMain was called.
        childProcessBinder.waitOnRunMainCalled();

        // Stop the launcher.
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        processLauncher.stop();
                    }
                });

        // Note we don't wait for service to notify its onDestroy, as it may not
        // always be called.

        // The client should also get a notification that the connection was lost.
        onConnectionLostHelper.waitForCallback(/* currentCallback= */ 0);
    }

    @Test
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testLaunchServiceThatUsesConnectionAllocator() throws Exception {
        testProcessLauncher(/* boundConnectionToUse= */ null);
    }

    @Test
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testLaunchServiceCreatedWithBoundConnection() throws Exception {
        // Wraps the serviceCallback provided by the ChildProcessLauncher so that the
        // ChildProcessConnection can forward to them appropriately.
        final ServiceCallbackForwarder serviceCallbackForwarder = new ServiceCallbackForwarder();
        final ChildProcessConnection boundConnection =
                ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                        new Callable<ChildProcessConnection>() {
                            @Override
                            public ChildProcessConnection call() {
                                Context context = InstrumentationRegistry.getTargetContext();
                                return mConnectionAllocator.allocate(
                                        context,
                                        new Bundle()
                                        /* serviceBundle= */ ,
                                        serviceCallbackForwarder);
                            }
                        });
        Assert.assertNotNull(boundConnection);

        CriteriaHelper.pollInstrumentationThread(
                boundConnection::isConnected, "Connection failed to connect");
        testProcessLauncher(new AlreadyBoundConnection(boundConnection, serviceCallbackForwarder));
    }

    /** Tests cleanup for a connection that fails to connect in the first place. */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceFailedToBind() {
        final ChildConnectionAllocator badConnectionAllocator =
                ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                        new Callable<ChildConnectionAllocator>() {
                            @Override
                            public ChildConnectionAllocator call() {
                                return ChildConnectionAllocator.createFixedForTesting(
                                        null,
                                        "org.chromium.wrong_package",
                                        "WrongService",
                                        /* serviceCount= */ 2,
                                        /* bindToCaller= */ false,
                                        /* bindAsExternalService= */ false,
                                        /* useStrongBinding= */ false);
                            }
                        });
        Assert.assertFalse(badConnectionAllocator.anyConnectionAllocated());

        // Try to allocate a connection to service class in incorrect package. We can do that by
        // using the instrumentation context (getContext()) instead of the app context
        // (getTargetContext()).
        ChildProcessLauncher processLauncher =
                createChildProcessLauncher(
                        badConnectionAllocator,
                        /* setupConnection= */ true,
                        /* queueIfNoFreeConnection= */ false);

        Assert.assertNotNull(processLauncher);

        // Verify that the connection is not considered as allocated (or only briefly, as the
        // freeing is delayed).
        waitForConnectionAllocatorState(badConnectionAllocator, /* emptyState= */ true);
    }

    /** Tests cleanup for a connection that terminates before setup. */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceCrashedBeforeSetup() {
        Assert.assertFalse(mConnectionAllocator.anyConnectionAllocated());

        // Start and connect to a new service.
        ChildProcessLauncher processLauncher =
                createChildProcessLauncher(
                        mConnectionAllocator,
                        /* setupConnection= */ false,
                        /* queueIfNoFreeConnection= */ false);

        // Verify that the service is bound but not yet set up.
        Assert.assertTrue(mConnectionAllocator.anyConnectionAllocated());
        ChildProcessConnection connection = processLauncher.getConnection();
        Assert.assertNotNull(connection);
        waitForConnectionState(connection, CONNECTION_BLOCK_UNTIL_CONNECTED);
        Assert.assertEquals(0, getConnectionPid(connection));

        // Crash the service.
        connection.crashServiceForTesting();

        // Verify that the connection gets cleaned-up.
        waitForConnectionAllocatorState(mConnectionAllocator, /* emptyState= */ true);
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceCrashedAfterSetup() {
        Assert.assertFalse(mConnectionAllocator.anyConnectionAllocated());

        // Start and connect to a new service.
        ChildProcessLauncher processLauncher =
                createChildProcessLauncher(
                        mConnectionAllocator,
                        /* setupConnection= */ true,
                        /* queueIfNoFreeConnection= */ false);

        Assert.assertTrue(mConnectionAllocator.anyConnectionAllocated());
        ChildProcessConnection connection = processLauncher.getConnection();
        Assert.assertNotNull(connection);
        waitForConnectionState(connection, CONNECTION_BLOCK_UNTIL_SETUP);
        // We are passed set-up, the connection should have received its PID.
        Assert.assertNotEquals(0, getConnectionPid(connection));

        // Crash the service.
        connection.crashServiceForTesting();

        // Verify that the connection gets cleaned-up.
        waitForConnectionAllocatorState(mConnectionAllocator, /* emptyState= */ true);

        // Verify that the connection pid remains set after termination.
        Assert.assertNotEquals(0, getConnectionPid(connection));
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testPendingSpawnQueue() {
        Assert.assertFalse(mConnectionAllocator.anyConnectionAllocated());

        // Launch 4 processes. Since we have only 2 services, the 3rd and 4th should get queued.
        ChildProcessLauncher[] launchers = new ChildProcessLauncher[4];
        ChildProcessConnection[] connections = new ChildProcessConnection[4];
        for (int i = 0; i < 4; i++) {
            launchers[i] =
                    createChildProcessLauncher(
                            mConnectionAllocator,
                            /* setupConnection= */ true,
                            /* queueIfNoFreeConnection= */ true);
            Assert.assertNotNull(launchers[i]);
            connections[i] = launchers[i].getConnection();
        }
        Assert.assertNotNull(connections[0]);
        Assert.assertNotNull(connections[1]);
        Assert.assertNull(connections[2]);
        Assert.assertNull(connections[3]);

        // Test creating a launcher with queueIfNoFreeConnection false with no connection available.
        Assert.assertNull(
                createChildProcessLauncher(
                        mConnectionAllocator,
                        /* setupConnection= */ true,
                        /* queueIfNoFreeConnection= */ false));

        waitForConnectionState(connections[0], CONNECTION_BLOCK_UNTIL_SETUP);
        waitForConnectionState(connections[1], CONNECTION_BLOCK_UNTIL_SETUP);

        // Stop one connection, that should free-up a connection and the first queued launcher
        // should use it.
        stopLauncher(launchers[0]);
        waitUntilLauncherSetup(launchers[2]);

        // Last launcher is still queued.
        Assert.assertNull(launchers[3].getConnection());

        // Crash another launcher. It should free-up another connection that the queued-up launcher
        // should use.
        connections[1].crashServiceForTesting();
        waitUntilLauncherSetup(launchers[3]);
    }

    private static ChildProcessLauncher createChildProcessLauncher(
            final ChildConnectionAllocator connectionAllocator,
            final boolean setupConnection,
            final boolean queueIfNoFreeConnection) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<ChildProcessLauncher>() {
                    @Override
                    public ChildProcessLauncher call() {
                        ChildProcessLauncher processLauncher =
                                new ChildProcessLauncher(
                                        LauncherThread.getHandler(),
                                        new ChildProcessLauncher.Delegate() {},
                                        new String[0],
                                        new FileDescriptorInfo[0],
                                        connectionAllocator,
                                        /* binderCallback= */ null,
                                        /* binderBox= */ null);
                        if (!processLauncher.start(setupConnection, queueIfNoFreeConnection)) {
                            return null;
                        }
                        return processLauncher;
                    }
                });
    }

    private static void waitForConnectionAllocatorState(
            final ChildConnectionAllocator connectionAllocator, final boolean emptyState) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            connectionAllocator.anyConnectionAllocated(), Matchers.not(emptyState));
                });
    }

    private static void waitForConnectionState(
            final ChildProcessConnection connection, final int connectionState) {
        Assert.assertThat(
                connectionState,
                Matchers.anyOf(
                        Matchers.equalTo(CONNECTION_BLOCK_UNTIL_CONNECTED),
                        Matchers.equalTo(CONNECTION_BLOCK_UNTIL_SETUP)));
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    if (connectionState == CONNECTION_BLOCK_UNTIL_CONNECTED) {
                        Criteria.checkThat(connection.isConnected(), Matchers.is(true));
                    } else {
                        Criteria.checkThat(
                                connectionState, Matchers.is(CONNECTION_BLOCK_UNTIL_SETUP));
                        Criteria.checkThat(getConnectionPid(connection), Matchers.not(0));
                    }
                });
    }

    private static void waitUntilLauncherSetup(final ChildProcessLauncher launcher) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Failed wait for launcher to connect.",
                            launcher.getConnection(),
                            Matchers.notNullValue());
                });
        waitForConnectionState(launcher.getConnection(), CONNECTION_BLOCK_UNTIL_SETUP);
    }

    private static int getConnectionPid(final ChildProcessConnection connection) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return connection.getPid();
                    }
                });
    }

    private static void stopLauncher(final ChildProcessLauncher launcher) {
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        launcher.stop();
                    }
                });
    }
}
