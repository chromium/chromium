// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.ANP_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.END_OF_TEST_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.NODE_TIMEOUT_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.READY_FOR_TEST_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sContentShellDelegate;
import static org.chromium.ui.accessibility.AccessibilityState.EVENT_TYPE_MASK_ALL;
import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.EVENT_TYPE_MASK;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.core.view.accessibility.AccessibilityNodeProviderCompat;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.ui.accessibility.AccessibilityState;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/** Custom activity test rule for any content shell tests related to accessibility. */
@SuppressLint("VisibleForTests")
public class AccessibilityContentShellActivityTestRule extends ContentShellActivityTestRule {
    // Test output error messages.
    protected static final String EVENTS_ERROR =
            "Generated events and actions did not match expectations.";
    protected static final String NODE_ERROR =
            "Generated AccessibilityNodeInfo tree did not match expectations.";
    protected static final String EXPECTATIONS_NULL =
            "Test expectations were null, perhaps the file is missing? Create an empty file for "
                    + "both the -external and -assist-data tests.";
    protected static final String RESULTS_NULL =
            "Test results were null, did you add the tracker to WebContentsAccessibilityImpl?";
    protected static final String MISSING_FILE_ERROR =
            "Input file could not be read, perhaps the file is missing?";

    // Member variables required for testing framework. Although they are the same object, we will
    // instantiate an object of type |AccessibilityNodeProvider| for convenience.
    protected static final String BASE_DIRECTORY = "/chromium_tests_root";
    public AccessibilityNodeProviderCompat mNodeProvider;
    public WebContentsAccessibilityImpl mWcax;

    // Tracker for all events and actions performed during a given test.
    private AccessibilityActionAndEventTracker mTracker;

    public AccessibilityContentShellActivityTestRule() {
        super();
    }

    /**
     * Helper methods for setup of a basic web contents accessibility unit test.
     *
     * This method replaces the usual setUp() method annotated with @Before because we wish to
     * load different data with each test, but the process is the same for all tests.
     *
     * Leaving a commented @Before annotation on each method as a reminder/context clue.
     */
    /* @Before */
    protected void setupTestFromFile(String file) {
        // Verify file exists before beginning the test.
        verifyInputFile(file);

        launchContentShellWithUrl(UrlUtils.getIsolatedTestFileUrl(file));
        waitForActiveShellToBeDoneLoading();
        setupTestFramework();
        setAccessibilityDelegate();

        // To prevent flakes, do not disable accessibility mid tests.
        mWcax.setIsAutoDisableAccessibilityCandidateForTesting(false);

        sendReadyForTestSignal();
    }

    /**
     * Helper method to set up our tests. This method replaces the @Before method. Leaving a
     * commented @Before annotation on method as a reminder/context clue.
     */
    /* @Before */
    public void setupTestFramework() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(true);
                    AccessibilityState.setIsScreenReaderEnabledForTesting(true);
                    AccessibilityState.setStateMaskForTesting(EVENT_TYPE_MASK, EVENT_TYPE_MASK_ALL);
                });

        mWcax = getWebContentsAccessibility();
        mNodeProvider = getAccessibilityNodeProvider();

        mTracker = new AccessibilityActionAndEventTracker();
        mWcax.setAccessibilityTrackerForTesting(mTracker);
    }

    public void setupTestFrameworkForBasicMode() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(true);
                    AccessibilityState.setStateMaskForTesting(EVENT_TYPE_MASK, EVENT_TYPE_MASK_ALL);
                });

        mWcax = getWebContentsAccessibility();
        mNodeProvider = getAccessibilityNodeProvider();

        mTracker = new AccessibilityActionAndEventTracker();
        mWcax.setAccessibilityTrackerForTesting(mTracker);
    }

    public void setupTestFrameworkForFormControlsMode() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(true);
                    AccessibilityState.setIsOnlyPasswordManagersEnabledForTesting(true);
                    AccessibilityState.setStateMaskForTesting(EVENT_TYPE_MASK, EVENT_TYPE_MASK_ALL);
                });

        mWcax = getWebContentsAccessibility();
        mNodeProvider = getAccessibilityNodeProvider();

        mTracker = new AccessibilityActionAndEventTracker();
        mWcax.setAccessibilityTrackerForTesting(mTracker);
    }

    /** Helper method to tear down our tests so we can start the next test clean. */
    @After
    public void tearDown() {
        // Always reset our max events for good measure.
        if (mWcax != null) {
            mWcax.setMaxContentChangedEventsToFireForTesting(-1);
        }

        AccessibilityContentShellTestData.resetData();
    }

    /**
     * Returns the current |AccessibilityNodeProvider| from the WebContentsAccessibilityImpl
     * instance. Use polling to ensure a non-null value before returning.
     */
    private AccessibilityNodeProviderCompat getAccessibilityNodeProvider() {
        CriteriaHelper.pollUiThread(
                () -> mWcax.getAccessibilityNodeProviderCompat() != null, ANP_ERROR);
        return mWcax.getAccessibilityNodeProviderCompat();
    }

    /**
     * Helper method to call AccessibilityNodeInfo.getChildId and convert to a virtual
     * view ID using reflection, since the needed methods are hidden.
     */
    protected int getChildId(AccessibilityNodeInfoCompat node, int index) {
        try {
            // The methods found through reflection are only available in |AccessibilityNodeInfo|,
            // so we will unwrap |node| to perform the calls.
            AccessibilityNodeInfo nodeInfo = (AccessibilityNodeInfo) node.getInfo();
            Method getChildIdMethod =
                    AccessibilityNodeInfo.class.getMethod("getChildId", int.class);
            long childId = (long) getChildIdMethod.invoke(nodeInfo, Integer.valueOf(index));
            Method getVirtualDescendantIdMethod =
                    AccessibilityNodeInfo.class.getMethod("getVirtualDescendantId", long.class);
            int virtualViewId =
                    (int) getVirtualDescendantIdMethod.invoke(null, Long.valueOf(childId));
            return virtualViewId;
        } catch (Exception ex) {
            Assert.fail(
                    "Unable to call hidden AccessibilityNodeInfoCompat method: " + ex.toString());
            return 0;
        }
    }

    /**
     * Helper method to recursively search a tree of virtual views under an
     * AccessibilityNodeProvider and return one whose text or contentDescription equals |text|.
     * Returns the virtual view ID of the matching node, if found, and View.NO_ID if not.
     */
    private <T> int findNodeMatching(
            int virtualViewId,
            AccessibilityContentShellTestUtils.AccessibilityNodeInfoMatcher<T> matcher,
            T element) {
        AccessibilityNodeInfoCompat node = mNodeProvider.createAccessibilityNodeInfo(virtualViewId);
        Assert.assertNotEquals(node, null);

        if (matcher.matches(node, element)) return virtualViewId;

        for (int i = 0; i < node.getChildCount(); i++) {
            int childId = getChildId(node, i);
            AccessibilityNodeInfoCompat child = mNodeProvider.createAccessibilityNodeInfo(childId);
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
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            findNodeMatching(View.NO_ID, matcher, element),
                            Matchers.not(View.NO_ID));
                });

        int virtualViewId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> findNodeMatching(View.NO_ID, matcher, element));
        Assert.assertNotEquals(View.NO_ID, virtualViewId);
        return virtualViewId;
    }

    /**
     * Helper method to perform actions on the UI so we can then send accessibility events
     *
     * @param viewId int virtualViewId of the given node
     * @param action int desired AccessibilityNodeInfo action
     * @param args Bundle action bundle
     * @return boolean return value of performAction
     * @throws ExecutionException Error
     */
    public boolean performActionOnUiThread(int viewId, int action, Bundle args)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mNodeProvider.performAction(viewId, action, args));
    }

    /**
     * Helper method to perform an action on the UI, then poll for a given criteria to verify
     * the action was completed.
     *
     * @param viewId int                   virtualViewId of the given node
     * @param action int                   desired AccessibilityNodeInfo action
     * @param args Bundle                  action bundle
     * @param criteria Callable<Boolean>   criteria to poll against to verify completion
     * @return boolean                     return value of performAction
     * @throws ExecutionException          Error
     * @throws Throwable                   Error
     */
    public boolean performActionOnUiThread(
            int viewId, int action, Bundle args, Callable<Boolean> criteria)
            throws ExecutionException, Throwable {
        boolean returnValue = performActionOnUiThread(viewId, action, args);
        CriteriaHelper.pollUiThread(criteria, NODE_TIMEOUT_ERROR);
        return returnValue;
    }

    /** Helper method for executing a given JS method for the current web contents. */
    public void executeJS(String method) {
        ThreadUtils.runOnUiThreadBlocking(
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
                performActionOnUiThread(
                        virtualViewId, AccessibilityNodeInfoCompat.ACTION_FOCUS, null));
        Assert.assertTrue(
                performActionOnUiThread(
                        virtualViewId,
                        AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS,
                        null));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mNodeProvider.createAccessibilityNodeInfo(virtualViewId));

        CriteriaHelper.pollUiThread(
                () -> {
                    return mNodeProvider
                            .createAccessibilityNodeInfo(virtualViewId)
                            .isAccessibilityFocused();
                },
                NODE_TIMEOUT_ERROR);
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
     * Call through the WebContentsAccessibilityImpl to send a signal that we are ready to begin a
     * test (using the kEndOfTest signal for simplicity). Poll until we receive the generated Blink
     * event in response, then reset the tracker.
     */
    public void sendReadyForTestSignal() {
        ThreadUtils.runOnUiThreadBlocking(() -> mWcax.signalEndOfTestForTesting());
        CriteriaHelper.pollUiThread(() -> mTracker.testComplete(), READY_FOR_TEST_ERROR);
        ThreadUtils.runOnUiThreadBlocking(() -> mTracker.signalReadyForTest());
    }

    /**
     * Call through the WebContentsAccessibilityImpl to send a kEndOfTest event to signal that we
     * are done with a test. Poll until we receive the generated Blink event in response.
     */
    public void sendEndOfTestSignal() {
        ThreadUtils.runOnUiThreadBlocking(() -> mWcax.signalEndOfTestForTesting());
        CriteriaHelper.pollUiThread(() -> mTracker.testComplete(), END_OF_TEST_ERROR);
    }

    /**
     * Helper method to generate results from the |AccessibilityActionAndEventTracker|.
     *
     * @return          String      List of all actions and events performed during test.
     */
    public String getTrackerResults() {
        return mTracker.results();
    }

    /**
     * Read the contents of a file, and return as a String.
     *
     * @param file                  File to read (including path and name)
     * @return String               Contents of the given file.
     */
    protected String readExpectationFile(String file) {
        String directory = Environment.getExternalStorageDirectory().getPath() + BASE_DIRECTORY;

        try {
            File expectedFile = new File(directory, "/" + file);
            FileInputStream fis = new FileInputStream(expectedFile);

            byte[] data = new byte[(int) expectedFile.length()];
            fis.read(data);
            fis.close();

            return new String(data);
        } catch (IOException e) {
            throw new AssertionError(EXPECTATIONS_NULL, e);
        }
    }

    /**
     * Check that a given file exists on disk.
     *
     * @param file                  String - file to check, including path and name
     */
    protected void verifyInputFile(String file) {
        String directory = Environment.getExternalStorageDirectory().getPath() + BASE_DIRECTORY;

        File expectedFile = new File(directory, "/" + file);
        Assert.assertTrue(
                MISSING_FILE_ERROR
                        + " could not find the directory: "
                        + directory
                        + ", and/or file: "
                        + expectedFile.getPath(),
                expectedFile.exists());
    }
}
