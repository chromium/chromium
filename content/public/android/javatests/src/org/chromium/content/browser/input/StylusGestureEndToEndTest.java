// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doCallRealMethod;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.graphics.PointF;
import android.graphics.RectF;
import android.os.Build;
import android.view.inputmethod.DeleteGesture;
import android.view.inputmethod.DeleteRangeGesture;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.HandwritingGesture;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InsertGesture;
import android.view.inputmethod.JoinOrSplitGesture;
import android.view.inputmethod.RemoveSpaceGesture;
import android.view.inputmethod.SelectGesture;
import android.view.inputmethod.SelectRangeGesture;

import androidx.annotation.RequiresApi;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests the entire flow of performing a stylus gesture on a website. Uses JavaScript to get an
 * area of text, simulates a handwriting gesture object over that area and asserts that the correct
 * change has been made to the page.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({"enable-features=StylusRichGestures"})
@MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
public class StylusGestureEndToEndTest {
    @Rule public ImeActivityTestRule mRule = new ImeActivityTestRule();

    private InputConnection mWrappedInputConnection;
    private HandwritingGesture mHandwritingGesture;
    private static final String FALLBACK_TEXT = "this gesture failed";

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
    public void testSelectGesture() throws TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        RectF gestureRect = bounds.get(0);
        gestureRect.union(bounds.get(bounds.size() - 1));

        mHandwritingGesture =
                new SelectGesture.Builder()
                        .setGranularity(HandwritingGesture.GRANULARITY_WORD)
                        .setSelectionArea(gestureRect)
                        .setFallbackText(FALLBACK_TEXT)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWrappedInputConnection.performHandwritingGesture(
                            mHandwritingGesture, null, null);
                });

        // Get the text inside contenteditable1 and assert that it is the same.
        assertEquals(
                "\"hello world\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));
        // Get the currently selected text and assert that it is the contents of contenteditable1.
        assertEquals("\"hello world\"", runJavaScript("window.getSelection().toString();"));
    }

    @Test
    @MediumTest
    public void testInsertGesture() throws TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        PointF insertionPoint = new PointF(bounds.get(7).right, bounds.get(7).centerY());

        mHandwritingGesture =
                new InsertGesture.Builder()
                        .setInsertionPoint(insertionPoint)
                        .setTextToInsert("inserted")
                        .setFallbackText(FALLBACK_TEXT)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWrappedInputConnection.performHandwritingGesture(
                            mHandwritingGesture, null, null);
                });

        // Get the text inside contenteditable1 and assert that it contains the inserted text.
        assertEquals(
                "\"hello woinsertedrld\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));
    }

    @Test
    @LargeTest
    public void testDeleteGesture() throws TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        RectF gestureRect = bounds.get(0);
        gestureRect.union(bounds.get(6));

        mHandwritingGesture =
                new DeleteGesture.Builder()
                        .setGranularity(HandwritingGesture.GRANULARITY_WORD)
                        .setDeletionArea(gestureRect)
                        .setFallbackText(FALLBACK_TEXT)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWrappedInputConnection.performHandwritingGesture(
                            mHandwritingGesture, null, null);
                });

        // Get the text inside contenteditable1 and assert that it has been deleted.
        assertEquals(
                "\"world\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText"));
    }

    @Test
    @MediumTest
    public void testRemoveSpaceGesture() throws TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        RectF removalRect = bounds.get(2);
        removalRect.union(bounds.get(9));

        // The following reflection creates a new RemoveSpaceGesture for the space between "hello"
        // and "world" in contenteditable1.
        mHandwritingGesture =
                new RemoveSpaceGesture.Builder()
                        .setPoints(
                                new PointF(removalRect.left, removalRect.bottom),
                                new PointF(removalRect.right, removalRect.top))
                        .setFallbackText(FALLBACK_TEXT)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWrappedInputConnection.performHandwritingGesture(
                            mHandwritingGesture, null, null);
                });

        // Get the text inside contenteditable1 and assert that it contains the inserted text.
        assertEquals(
                "\"helloworld\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));
    }

    @Test
    @MediumTest
    public void testJoinOrSplitGesture() throws TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds("contenteditable1", "hello world");
        PointF joinOrSplitPoint = new PointF(bounds.get(5).centerX(), bounds.get(5).centerY());

        mHandwritingGesture =
                new JoinOrSplitGesture.Builder()
                        .setJoinOrSplitPoint(joinOrSplitPoint)
                        .setFallbackText(FALLBACK_TEXT)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWrappedInputConnection.performHandwritingGesture(
                            mHandwritingGesture, null, null);
                });

        // Get the text inside contenteditable1 and assert that it contains the inserted text.
        assertEquals(
                "\"helloworld\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));

        joinOrSplitPoint = new PointF(bounds.get(1).right, bounds.get(2).centerY());
        mHandwritingGesture =
                new JoinOrSplitGesture.Builder()
                        .setJoinOrSplitPoint(joinOrSplitPoint)
                        .setFallbackText(FALLBACK_TEXT)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWrappedInputConnection.performHandwritingGesture(
                            mHandwritingGesture, null, null);
                });

        // Get the text inside contenteditable1 and assert that it contains the inserted text.
        assertEquals(
                "\"he lloworld\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));
    }

    @Test
    @MediumTest
    public void testSelectRangeGesture() throws TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds(
                        "contenteditable1", "hello world\\ngoodbye world");
        // "orld"
        RectF startRect = bounds.get(7);
        startRect.union(bounds.get(10));
        // "goodb"
        RectF endRect = bounds.get(11);
        endRect.union(bounds.get(15));

        mHandwritingGesture =
                new SelectRangeGesture.Builder()
                        .setGranularity(HandwritingGesture.GRANULARITY_WORD)
                        .setSelectionStartArea(startRect)
                        .setSelectionEndArea(endRect)
                        .setFallbackText(FALLBACK_TEXT)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWrappedInputConnection.performHandwritingGesture(
                            mHandwritingGesture, null, null);
                });

        // Get the text inside contenteditable1 and assert that it is the same.
        assertEquals(
                "\"hello world\\ngoodbye world\"",
                runJavaScript("document.getElementById(\"contenteditable1\").innerText;"));

        assertEquals("\"world\\ngoodbye\"", runJavaScript("window.getSelection().toString();"));
    }

    @Test
    @MediumTest
    public void testDeleteRangeGesture() throws TimeoutException {
        List<RectF> bounds =
                initialiseElementAndGetCharacterBounds(
                        "contenteditable1", "hello world\\ngoodbye world");
        // "llo world"
        RectF startRect = bounds.get(2);
        startRect.union(bounds.get(10));
        // "good"
        RectF endRect = bounds.get(11);
        endRect.union(bounds.get(14));

        mHandwritingGesture =
                new DeleteRangeGesture.Builder()
                        .setGranularity(HandwritingGesture.GRANULARITY_WORD)
                        .setDeletionStartArea(startRect)
                        .setDeletionEndArea(endRect)
                        .setFallbackText(FALLBACK_TEXT)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWrappedInputConnection.performHandwritingGesture(
                            mHandwritingGesture, null, null);
                });

        // We have to remove the 0 width line break character that JavaScript leaves in the string.
        assertEquals(
                "\"world\"",
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
        runJavaScript(
                String.format(
                        "document.getElementById(\"%s\").focus() = \"%s\"", elementId, value));
        runJavaScript(
                String.format(
                        "document.getElementById(\"%s\").innerText = \"%s\"", elementId, value));
        int numChildNodes =
                Integer.parseInt(
                        runJavaScript(
                                String.format(
                                        "document.getElementById(\"%s\").childNodes.length",
                                        elementId)));
        // Iterate through all child text nodes of elementId, populating bounds for each character.
        List<RectF> bounds = new ArrayList<>();
        for (int child = 0; child < numChildNodes; child++) {
            String snippet =
                    String.format(
                            "document.getElementById(\"%s\").childNodes[%d].nodeName",
                            elementId, child);
            if ("\"#text\"".equals(runJavaScript(snippet))) {
                snippet =
                        String.format(
                                "document.getElementById(\"%s\").childNodes[%d].textContent.length",
                                elementId, child);
                int length = Integer.parseInt(runJavaScript(snippet));
                for (int i = 0; i < length; i++) {
                    String code =
                            String.format(
                                    "(function() {let el = document.getElementById(\"%s\"); "
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
                    bounds.add(
                            toScreenRectF(
                                    left,
                                    top,
                                    right,
                                    bottom,
                                    (WebContentsImpl) mRule.getWebContents()));
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

    private static RectF toScreenRectF(
        float left, float top, float right, float bottom, WebContentsImpl webContents) {
        // Convert from local CSS coordinates to absolute screen coordinates.
        RenderCoordinatesImpl rc = webContents.getRenderCoordinates();
        int[] screenLocation = new int[2];
        webContents.getViewAndroidDelegate().getContainerView().getLocationOnScreen(screenLocation);
        left = rc.fromLocalCssToPix(left) + screenLocation[0];
        top = rc.fromLocalCssToPix(top) + rc.getContentOffsetYPix() + screenLocation[1];
        right = rc.fromLocalCssToPix(right) + screenLocation[0];
        bottom = rc.fromLocalCssToPix(bottom) + rc.getContentOffsetYPixInt() + screenLocation[1];

        return new RectF(left, top, right, bottom);
    }
}
