// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentName;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.TestChildProcessConnection;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for BindingManager and ChildProcessConnection.
 *
 * Default property of being low-end device is overriden, so that both low-end and high-end policies
 * are tested.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BindingManagerTest {
    // Creates a mocked ChildProcessConnection that is optionally added to a BindingManager.
    private static ChildProcessConnection createTestChildProcessConnection(
            int pid, BindingManager manager, List<ChildProcessConnection> iterable) {
        TestChildProcessConnection connection = new TestChildProcessConnection(
                new ComponentName("org.chromium.test", "TestService"),
                false /* bindToCallerCheck */, false /* bindAsExternalService */,
                null /* serviceBundle */);
        connection.setPid(pid);
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        manager.addConnection(connection);
        iterable.add(connection);
        connection.removeModerateBinding(); // Remove initial binding.
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
        mManager = new BindingManager(mActivity, 4, mIterable);
        mVariableManager = new BindingManager(mActivity, mIterable);
    }

    @After
    public void tearDown() {
        LauncherThread.setLauncherThreadAsLauncherThread();
    }

    /**
     * Verifies that onSentToBackground() drops all the moderate bindings after some delay, and
     * onBroughtToForeground() doesn't recover them.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnBackground() {
        doTestModerateBindingDropOnBackground(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnBackgroundWithVariableSize() {
        doTestModerateBindingDropOnBackground(mVariableManager);
    }

    private void doTestModerateBindingDropOnBackground(BindingManager manager) {
        ChildProcessConnection[] connections = new ChildProcessConnection[3];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager, mIterable);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify that leaving the application for a short time doesn't clear the moderate bindings.
        manager.onSentToBackground();
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }
        manager.onBroughtToForeground();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Call onSentToBackground() and verify that all the moderate bindings drop after some
        // delay.
        manager.onSentToBackground();
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        for (ChildProcessConnection connection : connections) {
            Assert.assertFalse(connection.isModerateBindingBound());
        }

        // Call onBroughtToForeground() and verify that the previous moderate bindings aren't
        // recovered.
        manager.onBroughtToForeground();
        for (ChildProcessConnection connection : connections) {
            Assert.assertFalse(connection.isModerateBindingBound());
        }
    }

    /**
     * Verifies that onLowMemory() drops all the moderate bindings.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnLowMemory() {
        doTestModerateBindingDropOnLowMemory(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnLowMemoryVariableSize() {
        doTestModerateBindingDropOnLowMemory(mVariableManager);
    }

    private void doTestModerateBindingDropOnLowMemory(BindingManager manager) {
        final Application app = mActivity.getApplication();

        ChildProcessConnection[] connections = new ChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager, mIterable);
        }

        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Call onLowMemory() and verify that all the moderate bindings drop.
        app.onLowMemory();
        for (ChildProcessConnection connection : connections) {
            Assert.assertFalse(connection.isModerateBindingBound());
        }
    }

    /**
     * Verifies that onTrimMemory() drops moderate bindings properly.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnTrimMemory() {
        doTestModerateBindingDropOnTrimMemory(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnTrimMemoryWithVariableSize() {
        doTestModerateBindingDropOnTrimMemory(mVariableManager);
    }

    private void doTestModerateBindingDropOnTrimMemory(BindingManager manager) {
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
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager, mIterable);
        }

        for (Pair<Integer, Integer> pair : levelAndExpectedVictimCountList) {
            String message = "Failed for the level=" + pair.first;
            mIterable.clear();
            // Verify that each connection has a moderate binding after binding and releasing a
            // strong binding.
            for (ChildProcessConnection connection : connections) {
                manager.addConnection(connection);
                mIterable.add(connection);
                Assert.assertTrue(message, connection.isModerateBindingBound());
            }

            app.onTrimMemory(pair.first);
            // Verify that some of the moderate bindings have been dropped.
            for (int i = 0; i < connections.length; i++) {
                Assert.assertEquals(
                        message, i >= pair.second, connections[i].isModerateBindingBound());
            }
        }
    }

    /*
     * Test that Chrome is sent to the background, that the initially added moderate bindings are
     * removed and are not re-added when Chrome is brought back to the foreground.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedSentToBackground() {
        doTestModerateBindingTillBackgroundedSentToBackground(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedSentToBackgroundWithVariableSize() {
        doTestModerateBindingTillBackgroundedSentToBackground(mVariableManager);
    }

    private void doTestModerateBindingTillBackgroundedSentToBackground(BindingManager manager) {
        ChildProcessConnection connection = createTestChildProcessConnection(0, manager, mIterable);
        Assert.assertTrue(connection.isModerateBindingBound());

        manager.onSentToBackground();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(connection.isModerateBindingBound());

        // Bringing Chrome to the foreground should not re-add the moderate bindings.
        manager.onBroughtToForeground();
        Assert.assertFalse(connection.isModerateBindingBound());
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testOneWaivedConnection() {
        testOneWaivedConnection(mManager);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testOneWaivedConnectionWithVariableSize() {
        testOneWaivedConnection(mVariableManager);
    }

    private void testOneWaivedConnection(BindingManager manager) {
        ChildProcessConnection[] connections = new ChildProcessConnection[3];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager, mIterable);
        }

        // Make sure binding is added for all connections.
        for (ChildProcessConnection c : connections) {
            Assert.assertTrue(c.isModerateBindingBound());
        }

        // Move middle connection to be the first (ie lowest ranked).
        mIterable.set(0, connections[1]);
        mIterable.set(1, connections[0]);
        manager.rankingChanged();
        Assert.assertTrue(connections[0].isModerateBindingBound());
        Assert.assertFalse(connections[1].isModerateBindingBound());
        Assert.assertTrue(connections[2].isModerateBindingBound());

        // Swap back.
        mIterable.set(0, connections[0]);
        mIterable.set(1, connections[1]);
        manager.rankingChanged();
        Assert.assertFalse(connections[0].isModerateBindingBound());
        Assert.assertTrue(connections[1].isModerateBindingBound());
        Assert.assertTrue(connections[2].isModerateBindingBound());

        manager.removeConnection(connections[1]);
        Assert.assertFalse(connections[0].isModerateBindingBound());
        Assert.assertFalse(connections[1].isModerateBindingBound());
        Assert.assertTrue(connections[2].isModerateBindingBound());

        manager.removeConnection(connections[0]);
        Assert.assertFalse(connections[0].isModerateBindingBound());
        Assert.assertFalse(connections[1].isModerateBindingBound());
        Assert.assertTrue(connections[2].isModerateBindingBound());
    }
}
