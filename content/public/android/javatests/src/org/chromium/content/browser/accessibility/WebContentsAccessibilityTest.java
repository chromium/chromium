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
import android.support.test.filters.MediumTest;
import android.text.InputType;
import android.text.Spannable;
import android.text.style.SuggestionSpan;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
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

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return wcax.getAccessibilityNodeProvider() != null;
            }
        });

        return wcax.getAccessibilityNodeProvider();
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
        AccessibilityNodeInfo textNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(textNode, null);
        final Bundle arguments = new Bundle();
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX, 0);
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH, 4);
        provider.addExtraDataToAccessibilityNodeInfo(
                textNodeVirtualViewId, textNode, EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, arguments);

        // It should return a result, but all of the rects will be the same because it hasn't
        // loaded inline text boxes yet.
        Bundle extras = textNode.getExtras();
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
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AccessibilityNodeInfo textNode =
                        provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
                provider.addExtraDataToAccessibilityNodeInfo(textNodeVirtualViewId, textNode,
                        EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, arguments);
                Bundle extras = textNode.getExtras();
                RectF[] result =
                        (RectF[]) extras.getParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
                return result.length == 4 && !result[0].equals(result[1]);
            }
        });

        // The final result should be the separate bounding box of all four characters.
        textNode = provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        provider.addExtraDataToAccessibilityNodeInfo(
                textNodeVirtualViewId, textNode, EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, arguments);
        extras = textNode.getExtras();
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
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return View.NO_ID != findNodeMatching(provider, View.NO_ID, matcher);
            }
        });

        int virtualViewId = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> findNodeMatching(provider, View.NO_ID, matcher));
        Assert.assertNotEquals(View.NO_ID, virtualViewId);
        return virtualViewId;
    }

    /**
     * Block until the tree of virtual views under |provider| has a node whose
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
     * Block until the tree of virtual views under |provider| has a node whose input type
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
     * Block until the tree of virtual views under |provider| has a node whose className equals
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
     * Ensure an edit field can be traversed with granularity while typing.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @DisabledTest(message = "crbug.com/991463")
    public void testNavigationWithinEditTextField() throws Throwable {
        // Load a really simple webpage.
        final String data = "<form>\n"
                + "  First name:<br>\n"
                + "  <input id=\"fn\" type=\"text\" value=\"Text\"><br>\n"
                + "</form>";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editFieldVirtualViewId =
                waitForNodeWithTextInputType(provider, InputType.TYPE_CLASS_TEXT);
        AccessibilityNodeInfo editTextNode =
                provider.createAccessibilityNodeInfo(editFieldVirtualViewId);

        // Assert we have got the correct node.
        Assert.assertNotEquals(editTextNode, null);
        Assert.assertEquals(editTextNode.getInputType(), InputType.TYPE_CLASS_TEXT);
        Assert.assertEquals(editTextNode.getText().toString(), "Text");

        // Add an accessibility delegate to capture TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY
        // events and store the most recent character index returned from that event.
        final MutableInt mostRecentCharIndex = new MutableInt(-1);
        mActivityTestRule.getContainerView().setAccessibilityDelegate(
                new View.AccessibilityDelegate() {
                    @Override
                    public boolean onRequestSendAccessibilityEvent(
                            ViewGroup host, View child, AccessibilityEvent event) {
                        if (event.getEventType()
                                == AccessibilityEvent
                                           .TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY) {
                            mostRecentCharIndex.value = event.getFromIndex();
                        }

                        // Return false so that an accessibility event is not actually sent.
                        return false;
                    }
                });

        boolean result1 = performActionOnUiThread(
                provider, editFieldVirtualViewId, AccessibilityNodeInfo.ACTION_FOCUS, null);
        boolean result2 = performActionOnUiThread(provider, editFieldVirtualViewId,
                AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);
        boolean result3 = performActionOnUiThread(provider, editFieldVirtualViewId,
                AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS, null);

        // Assert all actions are performed successfully.
        Assert.assertEquals(result1, true);
        Assert.assertEquals(result2, true);
        Assert.assertEquals(result3, true);

        while (!editTextNode.isFocused()) {
            Thread.sleep(1);
            editTextNode.recycle();
            editTextNode = provider.createAccessibilityNodeInfo(editFieldVirtualViewId);
        }

        Bundle args = new Bundle();
        // Set granularity to Character.
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);

        // Simulate swipe left.
        for (int i = 3; i >= 0; i--) {
            boolean result = performActionOnUiThread(provider, editFieldVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);
            // Assert that the index of the character traversed is correct.
            Assert.assertEquals(i, mostRecentCharIndex.value);
        }
        // Simulate swipe right.
        for (int i = 0; i <= 3; i++) {
            boolean result = performActionOnUiThread(provider, editFieldVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);
            // Assert that the index of the character traversed is correct.
            Assert.assertEquals(i, mostRecentCharIndex.value);
        }
    }

    private static boolean performActionOnUiThread(AccessibilityNodeProvider provider, int viewId,
            int action, Bundle args) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> provider.performAction(viewId, action, args));
    }

    /**
     * Text fields should expose ACTION_SET_TEXT
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void testEditTextFieldValidNoErrorMessage() {
        final String data = "<input type='text'><br>\n";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();
        int textNodeVirtualViewId = waitForNodeWithClassName(provider, "android.widget.EditText");

        AccessibilityNodeInfo textNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(textNode, null);
        Assert.assertEquals(textNode.isContentInvalid(), false);
        Assert.assertEquals(textNode.getError(), "");
    }

    /**
     * Test spelling error is encoded as a Spannable.
     **/
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
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
}
