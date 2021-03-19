// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.ANP_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.END_OF_TEST_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.NODE_TIMEOUT_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sContentShellDelegate;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.lang.reflect.Method;
import java.util.concurrent.ExecutionException;

/**
 * Custom activity test rule for any content shell tests related to accessibility.
 */
@SuppressLint("VisibleForTests")
public class AccessibilityContentShellActivityTestRule extends ContentShellActivityTestRule {
    // Member variables required for testing framework. Although they are the same object, we will
    // instantiate an object of type |AccessibilityNodeProvider| for convenience.
    public AccessibilityNodeProvider mNodeProvider;
    public WebContentsAccessibilityImpl mWcax;

    // Tracker for all events and actions performed during a given test.
    private AccessibilityActionAndEventTracker mTracker;

    public AccessibilityContentShellActivityTestRule() {
        super();
    }

    /**
     * Helper method to set up our tests. This method replaces the @Before method.
     * Leaving a commented @Before annotation on method as a reminder/context clue.
     */
    /* @Before */
    public void setupTestFramework() {
        mWcax = getWebContentsAccessibility();
        mWcax.setState(true);
        mWcax.setAccessibilityEnabledForTesting();

        mNodeProvider = getAccessibilityNodeProvider();

        mTracker = new AccessibilityActionAndEventTracker();
        mWcax.setAccessibilityTrackerForTesting(mTracker);
    }

    /**
     * Helper method to tear down our tests so we can start the next test clean.
     */
    @After
    public void tearDown() {
        mTracker = null;
        mNodeProvider = null;

        // Always reset our max events for good measure.
        if (mWcax != null) {
            mWcax.setMaxContentChangedEventsToFireForTesting(-1);
            mWcax = null;
        }

        // Reset our test data.
        AccessibilityContentShellTestData.resetData();
    }

    /**
     * Returns the current |AccessibilityNodeProvider| from the WebContentsAccessibilityImpl
     * instance. Use polling to ensure a non-null value before returning.
     */
    private AccessibilityNodeProvider getAccessibilityNodeProvider() {
        CriteriaHelper.pollUiThread(() -> mWcax.getAccessibilityNodeProvider() != null, ANP_ERROR);
        return mWcax.getAccessibilityNodeProvider();
    }

    /**
     * Helper method to call AccessibilityNodeInfo.getChildId and convert to a virtual
     * view ID using reflection, since the needed methods are hidden.
     */
    private int getChildId(AccessibilityNodeInfo node, int index) {
        try {
            Method getChildIdMethod =
                    AccessibilityNodeInfo.class.getMethod("getChildId", int.class);
            long childId = (long) getChildIdMethod.invoke(node, Integer.valueOf(index));
            Method getVirtualDescendantIdMethod =
                    AccessibilityNodeInfo.class.getMethod("getVirtualDescendantId", long.class);
            int virtualViewId =
                    (int) getVirtualDescendantIdMethod.invoke(null, Long.valueOf(childId));
            return virtualViewId;
        } catch (Exception ex) {
            Assert.fail("Unable to call hidden AccessibilityNodeInfo method: " + ex.toString());
            return 0;
        }
    }

    /**
     * Helper method to recursively search a tree of virtual views under an
     * AccessibilityNodeProvider and return one whose text or contentDescription equals |text|.
     * Returns the virtual view ID of the matching node, if found, and View.NO_ID if not.
     */
    private <T> int findNodeMatching(int virtualViewId,
            AccessibilityContentShellTestUtils.AccessibilityNodeInfoMatcher<T> matcher, T element) {
        AccessibilityNodeInfo node = mNodeProvider.createAccessibilityNodeInfo(virtualViewId);
        Assert.assertNotEquals(node, null);

        if (matcher.matches(node, element)) return virtualViewId;

        for (int i = 0; i < node.getChildCount(); i++) {
            int childId = getChildId(node, i);
            AccessibilityNodeInfo child = mNodeProvider.createAccessibilityNodeInfo(childId);
            if (child != null) {
                int result = findNodeMatching(childId, matcher, element);
                if (result != View.NO_ID) return result;
            }
        }

        return View.NO_ID;
    }

    /**
     * Helper method to block until findNodeMatching() returns a valid node matching
     * the given criteria. Returns the virtual view ID of the matching node, if found, and
     * asserts if not.
     */
    public <T> int waitForNodeMatching(
            AccessibilityContentShellTestUtils.AccessibilityNodeInfoMatcher<T> matcher, T element) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    findNodeMatching(View.NO_ID, matcher, element), Matchers.not(View.NO_ID));
        });

        int virtualViewId = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> findNodeMatching(View.NO_ID, matcher, element));
        Assert.assertNotEquals(View.NO_ID, virtualViewId);
        return virtualViewId;
    }

    /**
     * Helper method to perform actions on the UI so we can then send accessibility events
     *
     * @param viewId int                   virtualViewId of the given node
     * @param action int                   desired AccessibilityNodeInfo action
     * @param args Bundle                  action bundle
     * @return boolean                     return value of performAction
     * @throws ExecutionException          Error
     */
    public boolean performActionOnUiThread(int viewId, int action, Bundle args)
            throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mNodeProvider.performAction(viewId, action, args));
    }

    /**
     * Helper method for executing a given JS method for the current web contents.
     */
    public void executeJS(String method) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getWebContents().evaluateJavaScriptForTests(method, null));
    }

    /**
     * Helper method to focus a given node.
     *
     * @param virtualViewId     The virtualViewId of the node to focus
     * @throws Throwable        Error
     */
    public void focusNode(int virtualViewId) throws Throwable {
        // Focus given node, assert actions were performed, then poll until node is updated.
        Assert.assertTrue(
                performActionOnUiThread(virtualViewId, AccessibilityNodeInfo.ACTION_FOCUS, null));
        Assert.assertTrue(performActionOnUiThread(
                virtualViewId, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null));
        AccessibilityNodeInfo nodeInfo = mNodeProvider.createAccessibilityNodeInfo(virtualViewId);

        CriteriaHelper.pollUiThread(() -> {
            nodeInfo.recycle();
            return mNodeProvider.createAccessibilityNodeInfo(virtualViewId)
                    .isAccessibilityFocused();
        }, NODE_TIMEOUT_ERROR);
    }

    /**
     * Helper method for setting standard AccessibilityDelegate. The delegate is set on the parent
     * as WebContentsAccessibilityImpl sends events using the parent.
     */
    public void setAccessibilityDelegate() {
        ((ViewGroup) getContainerView().getParent())
                .setAccessibilityDelegate(sContentShellDelegate);
    }

    /**
     * Call through the WebContentsAccessibilityImpl to send a kEndOfTest event to signal that we
     * are done with a test. Poll until we receive the generated Blink event in response.
     */
    public void sendEndOfTestSignal() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mWcax.signalEndOfTestForTesting());
        CriteriaHelper.pollUiThread(() -> mTracker.testComplete(), END_OF_TEST_ERROR);
    }

    /**
     * Helper method to generate results from the |AccessibilityActionAndEventTracker|.
     * @return          String      List of all actions and events performed during test.
     */
    public String getTrackerResults() {
        return mTracker.results();
    }
}
