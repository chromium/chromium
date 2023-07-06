// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doCallRealMethod;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import static org.chromium.content.browser.input.StylusTestHelper.FALLBACK_TEXT;
import static org.chromium.content.browser.input.StylusTestHelper.TARGET_PACKAGE;
import static org.chromium.content.browser.input.StylusTestHelper.getBuilderForClass;
import static org.chromium.content.browser.input.StylusTestHelper.getMethodsForClass;
import static org.chromium.content.browser.input.StylusTestHelper.toScreenRectF;

import android.graphics.PointF;
import android.graphics.RectF;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Tests the entire flow of performing a stylus gesture on a website. Uses JavaScript to get an
 * area of text, simulates a handwriting gesture object over that area and asserts that the correct
 * change has been made to the page.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({"enable-features=StylusRichGestures"})
@MinAndroidSdkLevel(34)
public class StylusGestureEndToEndTest {
    @Rule
    public ImeActivityTestRule mRule = new ImeActivityTestRule();

    private InputConnection mWrappedInputConnection;

    @Before
    public void setUp() throws Exception {
        mRule.setUpForUrl(ImeActivityTestRule.INPUT_FORM_HTML);
        EditorInfo editorInfo = new EditorInfo();
        ImeAdapterImpl imeAdapter = spy(mRule.getImeAdapter());
        doReturn(mRule.getConnection())
                .when(imeAdapter)
                .onCreateInputConnection(any(EditorInfo.class), any(boolean.class));
        doCallRealMethod().when(imeAdapter).onCreateInputConnection(any(EditorInfo.class));
        mWrappedInputConnection = imeAdapter.onCreateInputConnection(editorInfo);
    }

    @Test
    @LargeTest
    public void testSelectGesture()
            throws ClassNotFoundException, IllegalAccessException, InstantiationException,
                   InvocationTargetException, TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        RectF gestureRect = bounds.get(0);
        gestureRect.union(bounds.get(bounds.size() - 1));

        // The following reflection creates a new SelectGesture over the text in the
        // contenteditable1 element. It has word granularity and the fallback text defined in
        // FALLBACK_TEXT.
        Class builderClass = getBuilderForClass(Class.forName(TARGET_PACKAGE + "SelectGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity").invoke(builder, StylusGestureHandler.GRANULARITY_WORD);
        builderMethods.get("setSelectionArea").invoke(builder, gestureRect);
        builderMethods.get("setFallbackText").invoke(builder, FALLBACK_TEXT);
        Object gesture = builderMethods.get("build").invoke(builder);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Method performHandwritingGesture =
                    getMethodsForClass(mWrappedInputConnection.getClass())
                            .get("performHandwritingGesture");
            try {
                performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
            } catch (IllegalAccessException | InvocationTargetException e) {
                fail("Failed to call performHandwritingGesture");
            }
        });

        // Get the text inside contenteditable1 and assert that it is the same.
        assertEquals("\"hello world\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));
        // Get the currently selected text and assert that it is the contents of contenteditable1.
        assertEquals("\"hello world\"", runJavaScript("window.getSelection().toString();"));
    }

    @Test
    @MediumTest
    public void testInsertGesture()
            throws ClassNotFoundException, IllegalAccessException, InstantiationException,
                   InvocationTargetException, TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        PointF insertionPoint = new PointF(bounds.get(7).right, bounds.get(7).centerY());

        // The following reflection creates a new InsertGesture at the point in contenteditable1
        // between o and r in "world". It should insert the word "inserted" at that point.
        Class builderClass = getBuilderForClass(Class.forName(TARGET_PACKAGE + "InsertGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setInsertionPoint").invoke(builder, insertionPoint);
        builderMethods.get("setTextToInsert").invoke(builder, "inserted");
        builderMethods.get("setFallbackText").invoke(builder, FALLBACK_TEXT);
        Object gesture = builderMethods.get("build").invoke(builder);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Method performHandwritingGesture =
                    getMethodsForClass(mWrappedInputConnection.getClass())
                            .get("performHandwritingGesture");
            try {
                performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
            } catch (IllegalAccessException | InvocationTargetException e) {
                fail("Failed to call performHandwritingGesture");
            }
        });

        // Get the text inside contenteditable1 and assert that it contains the inserted text.
        assertEquals("\"hello woinsertedrld\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));
    }

    @Test
    @LargeTest
    public void testDeleteGesture()
            throws ClassNotFoundException, IllegalAccessException, InstantiationException,
                   InvocationTargetException, TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        RectF gestureRect = bounds.get(0);
        gestureRect.union(bounds.get(6));

        // The following reflection creates a new DeleteGesture over the area of contenteditable1
        // between the 1st and 7th characters. It has word granularity and the fallback text
        // defined in FALLBACK_TEXT.
        Class builderClass = getBuilderForClass(Class.forName(TARGET_PACKAGE + "DeleteGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity").invoke(builder, StylusGestureHandler.GRANULARITY_WORD);
        builderMethods.get("setDeletionArea").invoke(builder, gestureRect);
        builderMethods.get("setFallbackText").invoke(builder, FALLBACK_TEXT);
        Object gesture = builderMethods.get("build").invoke(builder);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Method performHandwritingGesture =
                    getMethodsForClass(mWrappedInputConnection.getClass())
                            .get("performHandwritingGesture");
            try {
                performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
            } catch (IllegalAccessException | InvocationTargetException e) {
                fail("Failed to call performHandwritingGesture");
            }
        });

        // Get the text inside contenteditable1 and assert that it has been deleted.
        assertEquals("\"world\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText"));
    }

    @Test
    @MediumTest
    public void testRemoveSpaceGesture()
            throws ClassNotFoundException, IllegalAccessException, InstantiationException,
                   InvocationTargetException, TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        RectF removalRect = bounds.get(2);
        removalRect.union(bounds.get(9));

        // The following reflection creates a new RemoveSpaceGesture for the space between "hello"
        // and "world" in contenteditable1.
        Class builderClass =
                getBuilderForClass(Class.forName(TARGET_PACKAGE + "RemoveSpaceGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setPoints")
                .invoke(builder, new PointF(removalRect.left, removalRect.bottom),
                        new PointF(removalRect.right, removalRect.top));
        builderMethods.get("setFallbackText").invoke(builder, FALLBACK_TEXT);
        Object gesture = builderMethods.get("build").invoke(builder);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Method performHandwritingGesture =
                    getMethodsForClass(mWrappedInputConnection.getClass())
                            .get("performHandwritingGesture");
            try {
                performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
            } catch (IllegalAccessException | InvocationTargetException e) {
                fail("Failed to call performHandwritingGesture");
            }
        });

        // Get the text inside contenteditable1 and assert that it contains the inserted text.
        assertEquals("\"helloworld\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));
    }

    @Test
    @MediumTest
    public void testJoinOrSplitGesture()
            throws ClassNotFoundException, IllegalAccessException, InstantiationException,
                   InvocationTargetException, TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        PointF joinOrSplitPoint = new PointF(bounds.get(5).centerX(), bounds.get(5).centerY());

        // The following reflection creates a new JoinOrSplitGesture for the space between "hello"
        // and "world" in contenteditable1.
        Class builderClass =
                getBuilderForClass(Class.forName(TARGET_PACKAGE + "JoinOrSplitGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setJoinOrSplitPoint").invoke(builder, joinOrSplitPoint);
        builderMethods.get("setFallbackText").invoke(builder, FALLBACK_TEXT);
        Object gesture = builderMethods.get("build").invoke(builder);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Method performHandwritingGesture =
                    getMethodsForClass(mWrappedInputConnection.getClass())
                            .get("performHandwritingGesture");
            try {
                performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
            } catch (IllegalAccessException | InvocationTargetException e) {
                fail("Failed to call performHandwritingGesture");
            }
        });

        // Get the text inside contenteditable1 and assert that it contains the inserted text.
        assertEquals("\"helloworld\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));

        // The following reflection creates a new JoinOrSplitGesture after the e in "helloworld" in
        // contenteditable1.
        joinOrSplitPoint = new PointF(bounds.get(1).right, bounds.get(2).centerY());
        builder = builderClass.newInstance();
        builderMethods.get("setJoinOrSplitPoint").invoke(builder, joinOrSplitPoint);
        builderMethods.get("setFallbackText").invoke(builder, FALLBACK_TEXT);
        Object gesture2 = builderMethods.get("build").invoke(builder);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Method performHandwritingGesture =
                    getMethodsForClass(mWrappedInputConnection.getClass())
                            .get("performHandwritingGesture");
            try {
                performHandwritingGesture.invoke(mWrappedInputConnection, gesture2, null, null);
            } catch (IllegalAccessException | InvocationTargetException e) {
                fail("Failed to call performHandwritingGesture");
            }
        });

        // Get the text inside contenteditable1 and assert that it contains the inserted text.
        assertEquals("\"he lloworld\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));
    }

    @Test
    @MediumTest
    public void testSelectRangeGesture()
            throws ClassNotFoundException, IllegalAccessException, InstantiationException,
                   InvocationTargetException, TimeoutException {
        List<RectF> bounds = initialiseElementAndGetCharacterBounds(
                "contenteditable1", "hello world\\ngoodbye world");
        // "orld"
        RectF startRect = bounds.get(7);
        startRect.union(bounds.get(10));
        // "goodb"
        RectF endRect = bounds.get(11);
        endRect.union(bounds.get(15));

        // The following reflection creates a new SelectRangeGesture over the area of the
        // contenteditable1 element containing the words "world" and "goodbye". These crosses a
        // line boundary. It has word granularity and the fallback text defined in FALLBACK_TEXT.
        Class builderClass =
                getBuilderForClass(Class.forName(TARGET_PACKAGE + "SelectRangeGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity").invoke(builder, StylusGestureHandler.GRANULARITY_WORD);
        builderMethods.get("setSelectionStartArea").invoke(builder, startRect);
        builderMethods.get("setSelectionEndArea").invoke(builder, endRect);
        builderMethods.get("setFallbackText").invoke(builder, FALLBACK_TEXT);
        Object gesture = builderMethods.get("build").invoke(builder);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Method performHandwritingGesture =
                    getMethodsForClass(mWrappedInputConnection.getClass())
                            .get("performHandwritingGesture");
            try {
                performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
            } catch (IllegalAccessException | InvocationTargetException e) {
                fail("Failed to call performHandwritingGesture");
            }
        });

        // Get the text inside contenteditable1 and assert that it is the same.
        assertEquals("\"hello world\\ngoodbye world\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));

        assertEquals("\"world\\ngoodbye\"", runJavaScript("window.getSelection().toString();"));
    }

    @Test
    @MediumTest
    public void testDeleteRangeGesture()
            throws ClassNotFoundException, IllegalAccessException, InstantiationException,
                   InvocationTargetException, TimeoutException {
        List<RectF> bounds = initialiseElementAndGetCharacterBounds(
                "contenteditable1", "hello world\\ngoodbye world");
        // "llo world"
        RectF startRect = bounds.get(2);
        startRect.union(bounds.get(10));
        // "good"
        RectF endRect = bounds.get(11);
        endRect.union(bounds.get(14));

        // The following reflection creates a new DeleteRangeGesture over the area of the
        // contenteditable1 element containing "hello world\ngoodbye". This should remove the first
        // line entirely leaving only the word "world" on a single line. It has word granularity
        // and the fallback text defined in FALLBACK_TEXT.
        Class builderClass =
                getBuilderForClass(Class.forName(TARGET_PACKAGE + "DeleteRangeGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity").invoke(builder, StylusGestureHandler.GRANULARITY_WORD);
        builderMethods.get("setDeletionStartArea").invoke(builder, startRect);
        builderMethods.get("setDeletionEndArea").invoke(builder, endRect);
        builderMethods.get("setFallbackText").invoke(builder, FALLBACK_TEXT);
        Object gesture = builderMethods.get("build").invoke(builder);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Method performHandwritingGesture =
                    getMethodsForClass(mWrappedInputConnection.getClass())
                            .get("performHandwritingGesture");
            try {
                performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
            } catch (IllegalAccessException | InvocationTargetException e) {
                fail("Failed to call performHandwritingGesture");
            }
        });

        // We have to remove the 0 width line break character that JavaScript leaves in the string.
        assertEquals("\"world\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;")
                        .replaceAll("\u00A0", ""));
    }

    /**
     * Sets the text of the specified element to the string value then uses javascript to get the
     * bounds of each individual character in the specified element.
     * @param elementId The ID of the element in the DOM containing the text (must not be <input>
     *                  or <textarea> as these do not report bounds for their selections.
     * @param value The text to set inside the element represented by elementId.
     * @return A list of rectangle bounds (one per character).
     */
    private List<RectF> initialiseElementAndGetCharacterBounds(String elementId, String value)
            throws TimeoutException {
        runJavaScript(String.format(
                "document.getElementById(\"%s\").focus() = \"%s\"", elementId, value));
        runJavaScript(String.format(
                "document.getElementById(\"%s\").innerText = \"%s\"", elementId, value));
        int numChildNodes = Integer.parseInt(runJavaScript(
                String.format("document.getElementById(\"%s\").childNodes.length", elementId)));
        // Iterate through all child text nodes of elementId, populating bounds for each character.
        List<RectF> bounds = new ArrayList<>();
        for (int child = 0; child < numChildNodes; child++) {
            if ("\"#text\"".equals(runJavaScript(
                        String.format("document.getElementById(\"%s\").childNodes[%d].nodeName",
                                elementId, child)))) {
                int length = Integer.parseInt(runJavaScript(String.format(
                        "document.getElementById(\"%s\").childNodes[%d].textContent.length",
                        elementId, child)));
                for (int i = 0; i < length; i++) {
                    String code =
                            String.format("(function() {let el = document.getElementById(\"%s\"); "
                                            + "let range = new Range(); "
                                            + "range.setStart(el.childNodes[%d], %d); "
                                            + "range.setEnd(el.childNodes[%d], %d); "
                                            + "window.getSelection().removeAllRanges(); "
                                            + "window.getSelection().addRange(range); "
                                            + "return window.getSelection().getRangeAt(0)"
                                            + ".getBoundingClientRect().%s;"
                                            + "})();",
                                    elementId, child, i, child, i + 1, "%s");
                    float left = Float.parseFloat(runJavaScript(String.format(code, "left")));
                    float top = Float.parseFloat(runJavaScript(String.format(code, "top")));
                    float right = Float.parseFloat(runJavaScript(String.format(code, "right")));
                    float bottom = Float.parseFloat(runJavaScript(String.format(code, "bottom")));
                    bounds.add(toScreenRectF(
                            left, top, right, bottom, (WebContentsImpl) mRule.getWebContents()));
                }
            }
        }
        runJavaScript("window.getSelection().removeAllRange();");
        return bounds;
    }

    /**
     * Helper method to make the tests slightly more readable. Runs a code snippet of JavaScript.
     * @param code The JavaScript code to execute.
     * @return The result of the JavaScript code.
     */
    private String runJavaScript(String code) throws TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(), code);
    }
}
