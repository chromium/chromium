// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static android.content.Context.CLIPBOARD_SERVICE;

import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_HTML_ELEMENT_STRING;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_PROGRESS_VALUE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_END_INT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_START_INT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_ACCESSIBILITY_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLEAR_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COPY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CUT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_LONG_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_NEXT_AT_MOVEMENT_GRANULARITY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_NEXT_HTML_ELEMENT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_UP;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PASTE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PREVIOUS_HTML_ELEMENT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_BACKWARD;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_DOWN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_FORWARD;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_LEFT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_RIGHT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_UP;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_PROGRESS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_SELECTION;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_TEXT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SHOW_ON_SCREEN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_CHARACTER;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_PARAGRAPH;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_WORD;

import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.NODE_TIMEOUT_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sClassNameMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sInputTypeMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sRangeInfoMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sTextMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sViewIdResourceNameMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_INITIAL;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_SUCCESSIVE;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_INITIAL;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_SUCCESSIVE;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_INITIAL;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_SUCCESSIVE;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_INITIAL;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_SUCCESSIVE;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.CACHE_MAX_NODES_HISTOGRAM;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.CACHE_PERCENTAGE_RETRIEVED_FROM_CACHE_HISTOGRAM;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.EVENTS_DROPPED_HISTOGRAM;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.ONE_HUNDRED_PERCENT_HISTOGRAM;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_BASIC;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_COMPLETE;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_FORM_CONTROLS;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.PERCENTAGE_DROPPED_HISTOGRAM;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.USAGE_ACCESSIBILITY_ALWAYS_ON_TIME;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.USAGE_FOREGROUND_TIME;
import static org.chromium.content.browser.accessibility.AccessibilityHistogramRecorder.USAGE_NATIVE_INITIALIZED_TIME;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_CHROME_ROLE;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_IMAGE_DATA;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_OFFSCREEN;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_BOTTOM;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_UNCLIPPED_TOP;
import static org.chromium.ui.accessibility.AccessibilityState.EVENT_TYPE_MASK_NONE;
import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.EVENT_TYPE_MASK;

import android.annotation.SuppressLint;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Bundle;
import android.text.InputType;
import android.text.Spannable;
import android.text.style.SuggestionSpan;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.TestAnimations;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for WebContentsAccessibility. Actually tests WebContentsAccessibilityImpl that implements
 * the interface.
 */
@RunWith(ContentJUnit4ClassRunner.class)
// TODO(crbug.com/344676953): Failing when batched, batch this again.
@SuppressLint("VisibleForTests")
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
@TestAnimations.EnableAnimations
public class WebContentsAccessibilityTest {
    // Test output error messages
    private static final String DISABLED_COMBOBOX_ERROR =
            "disabled combobox child elements should not be clickable";
    private static final String THRESHOLD_ERROR =
            "Too many TYPE_WINDOW_CONTENT_CHANGED events received in an atomic update.";
    private static final String THRESHOLD_LOW_EVENT_COUNT_ERROR =
            "Expected more TYPE_WINDOW_CONTENT_CHANGED events"
                    + "in an atomic update, is throttling still necessary?";
    private static final String SPELLING_ERROR =
            "node should have a Spannable with spelling correction for given text.";
    private static final String INPUT_RANGE_VALUE_MISMATCH =
            "Value for <input type='range'> is incorrect, did you honor 'step' value?";
    private static final String INPUT_RANGE_EVENT_ERROR =
            "TYPE_VIEW_SCROLLED event not received before timeout.";
    private static final String CACHING_ERROR = "AccessibilityNodeInfo cache has stale data";
    private static final String NODE_EXTRAS_UNCLIPPED_ERROR =
            "AccessibilityNodeInfo object should have unclipped bounds in extras bundle";
    private static final String TEXT_SELECTION_AND_TRAVERSAL_ERROR =
            "Expected to receive both a traversal and selection text event";
    private static final String BOUNDING_BOX_ERROR =
            "Expected bounding box to have updated values.";
    private static final String UMA_HISTOGRAM_ERROR =
            "Expected UMA histograms did not match recorded value.";
    private static final String VISIBLE_TO_USER_ERROR =
            "AccessibilityNodeInfo object has incorrect visibleToUser value";
    private static final String OFFSCREEN_BUNDLE_EXTRA_ERROR =
            "AccessibilityNodeInfo object has incorrect Bundle extras for offscreen boolean.";
    private static final String PERFORM_ACTION_ERROR =
            "performAction did not update node as expected.";
    private static final String IMAGE_DATA_BUNDLE_EXTRA_ERROR =
            "AccessibilityNodeInfo object does not have Bundle extra containing image data.";
    private static final String FOCUSING_ERROR =
            "Expected focus to be on a different node than it is.";

    // ContentFeatureList maps used for various tests.
    private static final Map<String, Boolean> INCLUDE_LONG_CLICK_ENABLED =
            Map.of(ContentFeatureList.ACCESSIBILITY_INCLUDE_LONG_CLICK_ACTION, true);
    private static final Map<String, Boolean> INCLUDE_LONG_CLICK_DISABLED =
            Map.of(ContentFeatureList.ACCESSIBILITY_INCLUDE_LONG_CLICK_ACTION, false);

    // Constant values for unit tests
    private static final int UNSUPPRESSED_EXPECTED_COUNT = 15;

    private AccessibilityNodeInfoCompat mNodeInfo;
    private AccessibilityContentShellTestData mTestData;

    @Rule
    public AccessibilityContentShellActivityTestRule mActivityTestRule =
            new AccessibilityContentShellActivityTestRule();

    /**
     * Helper methods for setup of a basic web contents accessibility unit test.
     *
     * These methods replace the usual setUp() method annotated with @Before because we wish to
     * load different data with each test, but the process is the same for all tests.
     *
     * Leaving a commented @Before annotation on each method as a reminder/context clue.
     */
    /* @Before */
    protected void setupTestWithHTML(String html) {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mActivityTestRule.setupTestFramework();
        mActivityTestRule.setAccessibilityDelegate();

        // To prevent flakes, do not disable accessibility mid tests.
        mActivityTestRule.mWcax.setIsAutoDisableAccessibilityCandidateForTesting(false);

        mTestData = AccessibilityContentShellTestData.getInstance();
        mActivityTestRule.sendReadyForTestSignal();
    }

    protected void setupTestWithHTMLForFormControlsMode(String html) {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mActivityTestRule.setupTestFrameworkForFormControlsMode();
        mActivityTestRule.setAccessibilityDelegate();

        // To prevent flakes, do not disable accessibility mid tests.
        mActivityTestRule.mWcax.setIsAutoDisableAccessibilityCandidateForTesting(false);

        mTestData = AccessibilityContentShellTestData.getInstance();
        mActivityTestRule.sendReadyForTestSignal();
    }

    protected void setupTestWithHTMLForBasicMode(String html) {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mActivityTestRule.setupTestFrameworkForBasicMode();
        mActivityTestRule.setAccessibilityDelegate();

        // To prevent flakes, do not disable accessibility mid tests.
        mActivityTestRule.mWcax.setIsAutoDisableAccessibilityCandidateForTesting(false);

        mTestData = AccessibilityContentShellTestData.getInstance();
        mActivityTestRule.sendReadyForTestSignal();
    }

    /* @Before */
    protected void setupTestFromFile(String filepath) {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.getIsolatedTestFileUrl(filepath));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mActivityTestRule.setupTestFramework();
        mActivityTestRule.setAccessibilityDelegate();

        // To prevent flakes, do not disable accessibility mid tests.
        mActivityTestRule.mWcax.setIsAutoDisableAccessibilityCandidateForTesting(false);

        mTestData = AccessibilityContentShellTestData.getInstance();
        mActivityTestRule.sendReadyForTestSignal();
    }

    // Helper pass-through methods to make tests easier to read.
    private <T> int waitForNodeMatching(
            AccessibilityContentShellTestUtils.AccessibilityNodeInfoMatcher<T> matcher, T element) {
        return mActivityTestRule.waitForNodeMatching(matcher, element);
    }

    private boolean performActionOnUiThread(
            int viewId, AccessibilityNodeInfoCompat.AccessibilityActionCompat action, Bundle args)
            throws ExecutionException {
        return mActivityTestRule.performActionOnUiThread(viewId, action.getId(), args);
    }

    private boolean performActionOnUiThread(
            int viewId,
            AccessibilityNodeInfoCompat.AccessibilityActionCompat action,
            Bundle args,
            Callable<Boolean> criteria)
            throws ExecutionException, Throwable {
        return mActivityTestRule.performActionOnUiThread(viewId, action.getId(), args, criteria);
    }

    private void executeJS(String method) {
        mActivityTestRule.executeJS(method);
    }

    private void focusNode(int virtualViewId) throws Throwable {
        mActivityTestRule.focusNode(virtualViewId);
    }

    public AccessibilityNodeInfoCompat createAccessibilityNodeInfo(int virtualViewId) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.mNodeProvider.createAccessibilityNodeInfo(virtualViewId));
    }

    /**
     * Helper method for sending text related events and confirming that the associated text
     * selection and traversal events have been dispatched before continuing with test.
     *
     * @param viewId            int virtualViewId of the text field
     * @param action            AccessibilityActionCompat action to perform
     * @param args              Bundle optional arguments
     * @throws ExecutionException   Error
     */
    private void performTextActionOnUiThread(
            int viewId, AccessibilityNodeInfoCompat.AccessibilityActionCompat action, Bundle args)
            throws ExecutionException {
        // Reset values for traversal and selection events.
        mTestData.setReceivedTraversalEvent(false);
        mTestData.setReceivedSelectionEvent(false);

        // Perform our text selection/traversal action.
        mActivityTestRule.performActionOnUiThread(viewId, action.getId(), args);

        // Poll until both events have been confirmed as received
        CriteriaHelper.pollUiThread(
                () -> {
                    return mTestData.hasReceivedTraversalEvent()
                            && mTestData.hasReceivedSelectionEvent();
                },
                TEXT_SELECTION_AND_TRAVERSAL_ERROR);
    }

    // ------------------ Tests of WebContentsAccessibilityImpl methods ------------------ //

    /** Ensure we throttle TYPE_WINDOW_CONTENT_CHANGED events for large tree updates. */
    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testMaxContentChangedEventsFired_default() throws Throwable {
        // Build a simple web page with complex visibility change.
        setupTestFromFile("content/test/data/android/type_window_content_changed_events.html");

        // Determine the current max events to fire
        int maxEvents = mActivityTestRule.mWcax.getMaxContentChangedEventsToFireForTesting();

        // Find the button node.
        int vvid = waitForNodeMatching(sClassNameMatcher, "android.widget.Button");
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, "Expand All", mNodeInfo.getText());

        // Run JS code to expand comboboxes.
        executeJS("expandComboboxes()");

        // Poll until the JS method is confirmed to have finished.
        CriteriaHelper.pollUiThread(
                () -> {
                    return createAccessibilityNodeInfo(vvid).getText().toString().equals("Done");
                },
                NODE_TIMEOUT_ERROR);

        // Signal end of test
        mActivityTestRule.sendEndOfTestSignal();

        // Verify number of events processed, allow for multiple atomic updates.
        int eventCount = mTestData.getTypeWindowContentChangedCount();
        Assert.assertTrue(thresholdError(eventCount, maxEvents), eventCount <= (maxEvents * 3));
    }

    /**
     * Ensure we need to throttle TYPE_WINDOW_CONTENT_CHANGED events for some large tree updates.
     */
    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testMaxContentChangedEventsFired_largeLimit() throws Throwable {
        // Build a simple web page with complex visibility change.
        setupTestFromFile("content/test/data/android/type_window_content_changed_events.html");

        // "Disable" event suppression by setting an arbitrarily high max events value.
        mActivityTestRule.mWcax.setMaxContentChangedEventsToFireForTesting(Integer.MAX_VALUE);

        // Find the button node.
        int vvid = waitForNodeMatching(sClassNameMatcher, "android.widget.Button");
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, "Expand All", mNodeInfo.getText());

        // Run JS code to expand comboboxes.
        executeJS("expandComboboxes()");

        // Poll until the JS method is confirmed to have finished.
        CriteriaHelper.pollUiThread(
                () -> {
                    return createAccessibilityNodeInfo(vvid).getText().toString().equals("Done");
                },
                NODE_TIMEOUT_ERROR);

        // Signal end of test
        mActivityTestRule.sendEndOfTestSignal();

        // Verify number of events processed
        int eventCount = mTestData.getTypeWindowContentChangedCount();
        Assert.assertTrue(lowThresholdError(eventCount), eventCount > UNSUPPRESSED_EXPECTED_COUNT);
    }

    /** Test that UMA histograms are recorded for AX Mode Complete. */
    @Test
    @SmallTest
    public void testUMAHistograms_AXModeComplete() throws Throwable {
        // Build a simple web page with a few nodes to traverse.
        setupTestWithHTML(
                "<p>This is a test 1</p>\n"
                        + "<p>This is a test 2</p>\n"
                        + "<p>This is a test 3</p>");

        // Set the relevant features and accessibility state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsScreenReaderEnabledForTesting(true);
                    AccessibilityState.setIsOnlyPasswordManagersEnabledForTesting(false);
                });

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM, 0)
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE, 0)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC)
                        .expectIntRecord(EVENTS_DROPPED_HISTOGRAM, 0)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_COMPLETE)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_FORM_CONTROLS)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_BASIC)
                        .build();
        performHistogramActions();

        histogramWatcher.assertExpected();
    }

    /** Test that UMA histograms are recorded for AX Mode Form Controls. */
    @Test
    @SmallTest
    public void testUMAHistograms_AXModeFormControls() throws Throwable {
        // Build a simple web page with a few nodes to traverse.
        setupTestWithHTMLForFormControlsMode(
                "<p>This is a test 1</p>\n"
                        + "<p>This is a test 2</p>\n"
                        + "<p>This is a test 3</p>");

        // Set the relevant features and accessibility state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsScreenReaderEnabledForTesting(false);
                    AccessibilityState.setIsOnlyPasswordManagersEnabledForTesting(true);
                });

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM, 0)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE)
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS, 0)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC)
                        .expectIntRecord(EVENTS_DROPPED_HISTOGRAM, 0)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_COMPLETE)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_FORM_CONTROLS)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_BASIC)
                        .build();

        performHistogramActions();

        histogramWatcher.assertExpected();
    }

    /** Test that UMA histograms are recorded for AX Mode Basic. */
    @Test
    @SmallTest
    public void testUMAHistograms_AXModeBasic() throws Throwable {
        // Build a simple web page with a few nodes to traverse.
        setupTestWithHTMLForBasicMode(
                "<p>This is a test 1</p>\n"
                        + "<p>This is a test 2</p>\n"
                        + "<p>This is a test 3</p>");

        // Set the relevant features and screen reader state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsScreenReaderEnabledForTesting(false);
                    AccessibilityState.setIsOnlyPasswordManagersEnabledForTesting(false);
                });

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM, 0)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS)
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC, 0)
                        .expectIntRecord(EVENTS_DROPPED_HISTOGRAM, 0)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_COMPLETE)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_FORM_CONTROLS)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_BASIC)
                        .build();

        performHistogramActions();

        histogramWatcher.assertExpected();
    }

    /**
     * Test that UMA histograms are recorded for AX Mode Complete when 100% of events are dropped.
     */
    @Test
    @SmallTest
    public void testUMAHistograms_AXModeComplete_100Percent() throws Throwable {
        // Build a simple web page with a few nodes to traverse.
        setupTestWithHTML(
                "<p>This is a test 1</p>\n"
                        + "<p>This is a test 2</p>\n"
                        + "<p>This is a test 3</p>");

        // Set the relevant features and screen reader state, set event type masks to empty.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setStateMaskForTesting(
                            EVENT_TYPE_MASK, EVENT_TYPE_MASK_NONE);
                    AccessibilityState.setIsScreenReaderEnabledForTesting(true);
                    AccessibilityState.setIsOnlyPasswordManagersEnabledForTesting(false);
                });

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM, 100)
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE, 100)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC)
                        .expectIntRecord(EVENTS_DROPPED_HISTOGRAM, 3)
                        .expectIntRecord(ONE_HUNDRED_PERCENT_HISTOGRAM, 3)
                        .expectIntRecord(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_COMPLETE, 3)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_FORM_CONTROLS)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_BASIC)
                        .build();

        performHistogramActions();

        histogramWatcher.assertExpected();
    }

    /**
     * Test that UMA histograms are recorded for AX Mode Form Controls when 100% of events are
     * dropped.
     */
    @Test
    @SmallTest
    public void testUMAHistograms_AXModeFormControls_100Percent() throws Throwable {
        // Build a simple web page with a few nodes to traverse.
        setupTestWithHTMLForFormControlsMode(
                "<p>This is a test 1</p>\n"
                        + "<p>This is a test 2</p>\n"
                        + "<p>This is a test 3</p>");

        // Set the relevant features and screen reader state, set event type masks to empty.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setStateMaskForTesting(
                            EVENT_TYPE_MASK, EVENT_TYPE_MASK_NONE);
                    AccessibilityState.setIsScreenReaderEnabledForTesting(false);
                    AccessibilityState.setIsOnlyPasswordManagersEnabledForTesting(true);
                });

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM, 100)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE)
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS, 100)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC)
                        .expectIntRecord(EVENTS_DROPPED_HISTOGRAM, 3)
                        .expectIntRecord(ONE_HUNDRED_PERCENT_HISTOGRAM, 3)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_COMPLETE)
                        .expectIntRecord(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_FORM_CONTROLS, 3)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_BASIC)
                        .build();

        performHistogramActions();

        histogramWatcher.assertExpected();
    }

    /** Test that UMA histograms are recorded for AX Mode Basic when 100% of events are dropped. */
    @Test
    @SmallTest
    public void testUMAHistograms_AXModeBasic_100Percent() throws Throwable {
        // Build a simple web page with a few nodes to traverse.
        setupTestWithHTMLForBasicMode(
                "<p>This is a test 1</p>\n"
                        + "<p>This is a test 2</p>\n"
                        + "<p>This is a test 3</p>");

        // Set the relevant features and screen reader state, set event type masks to empty.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setStateMaskForTesting(
                            EVENT_TYPE_MASK, EVENT_TYPE_MASK_NONE);
                    AccessibilityState.setIsScreenReaderEnabledForTesting(false);
                    AccessibilityState.setIsOnlyPasswordManagersEnabledForTesting(false);
                });

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM, 100)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_COMPLETE)
                        .expectNoRecords(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_FORM_CONTROLS)
                        .expectIntRecord(PERCENTAGE_DROPPED_HISTOGRAM_AXMODE_BASIC, 100)
                        .expectIntRecord(EVENTS_DROPPED_HISTOGRAM, 3)
                        .expectIntRecord(ONE_HUNDRED_PERCENT_HISTOGRAM, 3)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_COMPLETE)
                        .expectNoRecords(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_FORM_CONTROLS)
                        .expectIntRecord(ONE_HUNDRED_PERCENT_HISTOGRAM_AXMODE_BASIC, 3)
                        .build();

        performHistogramActions();

        histogramWatcher.assertExpected();
    }

    /**
     * Test that UMA histograms are recorded for the cache statistics, including the max number of
     * nodes stored in the cache, and percentage of requests retrieved from the cache.
     */
    @Test
    @SmallTest
    public void testUMAHistograms_Cache() throws Throwable {
        // Build a simple web page with a few nodes to traverse.
        setupTestWithHTML(
                "<p>This is a test 1</p>\n"
                        + "<p>This is a test 2</p>\n"
                        + "<p>This is a test 3</p>");

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(CACHE_MAX_NODES_HISTOGRAM, 4)
                        .expectAnyRecord(CACHE_PERCENTAGE_RETRIEVED_FROM_CACHE_HISTOGRAM)
                        .build();

        performHistogramActions();

        histogramWatcher.assertExpected();
    }

    /**
     * Test that UMA histograms are recorded when an instance has disabled/re-enabled accessibility
     * with the Auto-disable Accessibility feature.
     */
    @Test
    @SmallTest
    public void testUMAHistograms_AutoDisableAccessibilityV2() throws Throwable {
        setupTestWithHTML("<p>This is a test</p>");
        waitForNodeMatching(sTextMatcher, "This is a test");

        // Explicitly enable auto-disable capabilities for this test.
        mActivityTestRule.mWcax.setIsAutoDisableAccessibilityCandidateForTesting(true);

        // The test suite always initializes native, so first we will disable it manually.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_INITIAL)
                        .expectAnyRecord(AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_INITIAL)
                        .expectAnyRecord(USAGE_NATIVE_INITIALIZED_TIME)
                        .expectNoRecords(USAGE_ACCESSIBILITY_ALWAYS_ON_TIME)
                        .expectNoRecords(AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_SUCCESSIVE)
                        .expectNoRecords(
                                AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_SUCCESSIVE)
                        .expectNoRecords(USAGE_FOREGROUND_TIME)
                        .build();

        // The test suite always initializes native, so mock a call to disable accessibility. We
        // must update AccessibilityState to ensure the AXMode is propagated through to C++.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.mWcax.forceAutoDisableAccessibilityForTesting();
                    AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(false);
                    AccessibilityState.setIsScreenReaderEnabledForTesting(false);
                });

        // Assert that we record initial enabled time and that disabled was called once.
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_INITIAL)
                        .expectAnyRecord(AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_INITIAL)
                        .expectNoRecords(USAGE_FOREGROUND_TIME)
                        .expectNoRecords(USAGE_NATIVE_INITIALIZED_TIME)
                        .expectNoRecords(USAGE_ACCESSIBILITY_ALWAYS_ON_TIME)
                        .expectNoRecords(AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_SUCCESSIVE)
                        .expectNoRecords(
                                AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_SUCCESSIVE)
                        .build();

        // To re-enable native accessibility, we need to make a request from the framework.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsScreenReaderEnabledForTesting(true);
                    AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(true);
                    mActivityTestRule.mWcax.getAccessibilityNodeProvider();
                });

        // Assert that we record initial disabled time and that re-enabled was called once.
        histogramWatcher.assertExpected();

        // We can disable accessibility again, and see entries in the 'successive' histograms.
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_INITIAL)
                        .expectNoRecords(AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_INITIAL)
                        .expectAnyRecord(USAGE_NATIVE_INITIALIZED_TIME)
                        .expectNoRecords(USAGE_ACCESSIBILITY_ALWAYS_ON_TIME)
                        .expectAnyRecord(AUTO_DISABLE_ACCESSIBILITY_ENABLED_TIME_SUCCESSIVE)
                        .expectAnyRecord(
                                AUTO_DISABLE_ACCESSIBILITY_DISABLE_METHOD_CALLED_SUCCESSIVE)
                        .expectNoRecords(USAGE_FOREGROUND_TIME)
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.mWcax.forceAutoDisableAccessibilityForTesting();
                    AccessibilityState.setIsScreenReaderEnabledForTesting(false);
                    AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(false);
                });
        histogramWatcher.assertExpected();

        // Finally re-enable accessibility again to verify 'successive' histograms.
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_INITIAL)
                        .expectNoRecords(AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_INITIAL)
                        .expectNoRecords(USAGE_FOREGROUND_TIME)
                        .expectNoRecords(USAGE_NATIVE_INITIALIZED_TIME)
                        .expectNoRecords(USAGE_ACCESSIBILITY_ALWAYS_ON_TIME)
                        .expectAnyRecord(AUTO_DISABLE_ACCESSIBILITY_DISABLED_TIME_SUCCESSIVE)
                        .expectAnyRecord(
                                AUTO_DISABLE_ACCESSIBILITY_REENABLE_METHOD_CALLED_SUCCESSIVE)
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsScreenReaderEnabledForTesting(true);
                    AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(true);
                    mActivityTestRule.mWcax.getAccessibilityNodeProvider();
                });
        histogramWatcher.assertExpected();
    }

    /** Test that the {resetFocus} method performs as expected with accessibility enabled. */
    @Test
    @SmallTest
    public void testResetFocus() throws Throwable {
        // Setup test page with example paragraphs.
        setupTestWithHTML("<p id='id1'>Example Paragraph 1</p><p>Example Paragraph 2</p>");

        // Find the root node, and a paragraph node, then focus the paragraph.
        int rootVvid = waitForNodeMatching(sClassNameMatcher, "android.webkit.WebView");
        int vvid = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        AccessibilityNodeInfoCompat rootNodeInfo = createAccessibilityNodeInfo(rootVvid);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, rootNodeInfo);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        focusNode(vvid);
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify the root is not focused, and the paragraph is focused.
        Assert.assertFalse(FOCUSING_ERROR, rootNodeInfo.isAccessibilityFocused());
        Assert.assertTrue(FOCUSING_ERROR, mNodeInfo.isAccessibilityFocused());

        // Use the public {resetFocus} method and verify focus has been removed.
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.mWcax.resetFocus());
        CriteriaHelper.pollUiThread(
                () -> !createAccessibilityNodeInfo(vvid).isAccessibilityFocused());
        rootNodeInfo = createAccessibilityNodeInfo(rootVvid);
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        Assert.assertFalse(FOCUSING_ERROR, rootNodeInfo.isAccessibilityFocused());
        Assert.assertFalse(FOCUSING_ERROR, mNodeInfo.isAccessibilityFocused());
    }

    /** Test restoring focus of the latest focused element with the {restoreFocus} method. */
    @Test
    @SmallTest
    public void testRestoreFocus() throws Throwable {
        // Setup test page with example paragraphs.
        setupTestWithHTML("<input id='id1'><input id='id2'>");

        // Find the root node, and a paragraph node, then focus the paragraph.
        int rootVvid = waitForNodeMatching(sClassNameMatcher, "android.webkit.WebView");
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");

        Assert.assertNotNull(NODE_TIMEOUT_ERROR, createAccessibilityNodeInfo(rootVvid));
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, createAccessibilityNodeInfo(vvid1));
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, createAccessibilityNodeInfo(vvid2));

        Assert.assertFalse(
                FOCUSING_ERROR, createAccessibilityNodeInfo(vvid1).isAccessibilityFocused());
        focusNode(vvid1);

        // Reset focus explicitly.
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.mWcax.resetFocus());
        CriteriaHelper.pollUiThread(
                () -> !createAccessibilityNodeInfo(vvid1).isAccessibilityFocused());

        // Restore focus, verify that it gets back.
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.mWcax.restoreFocus());
        CriteriaHelper.pollUiThread(
                () -> createAccessibilityNodeInfo(vvid1).isAccessibilityFocused());

        focusNode(vvid1);
        focusNode(vvid2);

        // Reset focus by performing an action, it covers one more way of losing focus.
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid2,
                        ACTION_CLEAR_ACCESSIBILITY_FOCUS,
                        null,
                        () -> !createAccessibilityNodeInfo(vvid2).isAccessibilityFocused()));

        // Restore focus, verify that the second (latest focused) element gets focus.
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.mWcax.restoreFocus());
        CriteriaHelper.pollUiThread(
                () -> createAccessibilityNodeInfo(vvid2).isAccessibilityFocused());
    }

    /**
     * Tests that Auto-disable Accessibility timers are not set for instances that are not
     * candidates for the feature (e.g. WebView, CCT).
     */
    @Test
    @SmallTest
    public void testAutoDisableAccessibility_candidatesCheck() throws Throwable {
        setupTestWithHTML("<p>This is a test</p>");
        waitForNodeMatching(sTextMatcher, "This is a test");

        // Set this instance as not a candidate.
        mActivityTestRule.mWcax.setIsAutoDisableAccessibilityCandidateForTesting(false);

        // Changing the accessibility state will refresh the native state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsTextShowPasswordEnabledForTesting(true);
                });

        Assert.assertFalse(mActivityTestRule.mWcax.hasAnyPendingTimersForTesting());
    }

    // ------------------ Tests of AccessibilityNodeInfo caching mechanism ------------------ //

    /**
     * Test our internal cache of |AccessibilityNodeInfo| objects for proper focus/action updates.
     */
    @Test
    @SmallTest
    public void testNodeInfoCache_AccessibilityFocusAndActions() throws Throwable {
        // Build a simple web page with two paragraphs that can be focused.
        setupTestWithHTML(
                "<div>\n"
                        + "  <p>Example Paragraph 1</p>\n"
                        + "  <p>Example Paragraph 2</p>\n"
                        + "</div>");

        // Define our root node and paragraph node IDs by looking for their text.
        int vvIdP1 = waitForNodeMatching(sTextMatcher, "Example Paragraph 1");
        int vvIdP2 = waitForNodeMatching(sTextMatcher, "Example Paragraph 2");

        // Get the |AccessibilityNodeInfoCompat| objects for our nodes.
        AccessibilityNodeInfoCompat nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        AccessibilityNodeInfoCompat nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);

        // Assert we have the correct nodes.
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP2);

        // Assert neither node has been focused, and both have a accessibility focusable action.
        Assert.assertFalse(nodeInfoP1.isAccessibilityFocused());
        Assert.assertFalse(nodeInfoP2.isAccessibilityFocused());
        Assert.assertTrue(nodeInfoP1.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP1.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));
        Assert.assertTrue(nodeInfoP2.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP2.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));

        // Now focus each paragraph in turn and check available actions.
        focusNode(vvIdP1);
        nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);
        Assert.assertTrue(nodeInfoP1.isAccessibilityFocused());
        Assert.assertFalse(nodeInfoP1.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertTrue(nodeInfoP1.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP2.isAccessibilityFocused());
        Assert.assertTrue(nodeInfoP2.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP2.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));

        // Focus second paragraph to confirm proper cache updates.
        focusNode(vvIdP2);
        nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);
        Assert.assertFalse(nodeInfoP1.isAccessibilityFocused());
        Assert.assertTrue(nodeInfoP1.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP1.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));
        Assert.assertTrue(nodeInfoP2.isAccessibilityFocused());
        Assert.assertFalse(nodeInfoP2.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertTrue(nodeInfoP2.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));
    }

    /** Test our internal cache of |AccessibilityNodeInfo| objects for proper leaf node updates. */
    @Test
    @SmallTest
    public void testNodeInfoCache_LeafNodeText() throws Throwable {
        // Build a simple web page with a text node inside a leaf node.
        setupTestFromFile("content/test/data/android/leaf_node_updates.html");

        // Find the encompassing <div> node.
        int vvIdDiv = waitForNodeMatching(sViewIdResourceNameMatcher, "test");
        mNodeInfo = createAccessibilityNodeInfo(vvIdDiv);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, "Example text 1", mNodeInfo.getText());

        // Focus the encompassing node.
        focusNode(vvIdDiv);

        // Run JS code to update the text.
        executeJS("updateText()");

        // Signal end of test.
        mActivityTestRule.sendEndOfTestSignal();

        // Check whether the text of the encompassing node has been updated.
        mNodeInfo = createAccessibilityNodeInfo(vvIdDiv);
        Assert.assertEquals(CACHING_ERROR, "Example text 2", mNodeInfo.getText());
    }

    /**
     * Test our internal cache of |AccessibilityNodeInfo| objects for updates to the
     * bounding boxes of nodes during window resizes.
     */
    @Test
    @SmallTest
    public void testNodeInfoCache_BoundingBoxUpdatesOnWindowResize() {
        // Build a simple web page with a flex and a will-change: transform button.
        setupTestWithHTML(
                "<div style=\"display: flex; min-height: 90vh;\">\n"
                        + " <div style=\"display: flex; flex-grow: 1; align-items: flex-end;\">\n"
                        + "   <div>\n"
                        + "     <button style=\"display: inline-flex; will-change: transform;\">\n"
                        + "       Next\n"
                        + "     </button>\n"
                        + "   </div>\n"
                        + " </div>\n"
                        + "</div>");

        // Find the button and get the current bounding box.
        int buttonvvId = waitForNodeMatching(sClassNameMatcher, "android.widget.Button");
        mNodeInfo = createAccessibilityNodeInfo(buttonvvId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, "Next", mNodeInfo.getText());

        Rect beforeBounds = new Rect();
        mNodeInfo.getBoundsInScreen(beforeBounds);

        // Resize the web contents.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getWebContents().setSize(1080, beforeBounds.top / 3));

        // Send end of test signal.
        mActivityTestRule.sendEndOfTestSignal();

        // Fetch the bounding box again and assert top has shrunk by at least half.
        mNodeInfo = createAccessibilityNodeInfo(buttonvvId);
        Rect afterBounds = new Rect();
        mNodeInfo.getBoundsInScreen(afterBounds);

        Assert.assertTrue(BOUNDING_BOX_ERROR, afterBounds.top < (beforeBounds.top / 2));
    }

    // ------------------ Tests of AccessibilityEvents ------------------ //
    // These tests are included here rather than in WebContentsAccessibilityEventsTest because
    // they test the AccessibilityEvent over a series of actions, rather than one method.

    /** Ensure that disabled comboboxes and children are not shadow clickable. */
    @Test
    @SmallTest
    public void testEvent_Combobox_disabled() throws Throwable {
        // Build a simple web page with a disabled combobox.
        setupTestWithHTML(
                "<select disabled>\n"
                        + "  <option>Volvo</option>\n"
                        + "  <option>Saab</option>\n"
                        + "  <option>Mercedes</option>\n"
                        + "</select>");

        // Find the disabled option node and set a delegate to track focus.
        int disabledNodeId = waitForNodeMatching(sTextMatcher, "Volvo");
        mNodeInfo = createAccessibilityNodeInfo(disabledNodeId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        focusNode(disabledNodeId);
        mTestData.setReceivedAccessibilityFocusEvent(false);

        // Perform a click on the node.
        performActionOnUiThread(disabledNodeId, ACTION_CLICK, null);

        // Signal end of test
        mActivityTestRule.sendEndOfTestSignal();

        // Check we did not receive any events.
        Assert.assertFalse(DISABLED_COMBOBOX_ERROR, mTestData.hasReceivedAccessibilityFocusEvent());
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by character with selection mode off
     */
    @Test
    @SmallTest
    public void testEvent_SelectionOFF_CharacterGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing"
        setupTestWithHTML("<input id=\"fn\" type=\"text\" value=\"Testing\">");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editTextVirtualViewId =
                waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(editTextVirtualViewId);
        Assert.assertNotEquals(mNodeInfo, null);

        focusNode(editTextVirtualViewId);

        // Set granularity to CHARACTER, with selection FALSE
        Bundle args = new Bundle();
        args.putInt(ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT, MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);

        // Simulate swiping left (backward)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by character with selection mode on
     */
    @Test
    @LargeTest
    public void testEvent_SelectionON_CharacterGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing"
        setupTestWithHTML("<input id=\"fn\" type=\"text\" value=\"Testing\">");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editTextVirtualViewId =
                waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(editTextVirtualViewId);
        Assert.assertNotEquals(mNodeInfo, null);

        focusNode(editTextVirtualViewId);

        // Set granularity to CHARACTER, with selection TRUE
        Bundle args = new Bundle();
        args.putInt(ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT, MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping left (backward) (adds to selections)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(7, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward) (removes from selection)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(7, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }

        // Turn selection mode off and traverse to beginning so we can select forwards
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);
        }

        // Turn selection mode on
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping right (forward) (adds to selection)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping left (backward) (removes from selections)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by word with selection mode off
     */
    @Test
    @SmallTest
    public void testEvent_SelectionOFF_WordGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing this output is correct"
        setupTestWithHTML(
                "<input id=\"fn\" type=\"text\" value=\"Testing this output is correct\">");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editTextVirtualViewId =
                waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(editTextVirtualViewId);
        Assert.assertNotEquals(mNodeInfo, null);

        focusNode(editTextVirtualViewId);

        // Set granularity to WORD, with selection FALSE
        Bundle args = new Bundle();
        args.putInt(ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT, MOVEMENT_GRANULARITY_WORD);
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);

        int[] wordStarts = new int[] {0, 8, 13, 20, 23};
        int[] wordEnds = new int[] {7, 12, 19, 22, 30};

        // Simulate swiping left (backward) through all 5 words, check indices along the way
        for (int i = 4; i >= 0; --i) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(wordStarts[i], mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordStarts[i], mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward) through all 5 words, check indices along the way
        for (int i = 0; i < 5; ++i) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getSelectionToIndex());
        }
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by word with selection mode on
     */
    @Test
    @LargeTest
    public void testEvent_SelectionON_WordGranularity() throws Throwable {
        setupTestWithHTML(
                "<input id=\"fn\" type=\"text\" value=\"Testing this output is correct\">");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editTextVirtualViewId =
                waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(editTextVirtualViewId);
        Assert.assertNotEquals(mNodeInfo, null);

        focusNode(editTextVirtualViewId);

        // Set granularity to WORD, with selection TRUE
        Bundle args = new Bundle();
        args.putInt(ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT, MOVEMENT_GRANULARITY_WORD);
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        int[] wordStarts = new int[] {0, 8, 13, 20, 23};
        int[] wordEnds = new int[] {7, 12, 19, 22, 30};

        // Simulate swiping left (backward, adds to selection) through all 5 words, check indices
        for (int i = 4; i >= 0; --i) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(30, mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordStarts[i], mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward, removes selection) through all 5 words, check indices
        for (int i = 0; i < 5; ++i) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(30, mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getSelectionToIndex());
        }

        // Turn selection mode off and traverse to beginning so we can select forwards
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
        for (int i = 4; i >= 0; i--) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);
        }

        // Turn selection mode on
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping right (forward) (adds to selection)
        for (int i = 0; i < 5; ++i) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getSelectionToIndex());
        }

        // Simulate swiping left (backward) (removes from selections)
        for (int i = 4; i >= 0; --i) {
            performTextActionOnUiThread(
                    editTextVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordStarts[i], mTestData.getSelectionToIndex());
        }
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating a
     * contenteditable by character with selection mode on.
     */
    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/1360585")
    public void testEvent_contenteditable_SelectionON_CharacterGranularity() throws Throwable {
        setupTestWithHTML("<div contenteditable>Testing</div>");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int contentEditableVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(contentEditableVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        focusNode(contentEditableVirtualViewId);

        // Send an end of test signal to ensure test page has fully started since some bots
        // seem to flake when the page has not fully loaded before testing begins.
        mActivityTestRule.sendEndOfTestSignal();

        // Set granularity to CHARACTER, with selection TRUE
        Bundle args = new Bundle();
        args.putInt(ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT, MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping right (forward) (adds to selection)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(
                    contentEditableVirtualViewId, ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping left (backward) (removes from selections)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(
                    contentEditableVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }

        // Turn selection mode off and traverse to end so we can select backwards
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(
                    contentEditableVirtualViewId, ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);
        }

        // Turn selection mode on
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping left (backward) (adds to selections)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(
                    contentEditableVirtualViewId, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(7, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward) (removes from selection)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(
                    contentEditableVirtualViewId, ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(7, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }
    }

    /**
     * Ensures paragraph navigation actions correctly navigate to the next paragraph and stop at
     * the last paragraph.
     */
    @Test
    @SmallTest
    public void testEvent_paragraphGranularity() throws Throwable {
        setupTestWithHTML(
                "<p>Paragraph 1</p>"
                        + "<p>Paragraph 2</p>"
                        + "<p>Paragraph 3</p>"
                        + "<p>Paragraph 4</p>"
                        + "<p>Paragraph 5</p>");

        // Set granularity to PARAGRAPH
        Bundle args = new Bundle();
        args.putInt(ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT, MOVEMENT_GRANULARITY_PARAGRAPH);
        args.putBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);

        int[] paragraphs = new int[5];
        for (int i = 0; i < 5; i++) {
            paragraphs[i] = waitForNodeMatching(sTextMatcher, "Paragraph " + (i + 1));
        }

        // Simulate swiping forward
        for (int i = 0; i < 4; i++) {
            mTestData.setReceivedAccessibilityFocusEvent(false);
            // Perform our text selection/traversal action.
            performActionOnUiThread(paragraphs[i], ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            // Poll until accessibility focus has changed
            CriteriaHelper.pollUiThread(
                    () -> {
                        return mTestData.hasReceivedAccessibilityFocusEvent();
                    });
        }

        // Ensure the last paragraph has accessibility focus
        AccessibilityNodeInfoCompat lastParagraphNodeInfo =
                createAccessibilityNodeInfo(paragraphs[4]);
        Assert.assertTrue(lastParagraphNodeInfo.isAccessibilityFocused());
    }

    // ------------------ Tests of AccessibilityNodeInfo objects ------------------ //
    // These tests are included here rather than in WebContentsAccessibilityTreeTest because
    // they test the AccessibilityNodeInfo over a series of actions/events, rather than statically.

    /**
     * Test <input type="range"> nodes and events for incrementing/decrementing value with actions.
     */
    @Test
    @SmallTest
    public void testNodeInfo_inputTypeRange() throws Throwable {
        // Create a basic input range, and find the associated |AccessibilityNodeInfo| object.
        setupTestWithHTML("<input type='range' min='0' max='40'>");

        // Find the input range and assert we have the correct node.
        int inputNodeVirtualViewId = waitForNodeMatching(sRangeInfoMatcher, "");
        mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.001);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 40, mNodeInfo.getRangeInfo().getMax(), 0.001);

        // Perform a series of slider increments and check results.
        for (int i = 1; i <= 10; i++) {
            // Increment our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId, ACTION_SCROLL_FORWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(
                    INPUT_RANGE_VALUE_MISMATCH,
                    20 + (2 * i),
                    mNodeInfo.getRangeInfo().getCurrent(),
                    0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }

        // Perform a series of slider decrements and check results.
        for (int i = 1; i <= 20; i++) {
            // Decrement our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId, ACTION_SCROLL_BACKWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(
                    INPUT_RANGE_VALUE_MISMATCH,
                    40 - (2 * i),
                    mNodeInfo.getRangeInfo().getCurrent(),
                    0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }
    }

    /**
     * Test <input type="range"> nodes and events for incrementing/decrementing value with actions.
     */
    @Test
    @SmallTest
    public void testNodeInfo_inputTypeRangeSmall() throws Throwable {
        // Create a basic input range, and find the associated |AccessibilityNodeInfo| object.
        setupTestWithHTML("<input type='range' min='0' max='10' value='0'>");

        // Find the input range and assert we have the correct node.
        int inputNodeVirtualViewId = waitForNodeMatching(sRangeInfoMatcher, "");
        mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.001);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 10, mNodeInfo.getRangeInfo().getMax(), 0.001);

        // Perform a series of slider increments and check results.
        for (int i = 1; i <= 10; i++) {
            // Increment our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId, ACTION_SCROLL_FORWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(
                    INPUT_RANGE_VALUE_MISMATCH, i, mNodeInfo.getRangeInfo().getCurrent(), 0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }

        // Perform a series of slider decrements and check results.
        for (int i = 1; i <= 10; i++) {
            // Decrement our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId, ACTION_SCROLL_BACKWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(
                    INPUT_RANGE_VALUE_MISMATCH,
                    10 - i,
                    mNodeInfo.getRangeInfo().getCurrent(),
                    0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }
    }

    /** Test <input type="range"> nodes move by a minimum value with increment/decrement actions. */
    @Test
    @SmallTest
    public void testNodeInfo_inputTypeRange_withRequiredMin() throws Throwable {
        // Create a basic input range, and find the associated |AccessibilityNodeInfo| object.
        setupTestWithHTML("<input type='range' min='0' max='1000' step='1'>");

        // Find the input range and assert we have the correct node.
        int inputNodeVirtualViewId = waitForNodeMatching(sRangeInfoMatcher, "");
        mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.001);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 1000, mNodeInfo.getRangeInfo().getMax(), 0.001);

        // Perform a series of slider increments and check results.
        for (int i = 1; i <= 10; i++) {
            // Increment our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId, ACTION_SCROLL_FORWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(
                    INPUT_RANGE_VALUE_MISMATCH,
                    500 + (50 * i),
                    mNodeInfo.getRangeInfo().getCurrent(),
                    0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }

        // Perform a series of slider decrements and check results.
        for (int i = 1; i <= 20; i++) {
            // Decrement our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId, ACTION_SCROLL_BACKWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(
                    INPUT_RANGE_VALUE_MISMATCH,
                    1000 - (50 * i),
                    mNodeInfo.getRangeInfo().getCurrent(),
                    0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }
    }

    /**
     * Test |AccessibilityNodeInfo| object for node with spelling error, and ensure the
     * spelling error is encoded as a Spannable.
     **/
    @Test
    @SmallTest
    public void testNodeInfo_spellingError() {
        setupTestWithHTML("<input type='text' value='one wordd has an error'>");

        // Call a test API to explicitly add a spelling error in the same format as
        // would be generated if spelling correction was enabled. Clear our cache for this node.
        int textNodeVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.mWcax.addSpellingErrorForTesting(textNodeVirtualViewId, 4, 9);
                });
        mActivityTestRule.mWcax.clearNodeInfoCacheForGivenId(textNodeVirtualViewId);

        // Get |AccessibilityNodeInfo| object and confirm it is not null.
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Assert that the node's text has a SuggestionSpan surrounding the proper word.
        CharSequence text = mNodeInfo.getText();
        Assert.assertTrue(SPELLING_ERROR, text instanceof Spannable);

        Spannable spannable = (Spannable) text;
        SuggestionSpan[] spans = spannable.getSpans(0, text.length(), SuggestionSpan.class);
        Assert.assertNotNull(SPELLING_ERROR, spans);
        Assert.assertEquals(SPELLING_ERROR, 1, spans.length);
        Assert.assertEquals(SPELLING_ERROR, 4, spannable.getSpanStart(spans[0]));
        Assert.assertEquals(SPELLING_ERROR, 9, spannable.getSpanEnd(spans[0]));
    }

    /** Test |AccessibilityNodeInfo| object for character bounds for a node in Android O. */
    @Test
    @SmallTest
    public void testNodeInfo_extraDataAdded_characterLocations() {
        setupTestWithHTML("<h1>Simple test page</h1><section><p>Text</p></section>");

        // Wait until we find a node in the accessibility tree with the text "Text".
        int textNodeVirtualViewId = waitForNodeMatching(sTextMatcher, "Text");
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Call the API we want to test - addExtraDataToAccessibilityNodeInfo.
        final Bundle arguments = new Bundle();
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX, 0);
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH, 4);

        // addExtraDataToAccessibilityNodeInfo() will end up calling RenderFrameHostImpl's method
        // AccessibilityPerformAction() in the C++ code, which needs to be run from the UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.mNodeProvider.addExtraDataToAccessibilityNodeInfo(
                            textNodeVirtualViewId,
                            mNodeInfo,
                            EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY,
                            arguments);
                });

        // It should return a result, but all of the rects will be the same because it hasn't
        // loaded inline text boxes yet.
        Bundle extras = mNodeInfo.getExtras();
        RectF[] result =
                (RectF[]) extras.getParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
        Assert.assertNotEquals(result, null);
        Assert.assertEquals(result.length, 4);
        Assert.assertEquals(result[0], result[1]);
        Assert.assertEquals(result[0], result[2]);
        Assert.assertEquals(result[0], result[3]);

        // The role string should be a camel cased programmatic identifier.
        CharSequence roleString = extras.getCharSequence(EXTRAS_KEY_CHROME_ROLE);
        Assert.assertEquals("paragraph", roleString.toString());

        // The data needed for text character locations loads asynchronously. Block until
        // it successfully returns the character bounds.
        CriteriaHelper.pollUiThread(
                () -> {
                    AccessibilityNodeInfoCompat textNode =
                            createAccessibilityNodeInfo(textNodeVirtualViewId);
                    mActivityTestRule.mNodeProvider.addExtraDataToAccessibilityNodeInfo(
                            textNodeVirtualViewId,
                            textNode,
                            EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY,
                            arguments);
                    Bundle textNodeExtras = textNode.getExtras();
                    RectF[] textNodeResults =
                            (RectF[])
                                    textNodeExtras.getParcelableArray(
                                            EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
                    Criteria.checkThat(textNodeResults, Matchers.arrayWithSize(4));
                    Criteria.checkThat(textNodeResults[0], Matchers.not(textNodeResults[1]));
                });

        // The final result should be the separate bounding box of all four characters.
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.mNodeProvider.addExtraDataToAccessibilityNodeInfo(
                            textNodeVirtualViewId,
                            mNodeInfo,
                            EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY,
                            arguments);
                });

        extras = mNodeInfo.getExtras();
        result = (RectF[]) extras.getParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
        Assert.assertNotEquals(result[0], result[1]);
        Assert.assertNotEquals(result[0], result[2]);
        Assert.assertNotEquals(result[0], result[3]);

        // All four should have nonzero left, top, width, and height
        for (int i = 0; i < 4; ++i) {
            Assert.assertTrue(result[i].left > 0);
            Assert.assertTrue(result[i].top > 0);
            Assert.assertTrue(result[i].width() > 0);
            Assert.assertTrue(result[i].height() > 0);
        }

        // They should be in order.
        Assert.assertTrue(result[0].left < result[1].left);
        Assert.assertTrue(result[1].left < result[2].left);
        Assert.assertTrue(result[2].left < result[3].left);
    }

    /** Test |AccessibilityNodeInfo| object for image data for a node in Android O. */
    @Test
    @SmallTest
    public void testNodeInfo_extraDataAdded_imageData() {
        // Setup test page with example image (20px red square).
        setupTestWithHTML(
                "<img id='id1' src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEU"
                        + "gAAABQAAAAUCAIAAAAC64paAAAAGElEQVR4AWOsZiAfDLDmUc2jmk"
                        + "c1j2oGADloCbFEqE6LAAAAAElFTkSuQmCC\"/>");

        // Find the image node.
        int imageViewId = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        mNodeInfo = createAccessibilityNodeInfo(imageViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // There should be no image data in the node currently.
        Assert.assertFalse(
                NODE_TIMEOUT_ERROR, mNodeInfo.getExtras().containsKey(EXTRAS_KEY_IMAGE_DATA));

        // The image data is added asynchronously, call the API and poll until it has been added.
        CriteriaHelper.pollUiThread(
                () -> {
                    mActivityTestRule.mNodeProvider.addExtraDataToAccessibilityNodeInfo(
                            imageViewId,
                            mNodeInfo,
                            EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY,
                            new Bundle());
                    return mNodeInfo.getExtras().containsKey(EXTRAS_KEY_IMAGE_DATA);
                });

        // Verify a byte array of sufficient size has been added to the node.
        Assert.assertTrue(
                IMAGE_DATA_BUNDLE_EXTRA_ERROR,
                mNodeInfo.getExtras().containsKey(EXTRAS_KEY_IMAGE_DATA));
        Assert.assertNotNull(
                IMAGE_DATA_BUNDLE_EXTRA_ERROR,
                mNodeInfo.getExtras().getByteArray(EXTRAS_KEY_IMAGE_DATA));
        Assert.assertTrue(
                IMAGE_DATA_BUNDLE_EXTRA_ERROR,
                mNodeInfo.getExtras().getByteArray(EXTRAS_KEY_IMAGE_DATA).length > 50);
    }

    @Test
    @SmallTest
    public void testNodeInfo_extras_unclippedBounds() throws Throwable {
        // Build a simple web page with a scrollable view.
        setupTestFromFile("content/test/data/android/scroll_element_offscreen.html");

        // Find the <div> that contains example paragraphs that can be scrolled.
        int vvIdDiv = waitForNodeMatching(sViewIdResourceNameMatcher, "scroll_view");
        mNodeInfo = createAccessibilityNodeInfo(vvIdDiv);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // The page may take a moment to finish onload method, so poll for a child count.
        CriteriaHelper.pollUiThread(
                () -> {
                    return createAccessibilityNodeInfo(vvIdDiv).getChildCount() == 100;
                },
                NODE_TIMEOUT_ERROR);

        // Focus the scroll container.
        focusNode(vvIdDiv);

        // Send a scroll event so some elements will be offscreen and poll for results.
        performActionOnUiThread(
                vvIdDiv,
                ACTION_PAGE_UP,
                null,
                () -> {
                    return createAccessibilityNodeInfo(vvIdDiv).getExtras() != null
                            && createAccessibilityNodeInfo(vvIdDiv)
                                            .getExtras()
                                            .getInt(EXTRAS_KEY_UNCLIPPED_TOP, 1)
                                    < 0;
                });

        // Signal end of test.
        mActivityTestRule.sendEndOfTestSignal();

        // Refresh the AccessibilityNodeInfo object for the container.
        mNodeInfo = createAccessibilityNodeInfo(vvIdDiv);

        // Check that the container has unclipped values set.
        Assert.assertNotNull(NODE_EXTRAS_UNCLIPPED_ERROR, mNodeInfo.getExtras());
        Assert.assertTrue(
                NODE_EXTRAS_UNCLIPPED_ERROR,
                mNodeInfo.getExtras().getInt(EXTRAS_KEY_UNCLIPPED_TOP) < 0);
        Assert.assertTrue(
                NODE_EXTRAS_UNCLIPPED_ERROR,
                mNodeInfo.getExtras().getInt(EXTRAS_KEY_UNCLIPPED_BOTTOM) > 0);
    }

    /**
     * Test |AccessibilityNodeInfo| object actions for node is specifically user scrollable,
     * and not just programmatically scrollable.
     */
    @Test
    @SmallTest
    public void testNodeInfo_Actions_OverflowHidden() throws Throwable {
        // Build a simple web page with a div and overflow:hidden
        setupTestWithHTML(
                "<div role='group' title='1234' "
                        + "style='overflow:hidden; width: 200px; height:50px'>\n"
                        + "  <p>Example Paragraph 1</p>\n"
                        + "  <p>Example Paragraph 2</p>\n"
                        + "</div>");

        // Define our root node and paragraph node IDs by looking for their text.
        int vvIdDiv = waitForNodeMatching(sTextMatcher, "1234");
        int vvIdP1 = waitForNodeMatching(sTextMatcher, "Example Paragraph 1");
        int vvIdP2 = waitForNodeMatching(sTextMatcher, "Example Paragraph 2");

        // Get the |AccessibilityNodeInfo| objects for our nodes.
        AccessibilityNodeInfoCompat nodeInfoDiv = createAccessibilityNodeInfo(vvIdDiv);
        AccessibilityNodeInfoCompat nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        AccessibilityNodeInfoCompat nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);

        // Assert we have the correct nodes.
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoDiv);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP2);

        // Assert the scroll actions are not present in any of the objects.
        assertActionsContainNoScrolls(nodeInfoDiv);
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Traverse to the next node, then re-assert.
        performActionOnUiThread(vvIdDiv, ACTION_NEXT_HTML_ELEMENT, new Bundle());
        assertActionsContainNoScrolls(nodeInfoDiv);
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Repeat.
        performActionOnUiThread(vvIdP1, ACTION_NEXT_HTML_ELEMENT, new Bundle());
        assertActionsContainNoScrolls(nodeInfoDiv);
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);
    }

    /** Test |AccessibilityNodeInfo| object actions for node is user scrollable. */
    @Test
    @SmallTest
    public void testNodeInfo_Actions_OverflowScroll() throws Throwable {
        // Build a simple web page with a div and overflow:scroll
        setupTestWithHTML(
                "<div id='div1' title='1234' style='overflow:scroll; width: 200px; height:50px'>\n"
                        + "  <p id='p1' tabindex=0>Example Paragraph 1</p>\n"
                        + "  <p id='p2' tabindex=0>Example Paragraph 2</p>\n"
                        + "</div>");

        // Define our root node and paragraph node IDs by looking for their ids.
        int vvIdDiv = waitForNodeMatching(sViewIdResourceNameMatcher, "div1");
        int vvIdP1 = waitForNodeMatching(sViewIdResourceNameMatcher, "p1");
        int vvIdP2 = waitForNodeMatching(sViewIdResourceNameMatcher, "p2");

        // Get the |AccessibilityNodeInfo| objects for our nodes.
        AccessibilityNodeInfoCompat nodeInfoDiv = createAccessibilityNodeInfo(vvIdDiv);
        AccessibilityNodeInfoCompat nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        AccessibilityNodeInfoCompat nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);

        // Assert we have the correct nodes.
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoDiv);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP2);

        // Assert the scroll actions ARE present for our div node, but not the others.
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_FORWARD));
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_DOWN));
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Traverse to the next node, then re-assert.
        performActionOnUiThread(vvIdDiv, ACTION_NEXT_HTML_ELEMENT, new Bundle());
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_FORWARD));
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_DOWN));
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Repeat.
        performActionOnUiThread(vvIdP1, ACTION_NEXT_HTML_ELEMENT, new Bundle());
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_FORWARD));
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_DOWN));
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);
    }

    /** Test that isVisibleToUser and offscreen extra are properly reflecting obscured views. */
    @Test
    @SmallTest
    public void testNodeInfo_isVisibleToUser_offscreenCSS() {
        // Build a simple web page with nodes that are clipped by CSS.
        setupTestFromFile("content/test/data/android/hide_visible_elements_with_css.html");

        // Find relevant nodes in the list.
        int vvIdText1 = waitForNodeMatching(sTextMatcher, "1");
        int vvIdText2 = waitForNodeMatching(sTextMatcher, "6");
        int vvIdText3 = waitForNodeMatching(sTextMatcher, "9");
        AccessibilityNodeInfoCompat mNodeInfo1 = createAccessibilityNodeInfo(vvIdText1);
        AccessibilityNodeInfoCompat mNodeInfo2 = createAccessibilityNodeInfo(vvIdText2);
        AccessibilityNodeInfoCompat mNodeInfo3 = createAccessibilityNodeInfo(vvIdText3);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo3);

        // Signal end of test.
        mActivityTestRule.sendEndOfTestSignal();

        // Check visibility of each element, all text nodes should be visible.
        Assert.assertTrue(VISIBLE_TO_USER_ERROR, mNodeInfo1.isVisibleToUser());
        Assert.assertTrue(VISIBLE_TO_USER_ERROR, mNodeInfo2.isVisibleToUser());
        Assert.assertTrue(VISIBLE_TO_USER_ERROR, mNodeInfo3.isVisibleToUser());

        // Check for offscreen Bundle extra, the second two texts should contain.
        Assert.assertFalse(
                OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo1.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN));
        Assert.assertTrue(
                OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo2.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN));
        Assert.assertTrue(
                OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo2.getExtras().getBoolean(EXTRAS_KEY_OFFSCREEN));
        Assert.assertTrue(
                OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo3.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN));
        Assert.assertTrue(
                OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo3.getExtras().getBoolean(EXTRAS_KEY_OFFSCREEN));
    }

    /** Test that ACTION_LONG_CLICK is included when experiment is running. */
    @Test
    @SmallTest
    public void testNodeInfo_Actions_longClickIncluded() throws Throwable {
        setupTestWithHTML("<p id='id1'>Example</p>");

        FeatureList.setTestFeatures(INCLUDE_LONG_CLICK_ENABLED);

        int vvId = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        mNodeInfo = createAccessibilityNodeInfo(vvId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        Assert.assertTrue(mNodeInfo.getActionList().contains(ACTION_LONG_CLICK));
    }

    /** Test that ACTION_LONG_CLICK is excluded when experiment is paused. */
    @Test
    @SmallTest
    public void testNodeInfo_Actions_longClickExcluded() throws Throwable {
        setupTestWithHTML("<p id='id1'>Example</p>");

        FeatureList.setTestFeatures(INCLUDE_LONG_CLICK_DISABLED);

        int vvId = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        mNodeInfo = createAccessibilityNodeInfo(vvId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        Assert.assertFalse(mNodeInfo.getActionList().contains(ACTION_LONG_CLICK));
    }

    // ------------------ Tests of performAction method ------------------ //

    /** Test that the performAction for ACTION_SET_TEXT works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_setText() throws Throwable {
        // Build a simple web page with an input node to interact with.
        setupTestWithHTML("<input type='text'><button>Button</button>");

        // Find input node and button node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        int vvidButton = waitForNodeMatching(sTextMatcher, "Button");
        AccessibilityNodeInfoCompat buttonNodeInfo = createAccessibilityNodeInfo(vvidButton);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, buttonNodeInfo);

        // Verify that bad requests have no effect.
        Assert.assertFalse(performActionOnUiThread(vvidButton, ACTION_SET_TEXT, null));
        Assert.assertFalse(performActionOnUiThread(vvid, ACTION_SET_TEXT, null));
        Bundle bundle = new Bundle();
        bundle.putCharSequence(ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, null);
        Assert.assertFalse(performActionOnUiThread(vvid, ACTION_SET_TEXT, bundle));

        // Send a proper action and poll for update.
        bundle.putCharSequence(ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, "new text");
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid,
                        ACTION_SET_TEXT,
                        bundle,
                        () -> !createAccessibilityNodeInfo(vvid).getText().toString().isEmpty()));

        // Send of test signal and update node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, "new text", mNodeInfo.getText().toString());
    }

    /** Test that the performAction for ACTION_SET_SELECTION works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_setSelection() throws Throwable {
        // Build a simple web page with an input node to interact with.
        setupTestWithHTML("<input type='text' value='test text'><button>Button</button>");

        // Find input node and button node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        int vvidButton = waitForNodeMatching(sTextMatcher, "Button");
        AccessibilityNodeInfoCompat buttonNodeInfo = createAccessibilityNodeInfo(vvidButton);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, buttonNodeInfo);

        // Verify that a bad request has no effect.
        Assert.assertFalse(performActionOnUiThread(vvidButton, ACTION_SET_SELECTION, null));

        // Send a proper action and poll for update.
        Bundle bundle = new Bundle();
        bundle.putInt(ACTION_ARGUMENT_SELECTION_START_INT, 2);
        bundle.putInt(ACTION_ARGUMENT_SELECTION_END_INT, 5);
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid,
                        ACTION_SET_SELECTION,
                        bundle,
                        () -> {
                            return createAccessibilityNodeInfo(vvid).getTextSelectionStart() > 0
                                    && createAccessibilityNodeInfo(vvid).getTextSelectionEnd() > 0;
                        }));

        // Send of test signal and update node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, 2, mNodeInfo.getTextSelectionStart());
        Assert.assertEquals(PERFORM_ACTION_ERROR, 5, mNodeInfo.getTextSelectionEnd());
    }

    /** Test that the performAction for ACTION_CUT works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_cut() throws Throwable {
        // Build a simple web page with an input field.
        setupTestWithHTML("<input type='text' value='test text'>");

        // Find the relevant node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Select a given portion of the text.
        Bundle bundle = new Bundle();
        bundle.putInt(ACTION_ARGUMENT_SELECTION_START_INT, 2);
        bundle.putInt(ACTION_ARGUMENT_SELECTION_END_INT, 7);
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid,
                        ACTION_SET_SELECTION,
                        bundle,
                        () -> {
                            return createAccessibilityNodeInfo(vvid).getTextSelectionStart() > 0
                                    && createAccessibilityNodeInfo(vvid).getTextSelectionEnd() > 0;
                        }));

        // Perform the "cut" action, and poll for clipboard to be non-null.
        ClipboardManager clipboardManager =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return (ClipboardManager)
                                    mActivityTestRule
                                            .getActivity()
                                            .getSystemService(CLIPBOARD_SERVICE);
                        });
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid, ACTION_CUT, null, () -> clipboardManager.getPrimaryClip() != null));

        // Send end of test signal and refresh input node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify text has been properly added to the clipboard.
        Assert.assertNotNull(PERFORM_ACTION_ERROR, clipboardManager.getPrimaryClip());
        Assert.assertEquals(
                PERFORM_ACTION_ERROR, 1, clipboardManager.getPrimaryClip().getItemCount());
        Assert.assertEquals(
                PERFORM_ACTION_ERROR,
                "st te",
                clipboardManager.getPrimaryClip().getItemAt(0).getText().toString());

        // Verify input node was changed by the cut action.
        Assert.assertEquals(PERFORM_ACTION_ERROR, "text", mNodeInfo.getText().toString());
    }

    /** Test that the performAction for ACTION_COPY works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_copy() throws Throwable {
        // Build a simple web page with an input field.
        setupTestWithHTML("<input type='text' value='test text'>");

        // Find the relevant node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Select a given portion of the text.
        Bundle bundle = new Bundle();
        bundle.putInt(ACTION_ARGUMENT_SELECTION_START_INT, 2);
        bundle.putInt(ACTION_ARGUMENT_SELECTION_END_INT, 7);
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid,
                        ACTION_SET_SELECTION,
                        bundle,
                        () -> {
                            return createAccessibilityNodeInfo(vvid).getTextSelectionStart() > 0
                                    && createAccessibilityNodeInfo(vvid).getTextSelectionEnd() > 0;
                        }));

        // Perform the "copy" action, and poll for clipboard to be non-null.
        ClipboardManager clipboardManager =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return (ClipboardManager)
                                    mActivityTestRule
                                            .getActivity()
                                            .getSystemService(CLIPBOARD_SERVICE);
                        });
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid, ACTION_COPY, null, () -> clipboardManager.getPrimaryClip() != null));

        // Send end of test signal and refresh input node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify text has been properly added to the clipboard.
        Assert.assertNotNull(PERFORM_ACTION_ERROR, clipboardManager.getPrimaryClip());
        Assert.assertEquals(
                PERFORM_ACTION_ERROR, 1, clipboardManager.getPrimaryClip().getItemCount());
        Assert.assertEquals(
                PERFORM_ACTION_ERROR,
                "st te",
                clipboardManager.getPrimaryClip().getItemAt(0).getText().toString());

        // Verify input node was not changed by the copy action.
        Assert.assertEquals(PERFORM_ACTION_ERROR, "test text", mNodeInfo.getText().toString());
    }

    /** Test that the performAction for ACTION_PASTE works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_paste() throws Throwable {
        // Build a simple web page with an input field.
        setupTestWithHTML("<input type='text'>");

        // Find the relevant node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Add some ClipData to the ClipboardManager to paste.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboardManager =
                            (ClipboardManager)
                                    mActivityTestRule
                                            .getActivity()
                                            .getSystemService(CLIPBOARD_SERVICE);
                    clipboardManager.setPrimaryClip(
                            ClipData.newPlainText("test text", "test text"));
                });

        // Focus the input field node.
        focusNode(vvid);

        // Perform a paste action and poll for the text to change.
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid,
                        ACTION_PASTE,
                        null,
                        () -> !createAccessibilityNodeInfo(vvid).getText().toString().isEmpty()));

        // Send end of test signal and update node info.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify text has not been removed from the clipboard.
        ClipboardManager clipboardManager =
                (ClipboardManager)
                        mActivityTestRule.getActivity().getSystemService(CLIPBOARD_SERVICE);
        Assert.assertNotNull(PERFORM_ACTION_ERROR, clipboardManager.getPrimaryClip());
        Assert.assertEquals(
                PERFORM_ACTION_ERROR, 1, clipboardManager.getPrimaryClip().getItemCount());
        Assert.assertEquals(
                PERFORM_ACTION_ERROR,
                "test text",
                clipboardManager.getPrimaryClip().getItemAt(0).getText().toString());

        // Verify text has been properly pasted into the input field.
        Assert.assertEquals(PERFORM_ACTION_ERROR, "test text", mNodeInfo.getText().toString());
    }

    /** Test that the performAction for ACTION_SET_SELECTION works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_setProgress() throws Throwable {
        // Build a simple web page with an element that supports range values.
        setupTestWithHTML("<input id='id1' type='range' min='0' max='50' value='10'>");

        // Find the relevant node.
        int vvid = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Verify that bad requests have no effect.
        Assert.assertFalse(performActionOnUiThread(vvid, ACTION_SET_PROGRESS, null));
        Bundle bundle = new Bundle();
        Assert.assertFalse(performActionOnUiThread(vvid, ACTION_SET_PROGRESS, bundle));

        // Send a proper action and poll for update.
        bundle.putFloat(ACTION_ARGUMENT_PROGRESS_VALUE, 20);
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid,
                        ACTION_SET_PROGRESS,
                        bundle,
                        () -> {
                            var current =
                                    createAccessibilityNodeInfo(vvid).getRangeInfo().getCurrent();
                            return Math.abs(current - 20) < 0.01;
                        }));

        // Update node.
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, 20, mNodeInfo.getRangeInfo().getCurrent(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 50, mNodeInfo.getRangeInfo().getMax(), 0.01);

        // Send action that exceeds max value to test clamping.
        bundle.putFloat(ACTION_ARGUMENT_PROGRESS_VALUE, 55);
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid,
                        ACTION_SET_PROGRESS,
                        bundle,
                        () -> {
                            var current =
                                    createAccessibilityNodeInfo(vvid).getRangeInfo().getCurrent();
                            return Math.abs(current - 50) < 0.01;
                        }));

        // Update node.
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, 50, mNodeInfo.getRangeInfo().getCurrent(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 50, mNodeInfo.getRangeInfo().getMax(), 0.01);

        // Send action that is less than minimum value to test clamping.
        bundle.putFloat(ACTION_ARGUMENT_PROGRESS_VALUE, -5);
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid,
                        ACTION_SET_PROGRESS,
                        bundle,
                        () -> {
                            var current =
                                    createAccessibilityNodeInfo(vvid).getRangeInfo().getCurrent();
                            return Math.abs(current - 0) < 0.01;
                        }));

        // Update node.
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, 0, mNodeInfo.getRangeInfo().getCurrent(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 50, mNodeInfo.getRangeInfo().getMax(), 0.01);
    }

    /** Test that the performAction for ACTION_SET_SELECTION works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_nextHtmlElement() throws Throwable {
        // Build a simple web page with elements that can be traversed.
        setupTestWithHTML("<p id='id1'>Example1</p><p id='id2'>Example2</p>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfoCompat mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfoCompat mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Focus our first node.
        focusNode(vvid1);

        // Verify that bad requests have no effect.
        Assert.assertFalse(performActionOnUiThread(vvid1, ACTION_NEXT_HTML_ELEMENT, null));
        Bundle bundle = new Bundle();
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, null);
        Assert.assertFalse(performActionOnUiThread(vvid1, ACTION_NEXT_HTML_ELEMENT, bundle));
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, "landmark");
        Assert.assertFalse(performActionOnUiThread(vvid1, ACTION_NEXT_HTML_ELEMENT, bundle));

        // Send a proper action and poll for update.
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, "p");
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid1,
                        ACTION_NEXT_HTML_ELEMENT,
                        bundle,
                        () -> createAccessibilityNodeInfo(vvid2).isAccessibilityFocused()));

        // Send of test signal and update node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);

        // Verify results.
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());
    }

    /** Test that the performAction for ACTION_SET_SELECTION works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_previousHtmlElement() throws Throwable {
        // Build a simple web page with elements that can be traversed.
        setupTestWithHTML("<p id='id1'>Example1</p><p id='id2'>Example2</p>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfoCompat mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfoCompat mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Focus our second node.
        focusNode(vvid2);

        // Verify that bad requests have no effect.
        Assert.assertFalse(performActionOnUiThread(vvid2, ACTION_PREVIOUS_HTML_ELEMENT, null));
        Bundle bundle = new Bundle();
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, null);
        Assert.assertFalse(performActionOnUiThread(vvid2, ACTION_PREVIOUS_HTML_ELEMENT, bundle));
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, "landmark");
        Assert.assertFalse(performActionOnUiThread(vvid2, ACTION_PREVIOUS_HTML_ELEMENT, bundle));

        // Send a proper action and poll for update.
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, "p");
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid2,
                        ACTION_PREVIOUS_HTML_ELEMENT,
                        bundle,
                        () -> createAccessibilityNodeInfo(vvid1).isAccessibilityFocused()));

        // Send of test signal and update node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);

        // Verify results.
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());
    }

    /**
     * Test that the performAction for ACTION_ACCESSIBILITY_FOCUS works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_accessibilityFocus() throws Throwable {
        // Build a simple web page with elements that can be traversed.
        setupTestWithHTML("<p id='id1'>Example1</p><p id='id2'>Example2</p>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfoCompat mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfoCompat mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Send an action and poll for update.
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid1,
                        ACTION_ACCESSIBILITY_FOCUS,
                        null,
                        () -> createAccessibilityNodeInfo(vvid1).isAccessibilityFocused()));

        // Update nodes and verify results.
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());
    }

    /**
     * Test that the performAction for ACTION_CLEAR_ACCESSIBILITY_FOCUS works properly
     * with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_accessibilityClearFocus() throws Throwable {
        // Build a simple web page with elements that can be traversed.
        setupTestWithHTML("<p id='id1'>Example1</p><p id='id2'>Example2</p>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfoCompat mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfoCompat mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Send an action and poll for update.
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid1,
                        ACTION_ACCESSIBILITY_FOCUS,
                        null,
                        () -> createAccessibilityNodeInfo(vvid1).isAccessibilityFocused()));

        // Update nodes and verify results.
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());

        // Clear accessibility focus from the node and verify.
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid1,
                        ACTION_CLEAR_ACCESSIBILITY_FOCUS,
                        null,
                        () -> !createAccessibilityNodeInfo(vvid1).isAccessibilityFocused()));

        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());
    }

    /** Test that the performAction for ACTION_FOCUS works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_focus() throws Throwable {
        // Build a simple web page with elements that can be focused.
        setupTestWithHTML("<input type='text' id='id1'><input type='text' id='id2'>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfoCompat mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfoCompat mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Send an action and poll for update.
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid1,
                        ACTION_FOCUS,
                        null,
                        () -> createAccessibilityNodeInfo(vvid1).isFocused()));

        // Update nodes and verify results.
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isFocused());
    }

    /** Test that the performAction for ACTION_CLEAR_FOCUS works properly with accessibility. */
    @Test
    @SmallTest
    public void testPerformAction_clearFocus() throws Throwable {
        // Build a simple web page with elements that can be focused.
        setupTestWithHTML("<input type='text' id='id1'><input type='text' id='id2'>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfoCompat mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfoCompat mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Send an action and poll for update.
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid1,
                        ACTION_FOCUS,
                        null,
                        () -> createAccessibilityNodeInfo(vvid1).isFocused()));

        // Update nodes and verify results.
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isFocused());

        // Clear focus from the node and verify.
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid1,
                        ACTION_CLEAR_FOCUS,
                        null,
                        () -> !createAccessibilityNodeInfo(vvid1).isFocused()));

        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo1.isFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isFocused());
    }

    /** Test that the performAction for ACTION_SHOW_ON_SCREEN works properly with accessibility. */
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1294296")
    public void testPerformAction_showOnScreen() throws Throwable {
        // Build a simple web page with a scrollable view.
        setupTestFromFile("content/test/data/android/scroll_element_offscreen.html");

        // Find a node offscreen, which should have the Bundle extra and large Bounds.
        int vvid = waitForNodeMatching(sTextMatcher, "Example Text 77");
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertNotNull(NODE_EXTRAS_UNCLIPPED_ERROR, mNodeInfo.getExtras());
        Rect originalBounds = new Rect(-1, -1, -1, -1);
        mNodeInfo.getBoundsInScreen(originalBounds);
        Assert.assertTrue(BOUNDING_BOX_ERROR, originalBounds.top > 0);
        Assert.assertTrue(BOUNDING_BOX_ERROR, originalBounds.bottom > 0);
        Assert.assertTrue(
                OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN));

        // Send an action and poll for update.
        Assert.assertTrue(
                performActionOnUiThread(
                        vvid,
                        ACTION_SHOW_ON_SCREEN,
                        null,
                        () -> {
                            return !createAccessibilityNodeInfo(vvid)
                                    .getExtras()
                                    .containsKey(EXTRAS_KEY_OFFSCREEN);
                        }));
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Rect updatedBounds = new Rect(-1, -1, -1, -1);
        mNodeInfo.getBoundsInScreen(updatedBounds);

        // Verify the bounds have decreased (moved up), and the offscreen extra has been removed.
        Assert.assertTrue(BOUNDING_BOX_ERROR, originalBounds.top > updatedBounds.top);
        Assert.assertTrue(BOUNDING_BOX_ERROR, originalBounds.bottom > updatedBounds.bottom);
        Assert.assertFalse(
                OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN));
    }

    private void assertActionsContainNoScrolls(AccessibilityNodeInfoCompat nodeInfo) {
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_FORWARD));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_BACKWARD));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_UP));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_DOWN));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_LEFT));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_RIGHT));
    }

    private String thresholdError(int count, int max) {
        return THRESHOLD_ERROR + " Received " + count + ", but expected no more than: " + max;
    }

    private String lowThresholdError(int count) {
        return THRESHOLD_LOW_EVENT_COUNT_ERROR
                + " Received "
                + count
                + ", but expected at least: "
                + UNSUPPRESSED_EXPECTED_COUNT;
    }

    /**
     * Helper method to perform a series of events that trigger histograms being tracked.
     * @throws Throwable error on focusNode
     */
    private void performHistogramActions() throws Throwable {
        // Find the three text nodes.
        int vvId1 = waitForNodeMatching(sTextMatcher, "This is a test 1");
        int vvId2 = waitForNodeMatching(sTextMatcher, "This is a test 2");
        int vvId3 = waitForNodeMatching(sTextMatcher, "This is a test 3");
        AccessibilityNodeInfoCompat mNodeInfo1 = createAccessibilityNodeInfo(vvId1);
        AccessibilityNodeInfoCompat mNodeInfo2 = createAccessibilityNodeInfo(vvId2);
        AccessibilityNodeInfoCompat mNodeInfo3 = createAccessibilityNodeInfo(vvId3);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo3);

        // Focus each node in turn to generate events.
        focusNode(vvId1);
        focusNode(vvId2);
        focusNode(vvId3);

        // Signal end of test.
        mActivityTestRule.sendEndOfTestSignal();

        // Force recording of UMA histograms.
        mActivityTestRule.mWcax.forceRecordUMAHistogramsForTesting();
        mActivityTestRule.mWcax.forceRecordCacheUMAHistogramsForTesting();
    }
}
