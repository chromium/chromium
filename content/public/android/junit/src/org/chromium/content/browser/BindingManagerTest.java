// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentName;
import android.os.Build;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureList;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.TestChildProcessConnection;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for BindingManager and ChildProcessConnection.
 *
 * Default property of being low-end device is overriden, so that both low-end and high-end policies
 * are tested.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.Q)
@LooperMode(LooperMode.Mode.LEGACY)
public class BindingManagerTest {
    private static final int BINDING_COUNT_LIMIT = 5;

    // Creates a mocked ChildProcessConnection that is optionally added to a BindingManager.
    private static ChildProcessConnection createTestChildProcessConnection(
            int pid, BindingManager manager, List<ChildProcessConnection> iterable) {
        TestChildProcessConnection connection =
                new TestChildProcessConnection(
                        new ComponentName("org.chromium.test", "TestService"),
                        /* bindToCallerCheck= */ false,
                        /* bindAsExternalService= */ false,
                        /* serviceBundle= */ null);
        connection.setPid(pid);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        if (manager != null) {
            manager.addConnection(connection);
        }
        iterable.add(connection);
        connection.removeVisibleBinding(); // Remove initial binding.
        return connection;
    }

    Activity mActivity;

    // Created in setUp() for convenience.
    BindingManager mManager;
    BindingManager mVariableManager;

    List<ChildProcessConnection> mIterable;

    @Before
    public void setUp() {
        // The tests run on only one thread. Pretend that is the launcher thread so LauncherThread
        // asserts are not triggered.
        LauncherThread.setCurrentThreadAsLauncherThread();
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mIterable = new ArrayList<>();
        mManager = new BindingManager(mActivity, BINDING_COUNT_LIMIT, mIterable);
        mVariableManager = new BindingManager(mActivity, BindingManager.NO_MAX_SIZE, mIterable);
    }

    @After
    public void tearDown() {
        LauncherThread.setLauncherThreadAsLauncherThread();
        FeatureList.setTestValues(null);
    }

    private void setupBindingType(boolean useNotPerceptibleBinding) {
        boolean isQOrHigher = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
        BindingManager.setUseNotPerceptibleBindingForTesting(
                useNotPerceptibleBinding && isQOrHigher);
        if (useNotPerceptibleBinding) {
            Assert.assertEquals(isQOrHigher, BindingManager.useNotPerceptibleBinding());
            return;
        }
        Assert.assertFalse(BindingManager.useNotPerceptibleBinding());
    }

    private void checkConnections(
            ChildProcessConnection[] connections,
            boolean useNotPerceptibleBinding,
            boolean isConnected) {
        boolean[] connected = new boolean[connections.length];
        Arrays.fill(connected, isConnected);
        checkConnections(connections, useNotPerceptibleBinding, connected);
    }

    private void checkConnections(
            ChildProcessConnection[] connections,
            boolean useNotPerceptibleBinding,
            boolean[] connected) {
        assert connections.length == connected.length;
        for (int i = 0; i < connections.length; i++) {
            Assert.assertEquals(
                    !useNotPerceptibleBinding && connected[i],
                    connections[i].isVisibleBindingBound());
            Assert.assertEquals(
                    useNotPerceptibleBinding && connected[i],
                    connections[i].isNotPerceptibleBindingBound());
        }
    }

    /**
     * Verifies that onSentToBackground() drops all the moderate bindings after some delay, and
     * onBroughtToForeground() doesn't recover them.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testVisibleBindingDropOnBackground() {
        setupBindingType(false);
        doTestBindingDropOnBackground(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testVisibleBindingDropOnBackgroundWithVariableSize() {
        setupBindingType(false);
        doTestBindingDropOnBackground(mVariableManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNotPerceptibleBindingDropOnBackground() {
        setupBindingType(true);
        doTestBindingDropOnBackground(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNotPerceptibleBindingDropOnBackgroundWithVariableSize() {
        setupBindingType(true);
        doTestBindingDropOnBackground(mVariableManager);
    }

    private void doTestBindingDropOnBackground(BindingManager manager) {
        ChildProcessConnection[] connections = new ChildProcessConnection[3];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(/* pid= */ i + 1, manager, mIterable);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        checkConnections(
                connections, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ true);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify that leaving the application for a short time doesn't clear the moderate bindings.
        manager.onSentToBackground();
        checkConnections(
                connections, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ true);

        manager.onBroughtToForeground();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        checkConnections(
                connections, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ true);

        // Call onSentToBackground() and verify that all the moderate bindings drop after some
        // delay.
        manager.onSentToBackground();
        checkConnections(
                connections, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ true);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        checkConnections(
                connections, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ false);

        // Call onBroughtToForeground() and verify that the previous moderate bindings aren't
        // recovered.
        manager.onBroughtToForeground();
        checkConnections(
                connections, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ false);
    }

    /** Verifies that onLowMemory() drops all the moderate bindings. */
    @Test
    @Feature({"ProcessManagement"})
    public void testVisibleBindingDropOnLowMemory() {
        setupBindingType(false);
        doTestBindingDropOnLowMemory(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testVisibleBindingDropOnLowMemoryVariableSize() {
        setupBindingType(false);
        doTestBindingDropOnLowMemory(mVariableManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNotPerceptibleBindingDropOnLowMemory() {
        setupBindingType(true);
        doTestBindingDropOnLowMemory(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNotPerceptibleBindingDropOnLowMemoryVariableSize() {
        setupBindingType(true);
        doTestBindingDropOnLowMemory(mVariableManager);
    }

    private void doTestBindingDropOnLowMemory(BindingManager manager) {
        final Application app = mActivity.getApplication();

        ChildProcessConnection[] connections = new ChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(/* pid= */ i + 1, manager, mIterable);
        }

        checkConnections(
                connections, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ true);

        // Call onLowMemory() and verify that all the moderate bindings drop.
        app.onLowMemory();
        checkConnections(
                connections, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ false);
    }

    /** Verifies that onTrimMemory() drops moderate bindings properly. */
    @Test
    @Feature({"ProcessManagement"})
    public void testVisibleBindingDropOnTrimMemory() {
        setupBindingType(false);
        doTestBindingDropOnTrimMemory(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testVisibleBindingDropOnTrimMemoryWithVariableSize() {
        setupBindingType(false);
        doTestBindingDropOnTrimMemory(mVariableManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNotPerceptibleBindingDropOnTrimMemory() {
        setupBindingType(true);
        doTestBindingDropOnTrimMemory(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNotPerceptibleBindingDropOnTrimMemoryWithVariableSize() {
        setupBindingType(true);
        doTestBindingDropOnTrimMemory(mVariableManager);
    }

    private void doTestBindingDropOnTrimMemory(BindingManager manager) {
        final Application app = mActivity.getApplication();
        // This test applies only to the moderate-binding manager.

        ArrayList<Pair<Integer, Integer>> levelAndExpectedVictimCountList = new ArrayList<>();
        levelAndExpectedVictimCountList.add(
                new Pair<Integer, Integer>(TRIM_MEMORY_RUNNING_MODERATE, 1));
        levelAndExpectedVictimCountList.add(new Pair<Integer, Integer>(TRIM_MEMORY_RUNNING_LOW, 2));
        levelAndExpectedVictimCountList.add(
                new Pair<Integer, Integer>(TRIM_MEMORY_RUNNING_CRITICAL, 4));

        ChildProcessConnection[] connections = new ChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(/* pid= */ i + 1, manager, mIterable);
        }

        for (Pair<Integer, Integer> pair : levelAndExpectedVictimCountList) {
            String message = "Failed for the level=" + pair.first;
            mIterable.clear();
            // Verify that each connection has a moderate binding after binding and releasing a
            // strong binding.
            for (ChildProcessConnection connection : connections) {
                manager.addConnection(connection);
                mIterable.add(connection);
            }

            checkConnections(
                    connections,
                    BindingManager.useNotPerceptibleBinding(),
                    /* isConnected= */ true);

            app.onTrimMemory(pair.first);
            // Verify that some of the moderate bindings have been dropped.
            for (int i = 0; i < connections.length; i++) {
                Assert.assertEquals(
                        message,
                        i >= pair.second,
                        BindingManager.useNotPerceptibleBinding()
                                ? connections[i].isNotPerceptibleBindingBound()
                                : connections[i].isVisibleBindingBound());
            }
        }
    }

    /*
     * Test that Chrome is sent to the background, that the initially added moderate bindings are
     * removed and are not re-added when Chrome is brought back to the foreground.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testVisibleBindingTillBackgroundedSentToBackground() {
        setupBindingType(false);
        doTestBindingTillBackgroundedSentToBackground(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testVisibleBindingTillBackgroundedSentToBackgroundWithVariableSize() {
        setupBindingType(false);
        doTestBindingTillBackgroundedSentToBackground(mVariableManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNotPerceptibleBindingTillBackgroundedSentToBackground() {
        setupBindingType(true);
        doTestBindingTillBackgroundedSentToBackground(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNotPerceptibleBindingTillBackgroundedSentToBackgroundWithVariableSize() {
        setupBindingType(true);
        doTestBindingTillBackgroundedSentToBackground(mVariableManager);
    }

    private void doTestBindingTillBackgroundedSentToBackground(BindingManager manager) {
        ChildProcessConnection[] connection = new ChildProcessConnection[1];
        connection[0] = createTestChildProcessConnection(0, manager, mIterable);
        checkConnections(
                connection, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ true);

        manager.onSentToBackground();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        checkConnections(
                connection, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ false);

        // Bringing Chrome to the foreground should not re-add the moderate bindings.
        manager.onBroughtToForeground();
        checkConnections(
                connection, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ false);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testOneWaivedConnection_VisibleBinding() {
        setupBindingType(false);
        doTestOneWaivedConnection(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testOneWaivedConnectionWithVariableSize_VisibleBinding() {
        setupBindingType(false);
        doTestOneWaivedConnection(mVariableManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testOneWaivedConnection_NotPerceptibleBinding() {
        setupBindingType(true);
        doTestOneWaivedConnection(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testOneWaivedConnectionWithVariableSize_NotPerceptibleBinding() {
        setupBindingType(true);
        doTestOneWaivedConnection(mVariableManager);
    }

    private void doTestOneWaivedConnection(BindingManager manager) {
        ChildProcessConnection[] connections = new ChildProcessConnection[3];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(/* pid= */ i + 1, manager, mIterable);
        }

        // Make sure binding is added for all connections.
        checkConnections(
                connections, BindingManager.useNotPerceptibleBinding(), /* isConnected= */ true);

        manager.rankingChanged();
        checkConnections(
                connections,
                BindingManager.useNotPerceptibleBinding(),
                new boolean[] {false, true, true});

        // Move middle connection to be the first (ie lowest ranked).
        mIterable.set(0, connections[1]);
        mIterable.set(1, connections[0]);
        manager.rankingChanged();
        checkConnections(
                connections,
                BindingManager.useNotPerceptibleBinding(),
                new boolean[] {true, false, true});

        // Swap back.
        mIterable.set(0, connections[0]);
        mIterable.set(1, connections[1]);
        manager.rankingChanged();
        checkConnections(
                connections,
                BindingManager.useNotPerceptibleBinding(),
                new boolean[] {false, true, true});

        manager.removeConnection(connections[1]);
        checkConnections(
                connections,
                BindingManager.useNotPerceptibleBinding(),
                new boolean[] {false, false, true});

        manager.removeConnection(connections[0]);
        checkConnections(
                connections,
                BindingManager.useNotPerceptibleBinding(),
                new boolean[] {false, false, true});
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testBindingCountLimit_VisibleBinding() {
        setupBindingType(false);
        doTestBindingCountLimit(mManager, /* limited= */ true);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNoBindingCountLimitWithVariableSize_VisibleBinding() {
        setupBindingType(false);
        doTestBindingCountLimit(mVariableManager, /* limited= */ false);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testBindingCountLimit_NotPerceptibleBinding() {
        setupBindingType(true);
        doTestBindingCountLimit(mManager, /* limited= */ true);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNoBindingCountLimitWithVariableSize_NotPerceptibleBinding() {
        setupBindingType(true);
        doTestBindingCountLimit(mVariableManager, /* limited= */ false);
    }

    private void doTestBindingCountLimit(BindingManager manager, boolean limited) {
        ChildProcessConnection[] connections = new ChildProcessConnection[BINDING_COUNT_LIMIT + 1];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(/* pid= */ i + 1, manager, mIterable);
        }

        if (!limited) {
            checkConnections(
                    connections,
                    BindingManager.useNotPerceptibleBinding(),
                    /* isConnected= */ true);
        } else {
            checkConnections(
                    connections,
                    BindingManager.useNotPerceptibleBinding(),
                    new boolean[] {false, true, true, true, true, true});
        }
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testBindingCountLimitLowestRankAddedLast_VisibleBinding() {
        setupBindingType(false);
        doTestBindingCountLimitLowestRankAddedLast(mManager, /* limited= */ true);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNoBindingCountLimitLowestRankAddedLastWithVariableSize_VisibleBinding() {
        setupBindingType(false);
        doTestBindingCountLimitLowestRankAddedLast(mVariableManager, /* limited= */ false);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testBindingCountLimitLowestRankAddedLast_NotPerceptibleBinding() {
        setupBindingType(true);
        doTestBindingCountLimitLowestRankAddedLast(mManager, /* limited= */ true);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testNoBindingCountLimitLowestRankAddedLastWithVariableSize_NotPerceptibleBinding() {
        setupBindingType(true);
        doTestBindingCountLimitLowestRankAddedLast(mVariableManager, /* limited= */ false);
    }

    private void doTestBindingCountLimitLowestRankAddedLast(
            BindingManager manager, boolean limited) {
        ChildProcessConnection[] connections = new ChildProcessConnection[BINDING_COUNT_LIMIT + 1];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(/* pid= */ i + 1, null, mIterable);
        }

        // Add the lowest ranked connection last to ensure it doesn't get added if the limit is
        // applied.
        mIterable.set(0, connections[BINDING_COUNT_LIMIT]);
        mIterable.set(BINDING_COUNT_LIMIT, connections[0]);
        for (int i = 0; i < connections.length; i++) {
            manager.addConnection(connections[i]);
        }

        if (!limited) {
            checkConnections(
                    connections,
                    BindingManager.useNotPerceptibleBinding(),
                    /* isConnected= */ true);
        } else {
            checkConnections(
                    connections,
                    BindingManager.useNotPerceptibleBinding(),
                    new boolean[] {true, true, true, true, true, false});
        }
    }
}
