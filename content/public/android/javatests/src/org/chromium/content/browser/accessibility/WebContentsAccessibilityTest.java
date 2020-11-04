// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.text.Spannable;
import android.text.style.SuggestionSpan;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.lang.reflect.Method;
import java.util.concurrent.ExecutionException;

/**
 * Tests for WebContentsAccessibility. Actually tests WebContentsAccessibilityImpl that
 * implements the interface.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebContentsAccessibilityTest {
    private interface AccessibilityNodeInfoMatcher {
        public boolean matches(AccessibilityNodeInfo node);
    }

    private static class MutableInt {
        public MutableInt(int initialValue) {
            value = initialValue;
        }

        public int value;
    }

    // Constant from AccessibilityNodeInfo defined in the L SDK.
    private static final int ACTION_SET_TEXT = 0x200000;

    // Member variables required for testing framework
    private AccessibilityNodeProvider mNodeProvider;
    private AccessibilityNodeInfo mNodeInfo;

    // Member variables used during unit tests involving single edit text field
    private MutableInt mTraverseFromIndex = new MutableInt(-1);
    private MutableInt mTraverseToIndex = new MutableInt(-1);
    private MutableInt mSelectionFromIndex = new MutableInt(-1);
    private MutableInt mSelectionToIndex = new MutableInt(-1);

    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    /*
     * Enable accessibility and wait until WebContentsAccessibility.getAccessibilityNodeProvider()
     * returns something not null.
     */
    private AccessibilityNodeProvider enableAccessibilityAndWaitForNodeProvider() {
        final WebContentsAccessibilityImpl wcax = mActivityTestRule.getWebContentsAccessibility();
        wcax.setState(true);
        wcax.setAccessibilityEnabledForTesting();

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(wcax.getAccessibilityNodeProvider(), Matchers.notNullValue());
        });

        return wcax.getAccessibilityNodeProvider();
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
    private int findNodeMatching(AccessibilityNodeProvider provider, int virtualViewId,
            AccessibilityNodeInfoMatcher matcher) {
        AccessibilityNodeInfo node = provider.createAccessibilityNodeInfo(virtualViewId);
        Assert.assertNotEquals(node, null);

        if (matcher.matches(node)) return virtualViewId;

        for (int i = 0; i < node.getChildCount(); i++) {
            int childId = getChildId(node, i);
            AccessibilityNodeInfo child = provider.createAccessibilityNodeInfo(childId);
            if (child != null) {
                int result = findNodeMatching(provider, childId, matcher);
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
    private int waitForNodeMatching(
            AccessibilityNodeProvider provider, AccessibilityNodeInfoMatcher matcher) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    findNodeMatching(provider, View.NO_ID, matcher), Matchers.not(View.NO_ID));
        });

        int virtualViewId = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> findNodeMatching(provider, View.NO_ID, matcher));
        Assert.assertNotEquals(View.NO_ID, virtualViewId);
        return virtualViewId;
    }

    /**
     * Block until the tree of virtual views under |mNodeProvider| has a node whose
     * text or contentDescription equals |text|. Returns the virtual view ID of
     * the matching node, if found, and asserts if not.
     */
    private int waitForNodeWithText(AccessibilityNodeProvider provider, String text) {
        return waitForNodeMatching(provider, new AccessibilityNodeInfoMatcher() {
            @Override
            public boolean matches(AccessibilityNodeInfo node) {
                return text.equals(node.getText()) || text.equals(node.getContentDescription());
            }
        });
    }

    /**
     * Block until the tree of virtual views under |mNodeProvider| has a node whose
     * text equals |text|. Returns the virtual view ID of the matching node, or asserts if not.
     */
    private int waitForNodeWithTextOnly(AccessibilityNodeProvider provider, String text) {
        return waitForNodeMatching(provider, new AccessibilityNodeInfoMatcher() {
            @Override
            public boolean matches(AccessibilityNodeInfo node) {
                return text.equals(node.getText());
            }
        });
    }

    /**
     * Block until the tree of virtual views under |mNodeProvider| has a node whose input type
     * is |type|. Returns the virtual view ID of the matching node, if found, and asserts if not.
     */
    @SuppressLint("NewApi")
    private int waitForNodeWithTextInputType(AccessibilityNodeProvider provider, int type) {
        return waitForNodeMatching(provider, new AccessibilityNodeInfoMatcher() {
            @Override
            public boolean matches(AccessibilityNodeInfo node) {
                return node.getInputType() == type;
            }
        });
    }

    /**
     * Block until the tree of virtual views under |mNodeProvider| has a node whose className equals
     * |className|. Returns the virtual view ID of the matching node, if found, and asserts if not.
     */
    private int waitForNodeWithClassName(AccessibilityNodeProvider provider, String className) {
        return waitForNodeMatching(provider, new AccessibilityNodeInfoMatcher() {
            @Override
            public boolean matches(AccessibilityNodeInfo node) {
                return className.equals(node.getClassName());
            }
        });
    }

    /**
     * Helper method to perform actions on the UI so we can then send accessibility events
     *
     * @param viewId int                            viewId set during setUpEditTextDelegate
     * @param action int                            desired AccessibilityNodeInfo action
     * @param args Bundle                           action bundle
     * @return boolean                              return value of performAction
     * @throws ExecutionException                   Error
     */
    private boolean performActionOnUiThread(int viewId, int action, Bundle args)
            throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mNodeProvider.performAction(viewId, action, args));
    }

    /**
     * Helper method to build a web page with an edit text for our set of tests, and find the
     * virtualViewId of that given field and return it
     *
     * @param htmlContent String        content of the web page
     * @param editTextValue String      value of the edit text field to find
     * @return int                      virtualViewId of the edit text identified
     */
    private int buildWebPageWithEditText(String htmlContent, String editTextValue) {
        // Load a simple page with an input and the text "Testing"
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(htmlContent));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mNodeProvider = enableAccessibilityAndWaitForNodeProvider();

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editTextVirtualViewId =
                waitForNodeWithTextInputType(mNodeProvider, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = mNodeProvider.createAccessibilityNodeInfo(editTextVirtualViewId);

        // Assert we have got the correct node.
        Assert.assertNotEquals(mNodeInfo, null);
        Assert.assertEquals(mNodeInfo.getInputType(), InputType.TYPE_CLASS_TEXT);
        Assert.assertEquals(mNodeInfo.getText().toString(), editTextValue);

        return editTextVirtualViewId;
    }

    /**
     * Helper method to build a web page with a contenteditable for our set of tests, and find the
     * virtualViewId of that given div and return it
     *
     * @param htmlContent String                content of the web page
     * @param contenteditableText String        value of the contenteditable div to find
     * @return int                              virtualViewId of the contenteditable identified
     */
    private int buildWebPageWithContentEditable(String htmlContent, String contenteditableText) {
        // Load a simple page with an input and the text "Testing"
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(htmlContent));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mNodeProvider = enableAccessibilityAndWaitForNodeProvider();

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int contenteditableVirtualViewId =
                waitForNodeWithClassName(mNodeProvider, "android.widget.EditText");
        mNodeInfo = mNodeProvider.createAccessibilityNodeInfo(contenteditableVirtualViewId);

        // Assert we have got the correct node.
        Assert.assertNotEquals(mNodeInfo, null);
        Assert.assertEquals(contenteditableText, mNodeInfo.getText().toString());

        return contenteditableVirtualViewId;
    }

    /**
     * Helper method to set up delegates on an edit text for testing. This is used in the tests
     * below that check our accessibility events are properly indexed. The editTextVirtualViewId
     * parameter should be the value returned from buildWebPageWithEditText
     *
     * @param editTextVirtualViewId int     virtualViewId of EditText to setup delegates on
     * @throws Throwable Error
     */
    private void setUpEditTextDelegate(int editTextVirtualViewId) throws Throwable {
        // Add an accessibility delegate to capture TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY and
        // TYPE_VIEW_TEXT_SELECTION_CHANGED events and store their ToIndex and FromIndex.
        // The delegate is set on the parent as WebContentsAccessibilityImpl sends events using the
        // parent.
        ((ViewGroup) mActivityTestRule.getContainerView().getParent())
                .setAccessibilityDelegate(new View.AccessibilityDelegate() {
                    @Override
                    public boolean onRequestSendAccessibilityEvent(
                            ViewGroup host, View child, AccessibilityEvent event) {
                        if (event.getEventType()
                                == AccessibilityEvent
                                           .TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY) {
                            mTraverseFromIndex.value = event.getFromIndex();
                            mTraverseToIndex.value = event.getToIndex();
                        } else if (event.getEventType()
                                == AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED) {
                            mSelectionFromIndex.value = event.getFromIndex();
                            mSelectionToIndex.value = event.getToIndex();
                        }

                        // Return false so that an accessibility event is not actually sent.
                        return false;
                    }
                });

        // Focus our field
        boolean result1 = performActionOnUiThread(
                editTextVirtualViewId, AccessibilityNodeInfo.ACTION_FOCUS, null);
        boolean result2 = performActionOnUiThread(
                editTextVirtualViewId, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);

        // Assert all actions are performed successfully.
        Assert.assertTrue(result1);
        Assert.assertTrue(result2);

        while (!mNodeInfo.isFocused()) {
            Thread.sleep(100);
            mNodeInfo.recycle();
            mNodeInfo = mNodeProvider.createAccessibilityNodeInfo(editTextVirtualViewId);
        }
    }

    /**
     * Helper method to refresh the |mNodeInfo| object by recycling, waiting 1 sec, then refresh.
     */
    private void refreshMNodeInfo(int virtualViewId) throws InterruptedException {
        mNodeInfo.recycle();
        Thread.sleep(1000);
        mNodeInfo = mNodeProvider.createAccessibilityNodeInfo(virtualViewId);
    }

    /**
     * Helper method to tear down the setup of our tests so we can start the next test clean
     */
    private void tearDown() {
        mNodeProvider = null;
        mNodeInfo = null;

        mTraverseFromIndex.value = -1;
        mTraverseToIndex.value = -1;
        mSelectionFromIndex.value = -1;
        mSelectionToIndex.value = -1;
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by character with selection mode off
     */
    @Test
    @MediumTest
    public void testEventIndices_SelectionOFF_CharacterGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing"
        int editTextVirtualViewId = buildWebPageWithEditText(
                "<input id=\"fn\" type=\"text\" value=\"Testing\">", "Testing");

        setUpEditTextDelegate(editTextVirtualViewId);

        // Set granularity to CHARACTER, with selection FALSE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);

        // Simulate swiping left (backward)
        for (int i = 7; i > 0; i--) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTraverseFromIndex.value);
            Assert.assertEquals(i, mTraverseToIndex.value);
            Assert.assertEquals(i - 1, mSelectionFromIndex.value);
            Assert.assertEquals(i - 1, mSelectionToIndex.value);
        }

        // Simulate swiping right (forward)
        for (int i = 0; i < 7; i++) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTraverseFromIndex.value);
            Assert.assertEquals(i + 1, mTraverseToIndex.value);
            Assert.assertEquals(i + 1, mSelectionFromIndex.value);
            Assert.assertEquals(i + 1, mSelectionToIndex.value);
        }

        tearDown();
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by character with selection mode on
     */
    @Test
    @LargeTest
    @Ignore("Skipping due to long run time")
    public void testEventIndices_SelectionON_CharacterGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing"
        int editTextVirtualViewId = buildWebPageWithEditText(
                "<input id=\"fn\" type=\"text\" value=\"Testing\">", "Testing");

        setUpEditTextDelegate(editTextVirtualViewId);

        // Set granularity to CHARACTER, with selection TRUE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping left (backward) (adds to selections)
        for (int i = 7; i > 0; i--) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTraverseFromIndex.value);
            Assert.assertEquals(i, mTraverseToIndex.value);
            Assert.assertEquals(7, mSelectionFromIndex.value);
            Assert.assertEquals(i - 1, mSelectionToIndex.value);

            refreshMNodeInfo(editTextVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(i - 1, mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(7, mNodeInfo.getTextSelectionEnd());
        }

        // Simulate swiping right (forward) (removes from selection)
        for (int i = 0; i < 7; i++) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTraverseFromIndex.value);
            Assert.assertEquals(i + 1, mTraverseToIndex.value);
            Assert.assertEquals(7, mSelectionFromIndex.value);
            Assert.assertEquals(i + 1, mSelectionToIndex.value);

            refreshMNodeInfo(editTextVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(i + 1, mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(7, mNodeInfo.getTextSelectionEnd());
        }

        // Turn selection mode off and traverse to beginning so we can select forwards
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
        for (int i = 7; i > 0; i--) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);
        }

        // Turn selection mode on
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping right (forward) (adds to selection)
        for (int i = 0; i < 7; i++) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTraverseFromIndex.value);
            Assert.assertEquals(i + 1, mTraverseToIndex.value);
            Assert.assertEquals(0, mSelectionFromIndex.value);
            Assert.assertEquals(i + 1, mSelectionToIndex.value);

            refreshMNodeInfo(editTextVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(0, mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(i + 1, mNodeInfo.getTextSelectionEnd());
        }

        // Simulate swiping left (backward) (removes from selections)
        for (int i = 7; i > 0; i--) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTraverseFromIndex.value);
            Assert.assertEquals(i, mTraverseToIndex.value);
            Assert.assertEquals(0, mSelectionFromIndex.value);
            Assert.assertEquals(i - 1, mSelectionToIndex.value);

            refreshMNodeInfo(editTextVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(0, mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(i - 1, mNodeInfo.getTextSelectionEnd());
        }

        tearDown();
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by word with selection mode off
     */
    @Test
    @MediumTest
    public void testEventIndices_SelectionOFF_WordGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing this output is correct"
        int editTextVirtualViewId = buildWebPageWithEditText(
                "<input id=\"fn\" type=\"text\" value=\"Testing this output is correct\">",
                "Testing this output is correct");

        setUpEditTextDelegate(editTextVirtualViewId);

        // Set granularity to WORD, with selection FALSE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);

        int[] wordStarts = new int[] {0, 8, 13, 20, 23};
        int[] wordEnds = new int[] {7, 12, 19, 22, 30};

        // Simulate swiping left (backward) through all 5 words, check indices along the way
        for (int i = 4; i >= 0; --i) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTraverseFromIndex.value);
            Assert.assertEquals(wordEnds[i], mTraverseToIndex.value);
            Assert.assertEquals(wordStarts[i], mSelectionFromIndex.value);
            Assert.assertEquals(wordStarts[i], mSelectionToIndex.value);
        }

        // Simulate swiping right (forward) through all 5 words, check indices along the way
        for (int i = 0; i < 5; ++i) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTraverseFromIndex.value);
            Assert.assertEquals(wordEnds[i], mTraverseToIndex.value);
            Assert.assertEquals(wordEnds[i], mSelectionFromIndex.value);
            Assert.assertEquals(wordEnds[i], mSelectionToIndex.value);
        }

        tearDown();
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by word with selection mode on
     */
    @Test
    @LargeTest
    @Ignore("Skipping due to long run time")
    public void testEventIndices_SelectionON_WordGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing this output is correct"
        int editTextVirtualViewId = buildWebPageWithEditText(
                "<input id=\"fn\" type=\"text\" value=\"Testing this output is correct\">",
                "Testing this output is correct");

        setUpEditTextDelegate(editTextVirtualViewId);

        // Set granularity to WORD, with selection TRUE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        int[] wordStarts = new int[] {0, 8, 13, 20, 23};
        int[] wordEnds = new int[] {7, 12, 19, 22, 30};

        // Simulate swiping left (backward, adds to selection) through all 5 words, check indices
        for (int i = 4; i >= 0; --i) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTraverseFromIndex.value);
            Assert.assertEquals(wordEnds[i], mTraverseToIndex.value);
            Assert.assertEquals(30, mSelectionFromIndex.value);
            Assert.assertEquals(wordStarts[i], mSelectionToIndex.value);

            refreshMNodeInfo(editTextVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(wordStarts[i], mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(30, mNodeInfo.getTextSelectionEnd());
        }

        // Simulate swiping right (forward, removes selection) through all 5 words, check indices
        for (int i = 0; i < 5; ++i) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTraverseFromIndex.value);
            Assert.assertEquals(wordEnds[i], mTraverseToIndex.value);
            Assert.assertEquals(30, mSelectionFromIndex.value);
            Assert.assertEquals(wordEnds[i], mSelectionToIndex.value);

            refreshMNodeInfo(editTextVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(wordEnds[i], mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(30, mNodeInfo.getTextSelectionEnd());
        }

        // Turn selection mode off and traverse to beginning so we can select forwards
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
        for (int i = 4; i >= 0; i--) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);
        }

        // Turn selection mode on
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping right (forward) (adds to selection)
        for (int i = 0; i < 5; ++i) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTraverseFromIndex.value);
            Assert.assertEquals(wordEnds[i], mTraverseToIndex.value);
            Assert.assertEquals(0, mSelectionFromIndex.value);
            Assert.assertEquals(wordEnds[i], mSelectionToIndex.value);

            refreshMNodeInfo(editTextVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(0, mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(wordEnds[i], mNodeInfo.getTextSelectionEnd());
        }

        // Simulate swiping left (backward) (removes from selections)
        for (int i = 4; i >= 0; --i) {
            performActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTraverseFromIndex.value);
            Assert.assertEquals(wordEnds[i], mTraverseToIndex.value);
            Assert.assertEquals(0, mSelectionFromIndex.value);
            Assert.assertEquals(wordStarts[i], mSelectionToIndex.value);

            refreshMNodeInfo(editTextVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(0, mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(wordStarts[i], mNodeInfo.getTextSelectionEnd());
        }

        tearDown();
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating a
     * contenteditable by character with selection mode on.
     */
    @Test
    @LargeTest
    @Ignore("Skipping due to long run time")
    public void testEventIndices_contenteditable_SelectionON_CharacterGranularity()
            throws Throwable {
        int contentEditableVirtualViewId =
                buildWebPageWithContentEditable("<div contenteditable>Testing</div>", "Testing");

        setUpEditTextDelegate(contentEditableVirtualViewId);

        // Move cursor to the end of the field for consistency.
        Bundle moveArgs = new Bundle();
        moveArgs.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
        for (int i = 7; i > 0; i--) {
            performActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, moveArgs);
        }

        // Set granularity to CHARACTER, with selection TRUE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping left (backward) (adds to selections)
        for (int i = 7; i > 0; i--) {
            performActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTraverseFromIndex.value);
            Assert.assertEquals(i, mTraverseToIndex.value);
            Assert.assertEquals(7, mSelectionFromIndex.value);
            Assert.assertEquals(i - 1, mSelectionToIndex.value);

            refreshMNodeInfo(contentEditableVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(i - 1, mNodeInfo.getTextSelectionEnd());
            Assert.assertEquals(7, mNodeInfo.getTextSelectionStart());
        }

        // Simulate swiping right (forward) (removes from selection)
        for (int i = 0; i < 7; i++) {
            performActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTraverseFromIndex.value);
            Assert.assertEquals(i + 1, mTraverseToIndex.value);
            Assert.assertEquals(7, mSelectionFromIndex.value);
            Assert.assertEquals(i + 1, mSelectionToIndex.value);

            refreshMNodeInfo(contentEditableVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(i + 1, mNodeInfo.getTextSelectionEnd());
            Assert.assertEquals(7, mNodeInfo.getTextSelectionStart());
        }

        // Turn selection mode off and traverse to beginning so we can select forwards
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
        for (int i = 7; i > 0; i--) {
            performActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);
        }

        // Turn selection mode on
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping right (forward) (adds to selection)
        for (int i = 0; i < 7; i++) {
            performActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTraverseFromIndex.value);
            Assert.assertEquals(i + 1, mTraverseToIndex.value);
            Assert.assertEquals(0, mSelectionFromIndex.value);
            Assert.assertEquals(i + 1, mSelectionToIndex.value);

            refreshMNodeInfo(contentEditableVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(0, mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(i + 1, mNodeInfo.getTextSelectionEnd());
        }

        // Simulate swiping left (backward) (removes from selections)
        for (int i = 7; i > 0; i--) {
            performActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTraverseFromIndex.value);
            Assert.assertEquals(i, mTraverseToIndex.value);
            Assert.assertEquals(0, mSelectionFromIndex.value);
            Assert.assertEquals(i - 1, mSelectionToIndex.value);

            refreshMNodeInfo(contentEditableVirtualViewId);

            Assert.assertTrue(mNodeInfo.isEditable());
            Assert.assertEquals(0, mNodeInfo.getTextSelectionStart());
            Assert.assertEquals(i - 1, mNodeInfo.getTextSelectionEnd());
        }

        tearDown();
    }

    /**
     * Text fields should expose ACTION_SET_TEXT
     */
    @Test
    @MediumTest
    public void testTextFieldExposesActionSetText() {
        // Load a web page with a text field.
        final String data = "<h1>Simple test page</h1>"
                + "<section><input type=text placeholder=Text></section>";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();
        int textNodeVirtualViewId = waitForNodeWithClassName(provider, "android.widget.EditText");
        AccessibilityNodeInfo textNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(textNode, null);
        for (AccessibilityNodeInfo.AccessibilityAction action : textNode.getActionList()) {
            if (action.getId() == ACTION_SET_TEXT) return;
        }
        Assert.fail("ACTION_SET_TEXT not found");
    }

    /**
     * ContentEditable elements should get a class name of EditText.
     **/
    @Test
    @MediumTest
    public void testContentEditableClassName() {
        final String data = "<div contenteditable>Edit This</div>";

        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();
        int textNodeVirtualViewId = waitForNodeWithClassName(provider, "android.widget.EditText");
        AccessibilityNodeInfo editableNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotNull(editableNode);
        Assert.assertEquals(editableNode.isEditable(), true);
        Assert.assertEquals(editableNode.getText().toString(), "Edit This");
    }

    /**
     * Tests presence of ContentInvalid attribute and correctness of
     * error message given aria-invalid = true
     **/
    @Test
    @MediumTest
    public void testEditTextFieldAriaInvalidTrueErrorMessage() {
        final String data = "<form>\n"
                + "  First name:<br>\n"
                + "  <input id='fn' type='text' aria-invalid='true'><br>\n"
                + "<input type='submit'><br>"
                + "</form>";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();
        int textNodeVirtualViewId = waitForNodeWithClassName(provider, "android.widget.EditText");
        AccessibilityNodeInfo textNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(textNode, null);
        Assert.assertEquals(textNode.isContentInvalid(), true);
        Assert.assertEquals(textNode.getError(), "Invalid entry");
    }

    /**
     * Tests presence of ContentInvalid attribute and correctness of
     * error message given aria-invalid = spelling
     **/
    @Test
    @MediumTest
    public void testEditTextFieldAriaInvalidSpellingErrorMessage() {
        final String data = "<input type='text' aria-invalid='spelling'><br>\n";

        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();
        int textNodeVirtualViewId = waitForNodeWithClassName(provider, "android.widget.EditText");
        AccessibilityNodeInfo textNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(textNode, null);
        Assert.assertEquals(textNode.isContentInvalid(), true);
        Assert.assertEquals(textNode.getError(), "Invalid spelling");
    }

    /**
     * Tests presence of ContentInvalid attribute and correctness of
     * error message given aria-invalid = grammar
     **/
    @Test
    @MediumTest
    public void testEditTextFieldAriaInvalidGrammarErrorMessage() {
        final String data = "<input type='text' aria-invalid='grammar'><br>\n";

        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();
        int textNodeVirtualViewId = waitForNodeWithClassName(provider, "android.widget.EditText");
        AccessibilityNodeInfo textNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(textNode, null);
        Assert.assertEquals(textNode.isContentInvalid(), true);
        Assert.assertEquals(textNode.getError(), "Invalid grammar");
    }

    /**
     * Tests ContentInvalid is false and empty error message for well-formed input
     **/
    @Test
    @MediumTest
    public void testEditTextFieldValidNoErrorMessage() {
        final String data = "<input type='text'><br>\n";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();
        int textNodeVirtualViewId = waitForNodeWithClassName(provider, "android.widget.EditText");

        AccessibilityNodeInfo textNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(null, textNode);
        Assert.assertFalse(textNode.isContentInvalid());
        Assert.assertNull(textNode.getError());
    }

    /**
     * Test spelling error is encoded as a Spannable.
     **/
    @Test
    @MediumTest
    public void testSpellingError() {
        // Load a web page containing a text field with one misspelling.
        // Note that for content_shell, no spelling suggestions are enabled
        // by default.
        final String data = "<input type='text' value='one wordd has an error'>";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();
        int textNodeVirtualViewId = waitForNodeWithClassName(provider, "android.widget.EditText");

        // Call a test API to explicitly add a spelling error in the same format as
        // would be generated if spelling correction was enabled.
        final WebContentsAccessibilityImpl wcax = mActivityTestRule.getWebContentsAccessibility();
        wcax.addSpellingErrorForTesting(textNodeVirtualViewId, 4, 9);

        // Clear our cache for this node.
        wcax.clearNodeInfoCacheForGivenId(textNodeVirtualViewId);

        // Now get that AccessibilityNodeInfo and retrieve its text.
        AccessibilityNodeInfo textNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(textNode, null);
        CharSequence text = textNode.getText();
        Assert.assertEquals(text.toString(), "one wordd has an error");

        // Assert that the text has a SuggestionSpan surrounding the proper word.
        Assert.assertTrue(text instanceof Spannable);
        Spannable spannable = (Spannable) text;
        Object spans[] = spannable.getSpans(0, text.length(), Object.class);
        boolean foundSuggestionSpan = false;
        for (Object span : spans) {
            if (span instanceof SuggestionSpan) {
                Assert.assertEquals(4, spannable.getSpanStart(span));
                Assert.assertEquals(9, spannable.getSpanEnd(span));
                foundSuggestionSpan = true;
            }
        }
        Assert.assertTrue(foundSuggestionSpan);
    }

    /**
     * Test Android O API to retrieve character bounds from an accessible node.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @TargetApi(Build.VERSION_CODES.O)
    public void testAddExtraDataToAccessibilityNodeInfo() {
        // Load a really simple webpage.
        final String data = "<h1>Simple test page</h1>"
                + "<section><p>Text</p></section>";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();

        // Wait until we find a node in the accessibility tree with the text "Text".
        final int textNodeVirtualViewId = waitForNodeWithText(provider, "Text");

        // Now call the API we want to test - addExtraDataToAccessibilityNodeInfo.
        final AccessibilityNodeInfo initialTextNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(initialTextNode, null);
        final Bundle arguments = new Bundle();
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX, 0);
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH, 4);

        // addExtraDataToAccessibilityNodeInfo() will end up calling RenderFrameHostImpl's method
        // AccessibilityPerformAction() in the C++ code, which needs to be run from the UI thread.
        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                provider.addExtraDataToAccessibilityNodeInfo(textNodeVirtualViewId, initialTextNode,
                        EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, arguments);
            }
        });

        // It should return a result, but all of the rects will be the same because it hasn't
        // loaded inline text boxes yet.
        Bundle extras = initialTextNode.getExtras();
        RectF[] result =
                (RectF[]) extras.getParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
        Assert.assertNotEquals(result, null);
        Assert.assertEquals(result.length, 4);
        Assert.assertEquals(result[0], result[1]);
        Assert.assertEquals(result[0], result[2]);
        Assert.assertEquals(result[0], result[3]);

        // The role string should be a camel cased programmatic identifier.
        CharSequence roleString = extras.getCharSequence("AccessibilityNodeInfo.chromeRole");
        Assert.assertEquals("paragraph", roleString.toString());

        // The data needed for text character locations loads asynchronously. Block until
        // it successfully returns the character bounds.
        CriteriaHelper.pollUiThread(() -> {
            AccessibilityNodeInfo textNode =
                    provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
            provider.addExtraDataToAccessibilityNodeInfo(textNodeVirtualViewId, textNode,
                    EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, arguments);
            Bundle textNodeExtras = textNode.getExtras();
            RectF[] textNodeResults = (RectF[]) textNodeExtras.getParcelableArray(
                    EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
            Criteria.checkThat(textNodeResults, Matchers.arrayWithSize(4));
            Criteria.checkThat(textNodeResults[0], Matchers.not(textNodeResults[1]));
        });

        // The final result should be the separate bounding box of all four characters.
        final AccessibilityNodeInfo finalTextNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                provider.addExtraDataToAccessibilityNodeInfo(textNodeVirtualViewId, finalTextNode,
                        EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, arguments);
            }
        });
        extras = finalTextNode.getExtras();
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

    /**
     * Ensure scrolling |AccessibilityNodeInfo| actions are not added unless the node is
     * specifically user scrollable, and not just programmatically scrollable.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @TargetApi(Build.VERSION_CODES.M)
    public void testAccessibilityNodeInfo_Actions_OverflowHidden() throws Throwable {
        // Build a simple web page with a div and overflow:hidden
        String htmlContent =
                "<div title=\"1234\" style=\"overflow:hidden; width: 200px; height: 50px\">\n"
                + "  <p>Example Paragraph 1</p>\n"
                + "  <p>Example Paragraph 2</p>\n"
                + "</div>";

        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(htmlContent));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mNodeProvider = enableAccessibilityAndWaitForNodeProvider();

        // Define our root node and paragraph node IDs by looking for their text.
        int virtualViewIdDiv = waitForNodeWithTextOnly(mNodeProvider, "1234");
        int virtualViewIdP1 = waitForNodeWithTextOnly(mNodeProvider, "Example Paragraph 1");
        int virtualViewIdP2 = waitForNodeWithTextOnly(mNodeProvider, "Example Paragraph 2");

        // Get the |AccessibilityNodeInfo| objects for our nodes.
        AccessibilityNodeInfo nodeInfoDiv =
                mNodeProvider.createAccessibilityNodeInfo(virtualViewIdDiv);
        AccessibilityNodeInfo nodeInfoP1 =
                mNodeProvider.createAccessibilityNodeInfo(virtualViewIdP1);
        AccessibilityNodeInfo nodeInfoP2 =
                mNodeProvider.createAccessibilityNodeInfo(virtualViewIdP2);

        // Assert we have the correct nodes.
        Assert.assertNotEquals(nodeInfoDiv, null);
        Assert.assertNotEquals(nodeInfoP1, null);
        Assert.assertNotEquals(nodeInfoP2, null);
        Assert.assertEquals(nodeInfoDiv.getText().toString(), "1234");
        Assert.assertEquals(nodeInfoP1.getText().toString(), "Example Paragraph 1");
        Assert.assertEquals(nodeInfoP2.getText().toString(), "Example Paragraph 2");

        // Assert the scroll actions are not present in any of the objects.
        assertActionsContainNoScrolls(nodeInfoDiv);
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Traverse to the next node, then re-assert.
        performActionOnUiThread(
                virtualViewIdDiv, AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, new Bundle());
        assertActionsContainNoScrolls(nodeInfoDiv);
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Repeat.
        performActionOnUiThread(
                virtualViewIdP1, AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, new Bundle());
        assertActionsContainNoScrolls(nodeInfoDiv);
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        tearDown();
    }

    /**
     * Ensure scrolling |AccessibilityNodeInfo| actions are added unless if the node is user
     * scrollable.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @TargetApi(Build.VERSION_CODES.M)
    public void testAccessibilityNodeInfo_Actions_OverflowScroll() throws Throwable {
        // Build a simple web page with a div and overflow:scroll
        String htmlContent =
                "<div title=\"1234\" style=\"overflow:scroll; width: 200px; height: 50px\">\n"
                + "  <p>Example Paragraph 1</p>\n"
                + "  <p>Example Paragraph 2</p>\n"
                + "</div>";

        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(htmlContent));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mNodeProvider = enableAccessibilityAndWaitForNodeProvider();

        // Define our root node and paragraph node IDs by looking for their text.
        int virtualViewIdDiv = waitForNodeWithTextOnly(mNodeProvider, "1234");
        int virtualViewIdP1 = waitForNodeWithTextOnly(mNodeProvider, "Example Paragraph 1");
        int virtualViewIdP2 = waitForNodeWithTextOnly(mNodeProvider, "Example Paragraph 2");

        // Get the |AccessibilityNodeInfo| objects for our nodes.
        AccessibilityNodeInfo nodeInfoDiv =
                mNodeProvider.createAccessibilityNodeInfo(virtualViewIdDiv);
        AccessibilityNodeInfo nodeInfoP1 =
                mNodeProvider.createAccessibilityNodeInfo(virtualViewIdP1);
        AccessibilityNodeInfo nodeInfoP2 =
                mNodeProvider.createAccessibilityNodeInfo(virtualViewIdP2);

        // Assert we have the correct nodes.
        Assert.assertNotEquals(nodeInfoDiv, null);
        Assert.assertNotEquals(nodeInfoP1, null);
        Assert.assertNotEquals(nodeInfoP2, null);
        Assert.assertEquals(nodeInfoDiv.getText().toString(), "1234");
        Assert.assertEquals(nodeInfoP1.getText().toString(), "Example Paragraph 1");
        Assert.assertEquals(nodeInfoP2.getText().toString(), "Example Paragraph 2");

        // Assert the scroll actions ARE present for our div node, but not the others.
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD));
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN));
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Traverse to the next node, then re-assert.
        performActionOnUiThread(
                virtualViewIdDiv, AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, new Bundle());
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD));
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN));
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Repeat.
        performActionOnUiThread(
                virtualViewIdP1, AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, new Bundle());
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD));
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN));
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        tearDown();
    }

    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @TargetApi(Build.VERSION_CODES.M)
    private void assertActionsContainNoScrolls(AccessibilityNodeInfo nodeInfo) {
        Assert.assertFalse(nodeInfo.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD));
        Assert.assertFalse(nodeInfo.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_BACKWARD));
        Assert.assertFalse(nodeInfo.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_UP));
        Assert.assertFalse(nodeInfo.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN));
        Assert.assertFalse(nodeInfo.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_LEFT));
        Assert.assertFalse(nodeInfo.getActionList().contains(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_RIGHT));
    }
}
