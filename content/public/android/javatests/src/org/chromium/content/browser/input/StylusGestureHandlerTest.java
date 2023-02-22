// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.graphics.PointF;
import android.graphics.RectF;
import android.view.inputmethod.InputConnection;

import androidx.core.os.BuildCompat;
import androidx.test.filters.MediumTest;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.blink.mojom.StylusWritingGestureAction;
import org.chromium.blink.mojom.StylusWritingGestureData;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.gfx.mojom.Rect;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;

/**
 * Tests for using a StylusGestureHandler as an InvocationHandler in place of an InputConnection in
 * ImeAdapterImpl#onCreateInputConnection.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({"expose-internals-for-testing", "enable-features=StylusRichGestures"})
public class StylusGestureHandlerTest {
    @Rule
    public ImeActivityTestRule mRule = new ImeActivityTestRule();

    private static String sTargetPackage = "android.view.inputmethod.";
    private static String sFallbackText = "this gesture failed";

    private InputConnection mWrappedInputConnection;
    private StylusWritingGestureData mLastGestureData;

    @Before
    public void setUp() throws Exception {
        Assume.assumeTrue("Skipping U+ test on older OS version", BuildCompat.isAtLeastU());
        mRule.setUpForUrl(ImeActivityTestRule.INPUT_FORM_HTML);
        mWrappedInputConnection = StylusGestureHandler.maybeProxyInputConnection(
                mRule.getInputConnection(), (gestureData) -> mLastGestureData = gestureData);
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
        Class builderClass = getBuilderForClass(Class.forName(sTargetPackage + "SelectGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity")
                .invoke(builder, StylusGestureHandler.GRANULARITY_CHARACTER);
        builderMethods.get("setSelectionArea").invoke(builder, new RectF(0, 0, 10, 10));
        builderMethods.get("setFallbackText").invoke(builder, sFallbackText);
        Object gesture = builderMethods.get("build").invoke(builder);

        Method performHandwritingGesture = getMethodsForClass(mWrappedInputConnection.getClass())
                                                   .get("performHandwritingGesture");
        performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.SELECT_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(0, 5, 0, 0), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(10, 5, 0, 0), mLastGestureData.endRect);
        assertEquals(sFallbackText, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
    }

    @Test
    @MediumTest
    public void testInsertGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        Class builderClass = getBuilderForClass(Class.forName(sTargetPackage + "InsertGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setTextToInsert").invoke(builder, "Foo");
        builderMethods.get("setInsertionPoint").invoke(builder, new PointF(15, 31));
        builderMethods.get("setFallbackText").invoke(builder, sFallbackText);
        Object gesture = builderMethods.get("build").invoke(builder);

        Method performHandwritingGesture = getMethodsForClass(mWrappedInputConnection.getClass())
                                                   .get("performHandwritingGesture");
        performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.ADD_SPACE_OR_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(15, 31, 0, 0), mLastGestureData.startRect);
        assertNull(mLastGestureData.endRect);
        assertEquals(sFallbackText, toJavaString(mLastGestureData.textAlternative));
        assertEquals("Foo", toJavaString(mLastGestureData.textToInsert));
    }

    @Test
    @MediumTest
    public void testDeleteGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        Class builderClass = getBuilderForClass(Class.forName(sTargetPackage + "DeleteGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity").invoke(builder, StylusGestureHandler.GRANULARITY_WORD);
        builderMethods.get("setDeletionArea").invoke(builder, new RectF(0, 0, 10, 10));
        builderMethods.get("setFallbackText").invoke(builder, sFallbackText);
        Object gesture = builderMethods.get("build").invoke(builder);

        Method performHandwritingGesture = getMethodsForClass(mWrappedInputConnection.getClass())
                                                   .get("performHandwritingGesture");
        performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.DELETE_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.WORD,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(0, 5, 0, 0), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(10, 5, 0, 0), mLastGestureData.endRect);
        assertEquals(sFallbackText, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
    }

    @Test
    @MediumTest
    public void testRemoveSpaceGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        Class builderClass =
                getBuilderForClass(Class.forName(sTargetPackage + "RemoveSpaceGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setPoints").invoke(builder, new PointF(51, 25), new PointF(105, 30));
        builderMethods.get("setFallbackText").invoke(builder, sFallbackText);
        Object gesture = builderMethods.get("build").invoke(builder);

        Method performHandwritingGesture = getMethodsForClass(mWrappedInputConnection.getClass())
                                                   .get("performHandwritingGesture");
        performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.REMOVE_SPACES, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(51, 25, 0, 0), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(105, 30, 0, 0), mLastGestureData.endRect);
        assertEquals(sFallbackText, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
    }

    @Test
    @MediumTest
    public void testJoinOrSplitGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        Class builderClass =
                getBuilderForClass(Class.forName(sTargetPackage + "JoinOrSplitGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setJoinOrSplitPoint").invoke(builder, new PointF(1, 19));
        builderMethods.get("setFallbackText").invoke(builder, sFallbackText);
        Object gesture = builderMethods.get("build").invoke(builder);

        Method performHandwritingGesture = getMethodsForClass(mWrappedInputConnection.getClass())
                                                   .get("performHandwritingGesture");
        performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.SPLIT_OR_MERGE, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(1, 19, 0, 0), mLastGestureData.startRect);
        assertNull(mLastGestureData.endRect);
        assertEquals(sFallbackText, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
    }

    @Test
    @MediumTest
    public void testSelectRangeGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        Class builderClass =
                getBuilderForClass(Class.forName(sTargetPackage + "SelectRangeGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity").invoke(builder, StylusGestureHandler.GRANULARITY_WORD);
        builderMethods.get("setSelectionStartArea").invoke(builder, new RectF(10, 10, 45, 55));
        builderMethods.get("setSelectionEndArea").invoke(builder, new RectF(0, 100, 70, 200));
        builderMethods.get("setFallbackText").invoke(builder, sFallbackText);
        Object gesture = builderMethods.get("build").invoke(builder);

        Method performHandwritingGesture = getMethodsForClass(mWrappedInputConnection.getClass())
                                                   .get("performHandwritingGesture");
        performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.SELECT_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.WORD,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(10, 10, 35, 45), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(0, 100, 70, 100), mLastGestureData.endRect);
        assertEquals(sFallbackText, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
    }

    @Test
    @MediumTest
    public void testDeleteRangeGesture()
            throws ClassNotFoundException, InvocationTargetException, IllegalAccessException,
                   InstantiationException, NoSuchMethodException {
        Class builderClass =
                getBuilderForClass(Class.forName(sTargetPackage + "DeleteRangeGesture"));
        Map<String, Method> builderMethods = getMethodsForClass(builderClass);
        Object builder = builderClass.newInstance();
        builderMethods.get("setGranularity")
                .invoke(builder, StylusGestureHandler.GRANULARITY_CHARACTER);
        builderMethods.get("setDeletionStartArea").invoke(builder, new RectF(10, 10, 45, 55));
        builderMethods.get("setDeletionEndArea").invoke(builder, new RectF(0, 100, 70, 200));
        builderMethods.get("setFallbackText").invoke(builder, sFallbackText);
        Object gesture = builderMethods.get("build").invoke(builder);

        Method performHandwritingGesture = getMethodsForClass(mWrappedInputConnection.getClass())
                                                   .get("performHandwritingGesture");
        performHandwritingGesture.invoke(mWrappedInputConnection, gesture, null, null);
        CriteriaHelper.pollUiThread(
                () -> mLastGestureData != null, "Gesture creation was unsuccessful");

        assertEquals(StylusWritingGestureAction.DELETE_TEXT, mLastGestureData.action);
        assertEquals(org.chromium.blink.mojom.StylusWritingGestureGranularity.CHARACTER,
                mLastGestureData.granularity);
        assertMojoRectsAreEqual(createMojoRect(10, 10, 35, 45), mLastGestureData.startRect);
        assertMojoRectsAreEqual(createMojoRect(0, 100, 70, 100), mLastGestureData.endRect);
        assertEquals(sFallbackText, toJavaString(mLastGestureData.textAlternative));
        assertNull(mLastGestureData.textToInsert);
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
}
