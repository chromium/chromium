// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BaseSwitches;
import org.chromium.base.ChildBindingState;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.IFileDescriptorInfo;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_shell_apk.ChildProcessLauncherTestUtils;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.Callable;

/** Instrumentation tests for ChildProcessLauncher. */
@RunWith(ContentJUnit4ClassRunner.class)
public class ChildProcessLauncherHelperTest {
    // Pseudo command line arguments to instruct the child process to wait until being killed.
    // Allowing the process to continue would lead to a crash when attempting to initialize IPC
    // channels that are not being set up in this test.
    private static final String[] sProcessWaitArguments = {
        "_", "--" + BaseSwitches.ANDROID_SKIP_CHILD_SERVICE_INIT_FOR_TESTING
    };
    private static final String DEFAULT_SANDBOXED_PROCESS_SERVICE =
            "org.chromium.content.app.SandboxedProcessService";

    private static final int DONT_BLOCK = 0;
    private static final int BLOCK_UNTIL_CONNECTED = 1;
    private static final int BLOCK_UNTIL_SETUP = 2;

    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();
    }

    private static void warmUpOnUiThreadBlocking(final Context context) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChildProcessLauncherHelperImpl.warmUpOnAnyThread(context);
                });
        ChildProcessConnection connection = getWarmUpConnection();
        Assert.assertNotNull(connection);
        blockUntilConnected(connection);
    }

    private void testWarmUpImpl() {
        Context context = InstrumentationRegistry.getTargetContext();
        warmUpOnUiThreadBlocking(context);

        Assert.assertEquals(1, getConnectedSandboxedServicesCount());

        ChildProcessLauncherHelperImpl launcherHelper =
                startSandboxedChildProcess(BLOCK_UNTIL_SETUP, /* doSetupConnection= */ true);

        // The warm-up connection was used, so no new process should have been created.
        Assert.assertEquals(1, getConnectedSandboxedServicesCount());

        int pid = getPid(launcherHelper);
        Assert.assertNotEquals(0, pid);

        stopProcess(launcherHelper);

        waitForConnectedSandboxedServicesCount(0);
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testWarmUp() {
        // Use the default creation parameters.
        testWarmUpImpl();
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testWarmUpWithBindToCaller() {
        Context context = InstrumentationRegistry.getTargetContext();
        ChildProcessCreationParamsImpl.set(
                context.getPackageName(),
                context.getPackageName(),
                /* isExternalSandboxedService= */ false,
                LibraryProcessType.PROCESS_CHILD,
                /* bindToCallerCheck= */ true,
                /* ignoreVisibilityForImportance= */ false);
        testWarmUpImpl();
    }

    // Tests that the warm-up connection is freed from its allocator if it crashes.
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testWarmUpProcessCrashBeforeUse() {
        Assert.assertEquals(0, getConnectedSandboxedServicesCount());

        Context context = InstrumentationRegistry.getTargetContext();
        warmUpOnUiThreadBlocking(context);

        Assert.assertEquals(1, getConnectedSandboxedServicesCount());

        // Crash the warm-up connection before it gets used.
        ChildProcessConnection connection = getWarmUpConnection();
        Assert.assertNotNull(connection);
        connection.crashServiceForTesting();

        // It should get cleaned-up.
        waitForConnectedSandboxedServicesCount(0);

        // And subsequent process launches should work.
        ChildProcessLauncherHelperImpl launcher =
                startSandboxedChildProcess(BLOCK_UNTIL_SETUP, /* doSetupConnection= */ true);
        Assert.assertEquals(1, getConnectedSandboxedServicesCount());
        Assert.assertNotNull(ChildProcessLauncherTestUtils.getConnection(launcher));
    }

    // Tests that the warm-up connection is freed from its allocator if it crashes after being used.
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testWarmUpProcessCrashAfterUse() {
        Context context = InstrumentationRegistry.getTargetContext();
        warmUpOnUiThreadBlocking(context);

        Assert.assertEquals(1, getConnectedSandboxedServicesCount());

        ChildProcessLauncherHelperImpl launcherHelper =
                startSandboxedChildProcess(BLOCK_UNTIL_SETUP, /* doSetupConnection= */ true);

        // The warm-up connection was used, so no new process should have been created.
        Assert.assertEquals(1, getConnectedSandboxedServicesCount());

        int pid = getPid(launcherHelper);
        Assert.assertNotEquals(0, pid);

        ChildProcessConnection connection = retrieveConnection(launcherHelper);
        connection.crashServiceForTesting();

        waitForConnectedSandboxedServicesCount(0);
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testLauncherCleanup() {
        ChildProcessLauncherHelperImpl launcher =
                startSandboxedChildProcess(BLOCK_UNTIL_SETUP, /* doSetupConnection= */ true);
        int pid = getPid(launcher);
        Assert.assertNotEquals(0, pid);

        // Stop the process explicitly, the launcher should get cleared.
        stopProcess(launcher);
        waitForConnectedSandboxedServicesCount(0);

        launcher = startSandboxedChildProcess(BLOCK_UNTIL_SETUP, /* doSetupConnection= */ true);
        pid = getPid(launcher);
        Assert.assertNotEquals(0, pid);

        // This time crash the connection, the launcher should also get cleared.
        ChildProcessConnection connection = retrieveConnection(launcher);
        connection.crashServiceForTesting();
        waitForConnectedSandboxedServicesCount(0);
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testReducePriorityOnBackground() {
        ChildProcessLauncherHelperImpl.setSkipDelayForReducePriorityOnBackgroundForTesting();

        final ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrl("about:blank");
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        Assert.assertTrue(ApplicationStatus.hasVisibleActivities());

        ChildProcessLauncherHelperImpl launcher =
                startChildProcess(
                        BLOCK_UNTIL_SETUP,
                        /* doSetupConnection= */ true,
                        /* sandboxed= */ false,
                        /* reducePriorityOnBackground= */ true,
                        /* canUseWarmUpConnection= */ true);
        final ChildProcessConnection connection =
                ChildProcessLauncherTestUtils.getConnection(launcher);

        Assert.assertEquals(
                ChildBindingState.STRONG,
                (int)
                        ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                                () -> connection.bindingStateCurrent()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.onStateChangeForTesting(activity, ActivityState.STOPPED));
        Assert.assertFalse(ApplicationStatus.hasVisibleActivities());
        Assert.assertTrue(
                ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                        () -> connection.bindingStateCurrent() < ChildBindingState.STRONG));
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testLaunchWithReducedPriorityOnBackground() {
        ChildProcessLauncherHelperImpl.setSkipDelayForReducePriorityOnBackgroundForTesting();

        final ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrl("about:blank");
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        ThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.onStateChangeForTesting(activity, ActivityState.STOPPED));
        Assert.assertFalse(ApplicationStatus.hasVisibleActivities());

        ChildProcessLauncherHelperImpl launcher =
                startChildProcess(
                        BLOCK_UNTIL_SETUP,
                        /* doSetupConnection= */ true,
                        /* sandboxed= */ false,
                        /* reducePriorityOnBackground= */ true,
                        /* canUseWarmUpConnection= */ true);
        final ChildProcessConnection connection =
                ChildProcessLauncherTestUtils.getConnection(launcher);

        Assert.assertTrue(
                ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                        () -> connection.bindingStateCurrent() < ChildBindingState.STRONG));
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testNotPerceptiveBindingForSpareRenderer() {
        FeatureOverrides.overrideParam(
                ContentFeatureList.sSpareRendererAddNotPerceptibleBinding.getFeatureName(),
                ContentFeatureList.sSpareRendererAddNotPerceptibleBinding.getName(),
                true);

        ChildProcessLauncherHelperImpl.setSkipDelayForReducePriorityOnBackgroundForTesting();

        final ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrl("about:blank");
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        ThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.onStateChangeForTesting(activity, ActivityState.STOPPED));
        Assert.assertFalse(ApplicationStatus.hasVisibleActivities());

        ChildProcessLauncherHelperImpl launcher =
                startChildProcess(
                        BLOCK_UNTIL_SETUP,
                        /* doSetupConnection= */ true,
                        /* sandboxed= */ true,
                        /* reducePriorityOnBackground= */ true,
                        /* canUseWarmUpConnection= */ true);
        final ChildProcessConnection connection =
                ChildProcessLauncherTestUtils.getConnection(launcher);

        Assert.assertEquals(
                0,
                (int)
                        ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                                () -> connection.getStrongBindingCount()));
        Assert.assertEquals(
                1,
                (int)
                        ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                                () -> connection.getVisibleBindingCount()));
        Assert.assertEquals(
                0,
                (int)
                        ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                                () -> connection.getNotPerceptibleBindingCount()));
        setPriorityForSpareRenderer(launcher, true);
        Assert.assertEquals(
                1,
                (int)
                        ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                                () -> connection.getNotPerceptibleBindingCount()));
        setPriorityForSpareRenderer(launcher, false);
        Assert.assertEquals(
                0,
                (int)
                        ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                                () -> connection.getNotPerceptibleBindingCount()));
    }

    private static ChildProcessLauncherHelperImpl startSandboxedChildProcess(
            int blockingPolicy, final boolean doSetupConnection) {
        return startChildProcess(
                blockingPolicy,
                doSetupConnection,
                /* sandboxed= */ true,
                /* reducePriorityOnBackground= */ false,
                /* canUseWarmUpConnection= */ true);
    }

    private static ChildProcessLauncherHelperImpl startChildProcess(
            int blockingPolicy,
            final boolean doSetupConnection,
            boolean sandboxed,
            boolean reducePriorityOnBackground,
            boolean canUseWarmUpConnection) {
        assertThat(doSetupConnection || blockingPolicy != BLOCK_UNTIL_SETUP).isTrue();
        ChildProcessLauncherHelperImpl launcher =
                ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                        new Callable<ChildProcessLauncherHelperImpl>() {
                            @Override
                            public ChildProcessLauncherHelperImpl call() {
                                return ChildProcessLauncherHelperImpl.createAndStartForTesting(
                                        sProcessWaitArguments,
                                        new IFileDescriptorInfo[0],
                                        sandboxed,
                                        reducePriorityOnBackground,
                                        canUseWarmUpConnection,
                                        /* binderCallback= */ null,
                                        doSetupConnection);
                            }
                        });
        if (blockingPolicy != DONT_BLOCK) {
            assertThat(blockingPolicy).isAnyOf(BLOCK_UNTIL_CONNECTED, BLOCK_UNTIL_SETUP);
            blockUntilConnected(launcher);
            if (blockingPolicy == BLOCK_UNTIL_SETUP) {
                blockUntilSetup(launcher);
            }
        }
        return launcher;
    }

    private static void blockUntilConnected(final ChildProcessLauncherHelperImpl launcher) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            launcher.getChildProcessConnection(), Matchers.notNullValue());
                    Criteria.checkThat(
                            launcher.getChildProcessConnection().isConnected(), Matchers.is(true));
                });
    }

    private static void blockUntilConnected(final ChildProcessConnection connection) {
        CriteriaHelper.pollInstrumentationThread(
                connection::isConnected, "The connection wasn't established.");
    }

    private static void blockUntilSetup(final ChildProcessLauncherHelperImpl launcher) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "The connection wasn't established", getPid(launcher), Matchers.not(0));
                });
    }

    // Returns the number of sandboxed connection currently connected,
    private static int getConnectedSandboxedServicesCount() {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return ChildProcessLauncherHelperImpl
                                .getConnectedSandboxedServicesCountForTesting();
                    }
                });
    }

    // Blocks until the number of sandboxed connections reaches targetCount.
    private static void waitForConnectedSandboxedServicesCount(int targetCount) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            getConnectedSandboxedServicesCount(), Matchers.is(targetCount));
                });
    }

    private static ChildProcessConnection retrieveConnection(
            final ChildProcessLauncherHelperImpl launcherHelper) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Failed waiting for child process to connect",
                            ChildProcessLauncherTestUtils.getConnection(launcherHelper),
                            Matchers.notNullValue());
                });
        return ChildProcessLauncherTestUtils.getConnection(launcherHelper);
    }

    private static void stopProcess(ChildProcessLauncherHelperImpl launcherHelper) {
        final ChildProcessConnection connection = retrieveConnection(launcherHelper);
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        ChildProcessLauncherHelperImpl.stop(connection.getPid());
                    }
                });
    }

    private static int getPid(final ChildProcessLauncherHelperImpl launcherHelper) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return launcherHelper.getPidForTesting();
                    }
                });
    }

    private static ChildProcessConnection getWarmUpConnection() {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<ChildProcessConnection>() {
                    @Override
                    public ChildProcessConnection call() {
                        return ChildProcessLauncherHelperImpl.getWarmUpConnectionForTesting();
                    }
                });
    }

    private static void setPriorityForSpareRenderer(
            final ChildProcessLauncherHelperImpl launcherHelper, boolean isSpareRenderer) {
        int pid = getPid(launcherHelper);
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        launcherHelper.setPriority(
                                pid,
                                /* visible= */ false,
                                /* hasMediaStream= */ false,
                                /* hasImmersiveXrSession= */ false,
                                /* hasForegroundServiceWorker= */ false,
                                /* frameDepth= */ 0,
                                /* intersectsViewport= */ false,
                                /* boostForPendingViews= */ false,
                                /* boostForLoading= */ false,
                                isSpareRenderer,
                                ChildProcessImportance.NORMAL);
                    }
                });
    }
}
