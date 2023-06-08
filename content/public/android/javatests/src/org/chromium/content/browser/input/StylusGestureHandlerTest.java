// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doCallRealMethod;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.blink.mojom.StylusWritingGestureAction;
import org.chromium.blink.mojom.StylusWritingGestureData;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.gfx.mojom.Rect;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Tests for using a StylusGestureHandler as an InvocationHandler in place of an InputConnection in
 * ImeAdapterImpl#onCreateInputConnection.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({"enable-features=StylusRichGestures"})
@MinAndroidSdkLevel(34)
public class StylusGestureHandlerTest {
    @Rule
    public ImeActivityTestRule mRule = new ImeActivityTestRule();

    private static final String TARGET_PACKAGE = "android.view.inputmethod.";
    private static final String FALLBACK_TEXT = "this gesture failed";
    private static final String GESTURE_TYPE_HISTOGRAM = "InputMethod.StylusHandwriting.Gesture";

    private InputConnection mWrappedInputConnection;
    private StylusWritingGestureData mLastGestureData;

    @Before
    public void setUp() throws Exception {
        mRule.setUpForUrl(ImeActivityTestRule.INPUT_FORM_HTML);
        mWrappedInputConnection =
                StylusGestureHandler.maybeProxyInputConnection(mRule.getInputConnection(),
                        (gesture) -> mLastGestureData = gesture.getGestureData());
    }

    @Test
    @MediumTest
    public void testOtherCallsAreRoutedToInputConnection() {
        assertEquals(mRule.getInputConnection().getHandler(), mWrappedInputConnection.getHandler());
    }

    @Test
    @MediumTest
    public void testSelectGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                GESTURE_TYPE_HISTOGRAM, StylusGestureHandler.UmaGestureType.SELECT);
        Class builderClass = getBuilderForClass(Class.forName(TARGET_PACKAGE + "SelectGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity")
                .invoke(builder, StylusGestureHandler.GRANULARITY_CHARACTER);
        builderMethods.get("setSelectionArea").invoke(builder, new RectF(0, 0, 10, 10));
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
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.SELECT_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(0, 5, 0, 0), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(10, 5, 0, 0), mLastGestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testInsertGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                GESTURE_TYPE_HISTOGRAM, StylusGestureHandler.UmaGestureType.INSERT);
        Class builderClass = getBuilderForClass(Class.forName(TARGET_PACKAGE + "InsertGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setTextToInsert").invoke(builder, "Foo");
        builderMethods.get("setInsertionPoint").invoke(builder, new PointF(15, 31));
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
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.ADD_SPACE_OR_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(15, 31, 0, 0), mLastGestureData.startRect);
        assertNull(mLastGestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(mLastGestureData.textAlternative));
        assertEquals("Foo", toJavaString(mLastGestureData.textToInsert));
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testDeleteGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                GESTURE_TYPE_HISTOGRAM, StylusGestureHandler.UmaGestureType.DELETE);
        Class builderClass = getBuilderForClass(Class.forName(TARGET_PACKAGE + "DeleteGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity").invoke(builder, StylusGestureHandler.GRANULARITY_WORD);
        builderMethods.get("setDeletionArea").invoke(builder, new RectF(0, 0, 10, 10));
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
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.DELETE_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.WORD,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(0, 5, 0, 0), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(10, 5, 0, 0), mLastGestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testRemoveSpaceGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                GESTURE_TYPE_HISTOGRAM, StylusGestureHandler.UmaGestureType.REMOVE_SPACE);
        Class builderClass =
                getBuilderForClass(Class.forName(TARGET_PACKAGE + "RemoveSpaceGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setPoints").invoke(builder, new PointF(51, 25), new PointF(105, 30));
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
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.REMOVE_SPACES, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(51, 25, 0, 0), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(105, 30, 0, 0), mLastGestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testJoinOrSplitGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                GESTURE_TYPE_HISTOGRAM, StylusGestureHandler.UmaGestureType.JOIN_OR_SPLIT);
        Class builderClass =
                getBuilderForClass(Class.forName(TARGET_PACKAGE + "JoinOrSplitGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setJoinOrSplitPoint").invoke(builder, new PointF(1, 19));
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
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.SPLIT_OR_MERGE, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(1, 19, 0, 0), mLastGestureData.startRect);
        assertNull(mLastGestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testSelectRangeGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                GESTURE_TYPE_HISTOGRAM, StylusGestureHandler.UmaGestureType.SELECT_RANGE);
        Class builderClass =
                getBuilderForClass(Class.forName(TARGET_PACKAGE + "SelectRangeGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity").invoke(builder, StylusGestureHandler.GRANULARITY_WORD);
        builderMethods.get("setSelectionStartArea").invoke(builder, new RectF(10, 10, 45, 55));
        builderMethods.get("setSelectionEndArea").invoke(builder, new RectF(0, 100, 70, 200));
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
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.SELECT_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.WORD,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(10, 10, 35, 45), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(0, 100, 70, 100), mLastGestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testDeleteRangeGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                GESTURE_TYPE_HISTOGRAM, StylusGestureHandler.UmaGestureType.DELETE_RANGE);
        Class builderClass =
                getBuilderForClass(Class.forName(TARGET_PACKAGE + "DeleteRangeGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity")
                .invoke(builder, StylusGestureHandler.GRANULARITY_CHARACTER);
        builderMethods.get("setDeletionStartArea").invoke(builder, new RectF(10, 10, 45, 55));
        builderMethods.get("setDeletionEndArea").invoke(builder, new RectF(0, 100, 70, 200));
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
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.DELETE_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(10, 10, 35, 45), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(0, 100, 70, 100), mLastGestureData.endRect);
        assertEquals(FALLBACK_TEXT, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
        histogram.assertExpected();
    }

    /**
     * Sets up an end to end test.
     * @return the bounds of the input_text element.
     */
    private RectF setUpEndToEndTest() throws TimeoutException {
        EditorInfo editorInfo = new EditorInfo();
        ImeAdapterImpl imeAdapter = spy(mRule.getImeAdapter());
        doReturn(mRule.getConnection())
                .when(imeAdapter)
                .onCreateInputConnection(any(EditorInfo.class), any(boolean.class));
        doCallRealMethod().when(imeAdapter).onCreateInputConnection(any(EditorInfo.class));
        mWrappedInputConnection = imeAdapter.onCreateInputConnection(editorInfo);
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(),
                "document.getElementById(\"input_text\").value = \"hello world\"");

        // Gets the bounding box of the element with id: input_text.
        float left = Float.parseFloat(
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(),
                        "document.getElementById('input_text').getBoundingClientRect().left"));
        float top = Float.parseFloat(
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(),
                        "document.getElementById('input_text').getBoundingClientRect().top"));
        float right = Float.parseFloat(
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(),
                        "document.getElementById('input_text').getBoundingClientRect().right"));
        float bottom = Float.parseFloat(
                JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(),
                        "document.getElementById('input_text').getBoundingClientRect().bottom"));

        RectF gestureRect =
                toScreenRectF(left, top, right, bottom, (WebContentsImpl) mRule.getWebContents());
        gestureRect.inset(1, 1); // Inset to avoid including the border.
        return gestureRect;
    }

    @Test
    @LargeTest
    public void testDeleteGestureEndToEnd()
            throws TimeoutException, ClassNotFoundException, IllegalAccessException,
                   InstantiationException, InvocationTargetException {
        RectF gestureRect = setUpEndToEndTest();

        // The following reflection creates a new DeleteGesture over the area of the input_text
        // element. It has word granularity and the fallback text defined in FALLBACK_TEXT.
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

        // Get the text inside input_text and assert that it has been deleted.
        assertEquals("\"\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mRule.getWebContents(), "document.getElementById(\"input_text\").value"));
    }

    @Test
    @LargeTest
    public void testSelectGestureEndToEnd()
            throws TimeoutException, ClassNotFoundException, IllegalAccessException,
                   InstantiationException, InvocationTargetException {
        RectF gestureRect = setUpEndToEndTest();
        // The following reflection creates a new SelectGesture over the area of the input_text
        // element. It has word granularity and the fallback text defined in FALLBACK_TEXT.
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

        // Get the text inside input_text and assert that it is the same.
        assertEquals("\"hello world\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mRule.getWebContents(), "document.getElementById(\"input_text\").value;"));
        // Get the currently selected text and assert that it is the contents of input_text.
        assertEquals("\"hello world\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mRule.getWebContents(), "window.getSelection().toString();"));
    }

    private static Map<String, Method> getMethodsForClass(Class<?> className) {
        Map<String, Method> methodMap = new HashMap<>();
        for (Method method : className.getDeclaredMethods()) {
            methodMap.put(method.getName(), method);
        }
        return methodMap;
    }

    private static Class<?> getBuilderForClass(Class<?> className) throws ClassNotFoundException {
        Class builderClass = null;
        for (Class innerClass : className.getDeclaredClasses()) {
            if (innerClass.getName().endsWith("Builder")) {
                builderClass = innerClass;
            }
        }
        if (builderClass == null) {
            throw new ClassNotFoundException(
                    String.format("Could not find inner builder class for %s", className));
        }
        return builderClass;
    }

    private static Rect createMojoRect(int x, int y, int width, int height) {
        Rect rect = new Rect();
        rect.x = x;
        rect.y = y;
        rect.width = width;
        rect.height = height;
        return rect;
    }

    private static void assertMojoRectsAreEqual(Rect expected, Rect actual) {
        assertEquals(expected.x, actual.x);
        assertEquals(expected.y, actual.y);
        assertEquals(expected.width, actual.width);
        assertEquals(expected.height, actual.height);
    }

    private static String toJavaString(org.chromium.mojo_base.mojom.String16 buffer) {
        StringBuilder string = new StringBuilder();
        for (short c : buffer.data) {
            string.append((char) c);
        }
        return string.toString();
    }

    /**
     * @param left the X value for the left edge of the rectangle.
     * @param top the Y value for the top edge of the rectangle.
     * @param right the X value for the right edge of the rectangle.
     * @param bottom the Y value for the bottom edge of the rectangle.
     * @param webContents the web contents where this rectangle is being used (for converting to
     *         screen coordinates).
     * @return an Android RectF object represented by the four float values in screen coordinates.
     */
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
