// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.graphics.PointF;
import android.graphics.RectF;
import android.view.inputmethod.InputConnection;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.core.os.BuildCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink.mojom.StylusWritingGestureAction;
import org.chromium.blink.mojom.StylusWritingGestureData;
import org.chromium.blink.mojom.StylusWritingGestureGranularity;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.gfx.mojom.Rect;
import org.chromium.mojo_base.mojom.String16;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.concurrent.Executor;
import java.util.function.IntConsumer;

/**
 * InvocationHandler which uses reflection to access Android U APIs for stylus rich gestures.
 * Adds a listener for InputConnection#performHandwritingGesture and constructs a Mojo GestureData
 * object to pass to its callback.
 * TODO(crbug.com/1416170): Remove this class once Android U is released.
 */
public class StylusGestureHandler implements InvocationHandler {
    // Mirror of the constants in Android's HandwritingGesture and InputConnection classes.
    // HandwritingGesture#GRANULARITY_WORD
    static final int GRANULARITY_WORD = 1;
    // HandwritingGesture#GRANULARITY_CHARACTER
    static final int GRANULARITY_CHARACTER = 2;

    private static final String TAG = "StylusGestureHandler";

    // Should be kept in sync with StylusHandwritingGesture in tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // Entries with the DW prefix are used by Samsung's DirectWriting service. All other entries are
    // used by Android stylus handwriting.
    @IntDef({UmaGestureType.DW_DELETE_TEXT, UmaGestureType.DW_ADD_SPACE_OR_TEXT,
            UmaGestureType.DW_REMOVE_SPACES, UmaGestureType.DW_SPLIT_OR_MERGE,
            UmaGestureType.SELECT, UmaGestureType.INSERT, UmaGestureType.DELETE,
            UmaGestureType.REMOVE_SPACE, UmaGestureType.JOIN_OR_SPLIT, UmaGestureType.SELECT_RANGE,
            UmaGestureType.DELETE_RANGE, UmaGestureType.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UmaGestureType {
        int DW_DELETE_TEXT = 0;
        int DW_ADD_SPACE_OR_TEXT = 1;
        int DW_REMOVE_SPACES = 2;
        int DW_SPLIT_OR_MERGE = 3;
        int SELECT = 4;
        int INSERT = 5;
        int DELETE = 6;
        int REMOVE_SPACE = 7;
        int JOIN_OR_SPLIT = 8;
        int SELECT_RANGE = 9;
        int DELETE_RANGE = 10;
        int NUM_ENTRIES = 11;
    }

    private final InputConnection mFallback;
    private final Callback<OngoingGesture> mOnGestureCallback;

    /**
     * If StylusRichGestures are enabled, return a proxy class so that we can handle calls to
     * InputConnection#performHandwritingGesture which doesn't exist at compile time but will at
     * runtime on Android U devices.
     * @param inputConnection The delegate InputConnection to which other method calls should be
     *         routed.
     * @param onGestureCallback A callback to handle performing the gesture.
     * @return Either an InputConnection or a proxy of an InputConnection.
     */
    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    public static @Nullable InputConnection maybeProxyInputConnection(
            @Nullable InputConnection inputConnection, Callback<OngoingGesture> onGestureCallback) {
        if (inputConnection == null || !BuildCompat.isAtLeastU()
                || !ContentFeatureMap.isEnabled(
                        org.chromium.blink_public.common.BlinkFeatures.STYLUS_RICH_GESTURES)) {
            return inputConnection;
        }

        InputConnection proxy = (InputConnection) Proxy.newProxyInstance(
                InputConnection.class.getClassLoader(), new Class<?>[] {InputConnection.class},
                new StylusGestureHandler(inputConnection, onGestureCallback));

        return proxy;
    }

    public static void logGestureType(@UmaGestureType int gestureType) {
        RecordHistogram.recordEnumeratedHistogram(
                "InputMethod.StylusHandwriting.Gesture", gestureType, UmaGestureType.NUM_ENTRIES);
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args)
            throws IllegalAccessException, IllegalArgumentException, InvocationTargetException,
                   ClassNotFoundException {
        if (!method.getName().equals("performHandwritingGesture")) {
            return method.invoke(mFallback, args);
        }
        StylusWritingGestureData gestureData = createGesture(args[0]);
        Executor executor = (Executor) args[1];
        IntConsumer consumer = (IntConsumer) args[2];
        // Callback should be run on the UI thread.
        PostTask.postTask(TaskTraits.UI_USER_BLOCKING, () -> {
            OngoingGesture gesture = new OngoingGesture(gestureData, executor, consumer);
            mOnGestureCallback.onResult(gesture);
        });
        return null;
    }

    private StylusGestureHandler(
            InputConnection fallback, Callback<OngoingGesture> onGestureCallback) {
        mFallback = fallback;
        mOnGestureCallback = onGestureCallback;
    }

    private StylusWritingGestureData createGesture(Object gesture) throws ClassNotFoundException {
        String packageName = "android.view.inputmethod.";
        StylusWritingGestureData gestureData = null;

        if (Class.forName(packageName + "SelectGesture").isInstance(gesture)) {
            gestureData = createSelectGesture(gesture);
            logGestureType(UmaGestureType.SELECT);
        } else if (Class.forName(packageName + "InsertGesture").isInstance(gesture)) {
            gestureData = createInsertGesture(gesture);
            logGestureType(UmaGestureType.INSERT);
        } else if (Class.forName(packageName + "DeleteGesture").isInstance(gesture)) {
            gestureData = createDeleteGesture(gesture);
            logGestureType(UmaGestureType.DELETE);
        } else if (Class.forName(packageName + "RemoveSpaceGesture").isInstance(gesture)) {
            gestureData = createRemoveSpaceGesture(gesture);
            logGestureType(UmaGestureType.REMOVE_SPACE);
        } else if (Class.forName(packageName + "JoinOrSplitGesture").isInstance(gesture)) {
            gestureData = createJoinOrSplitGesture(gesture);
            logGestureType(UmaGestureType.JOIN_OR_SPLIT);
        } else if (Class.forName(packageName + "SelectRangeGesture").isInstance(gesture)) {
            gestureData = createSelectRangeGesture(gesture);
            logGestureType(UmaGestureType.SELECT_RANGE);
        } else if (Class.forName(packageName + "DeleteRangeGesture").isInstance(gesture)) {
            gestureData = createDeleteRangeGesture(gesture);
            logGestureType(UmaGestureType.DELETE_RANGE);
        }
        return gestureData;
    }

    /**
     * Creates a StylusWritingGestureData object from an abstract SelectGesture object.
     * @param gesture The abstract SelectGesture object to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private StylusWritingGestureData createSelectGesture(Object gesture) {
        try {
            Class clazz = Class.forName("android.view.inputmethod.SelectGesture");
            StylusWritingGestureData gestureData = new StylusWritingGestureData();
            gestureData.action = StylusWritingGestureAction.SELECT_TEXT;

            // int SelectGesture#getGranularity()
            Method getGranularity = clazz.getMethod("getGranularity");
            gestureData.granularity = ((int) getGranularity.invoke(gesture)) == GRANULARITY_WORD
                    ? StylusWritingGestureGranularity.WORD
                    : StylusWritingGestureGranularity.CHARACTER;

            // String SelectGesture#getFallbackText()
            Method getFallback = clazz.getMethod("getFallbackText");
            gestureData.textAlternative = toMojoString((String) getFallback.invoke(gesture));

            // RectF SelectGesture#getSelectionArea()
            Method getArea = clazz.getMethod("getSelectionArea");
            Rect[] areas = toTwoMojoRects((RectF) getArea.invoke(gesture));
            gestureData.startRect = areas[0];
            gestureData.endRect = areas[1];
            return gestureData;
        } catch (Throwable e) {
            Log.e(TAG, "Could not unpack gesture object into SelectGesture");
        }
        return null;
    }

    /**
     * Creates a StylusWritingGestureData object from an abstract InsertGesture object.
     * @param gesture The abstract InsertGesture object to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private StylusWritingGestureData createInsertGesture(Object gesture) {
        try {
            Class clazz = Class.forName("android.view.inputmethod.InsertGesture");
            StylusWritingGestureData gestureData = new StylusWritingGestureData();
            gestureData.action = StylusWritingGestureAction.ADD_SPACE_OR_TEXT;
            gestureData.granularity = StylusWritingGestureGranularity.CHARACTER;

            // String InsertGesture#getFallbackText()
            Method getFallback = clazz.getMethod("getFallbackText");
            gestureData.textAlternative = toMojoString((String) getFallback.invoke(gesture));

            // String InsertGesture#getTextToinsert()
            Method getTextToInsert = clazz.getMethod("getTextToInsert");
            gestureData.textToInsert = toMojoString((String) getTextToInsert.invoke(gesture));

            // PointF InsertGesture#getInsertionPoint()
            Method getInsertionPoint = clazz.getMethod("getInsertionPoint");
            gestureData.startRect = toMojoRect((PointF) getInsertionPoint.invoke(gesture));
            return gestureData;
        } catch (Throwable e) {
            Log.e(TAG, "Could not unpack gesture object into InsertGesture");
        }
        return null;
    }

    /**
     * Creates a StylusWritingGestureData object from an abstract DeleteGesture object.
     * @param gesture The abstract DeleteGesture object to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private StylusWritingGestureData createDeleteGesture(Object gesture) {
        try {
            Class clazz = Class.forName("android.view.inputmethod.DeleteGesture");
            StylusWritingGestureData gestureData = new StylusWritingGestureData();
            gestureData.action = StylusWritingGestureAction.DELETE_TEXT;

            // int DeleteGesture#getGranularity()
            Method getGranularity = clazz.getMethod("getGranularity");
            gestureData.granularity = ((int) getGranularity.invoke(gesture)) == GRANULARITY_WORD
                    ? StylusWritingGestureGranularity.WORD
                    : StylusWritingGestureGranularity.CHARACTER;

            // String DeleteGesture#getFallbackText()
            Method getFallback = clazz.getMethod("getFallbackText");
            gestureData.textAlternative = toMojoString((String) getFallback.invoke(gesture));

            // RectF DeleteGesture#getDeletionArea()
            Method getArea = clazz.getMethod("getDeletionArea");
            Rect[] areas = toTwoMojoRects((RectF) getArea.invoke(gesture));
            gestureData.startRect = areas[0];
            gestureData.endRect = areas[1];
            return gestureData;
        } catch (Throwable e) {
            Log.e(TAG, "Could not unpack gesture object into DeleteGesture");
        }
        return null;
    }

    /**
     * Creates a StylusWritingGestureData object from an abstract RemoveSpaceGesture object.
     * @param gesture The abstract RemoveSpaceGesture object to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private StylusWritingGestureData createRemoveSpaceGesture(Object gesture) {
        try {
            Class clazz = Class.forName("android.view.inputmethod.RemoveSpaceGesture");
            StylusWritingGestureData gestureData = new StylusWritingGestureData();
            gestureData.action = StylusWritingGestureAction.REMOVE_SPACES;
            gestureData.granularity = StylusWritingGestureGranularity.CHARACTER;

            // String RemoveSpaceGesture#getFallbackText()
            Method getFallback = clazz.getMethod("getFallbackText");
            gestureData.textAlternative = toMojoString((String) getFallback.invoke(gesture));

            // PointF RemoveSpaceGesture#getStartPoint()
            Method getStartPoint = clazz.getMethod("getStartPoint");
            gestureData.startRect = toMojoRect((PointF) getStartPoint.invoke(gesture));

            // PointF RemoveSpaceGesture#getEndPoint()
            Method getEndPoint = clazz.getMethod("getEndPoint");
            gestureData.endRect = toMojoRect((PointF) getEndPoint.invoke(gesture));
            return gestureData;
        } catch (Throwable e) {
            Log.e(TAG, "Could not unpack gesture object into RemoveSpaceGesture");
        }
        return null;
    }

    /**
     * Creates a StylusWritingGestureData object from an abstract JoinOrSplitGesture object.
     * @param gesture The abstract JoinOrSplitGesture object to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private StylusWritingGestureData createJoinOrSplitGesture(Object gesture) {
        try {
            Class clazz = Class.forName("android.view.inputmethod.JoinOrSplitGesture");
            StylusWritingGestureData gestureData = new StylusWritingGestureData();
            gestureData.action = StylusWritingGestureAction.SPLIT_OR_MERGE;
            gestureData.granularity = StylusWritingGestureGranularity.CHARACTER;

            // String JoinOrSplitGesture#getFallbackText()
            Method getFallback = clazz.getMethod("getFallbackText");
            gestureData.textAlternative = toMojoString((String) getFallback.invoke(gesture));

            // PointF JoinOrSplitGesture#getJoinOrSplitPoint()
            Method getJoinOrSplitPoint = clazz.getMethod("getJoinOrSplitPoint");
            gestureData.startRect = toMojoRect((PointF) getJoinOrSplitPoint.invoke(gesture));
            return gestureData;
        } catch (Throwable e) {
            Log.e(TAG, "Could not unpack gesture object into JoinOrSplitGesture");
        }
        return null;
    }

    /**
     * Creates a StylusWritingGestureData object from an abstract SelectRangeGesture object.
     * @param gesture The abstract SelectRangeGesture object to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private StylusWritingGestureData createSelectRangeGesture(Object gesture) {
        try {
            Class clazz = Class.forName("android.view.inputmethod.SelectRangeGesture");
            StylusWritingGestureData gestureData = new StylusWritingGestureData();
            gestureData.action = StylusWritingGestureAction.SELECT_TEXT;

            // int SelectRangeGesture#getGranularity()
            Method getGranularity = clazz.getMethod("getGranularity");
            gestureData.granularity = ((int) getGranularity.invoke(gesture)) == GRANULARITY_WORD
                    ? StylusWritingGestureGranularity.WORD
                    : StylusWritingGestureGranularity.CHARACTER;

            // String SelectRangeGesture#getFallbackText()
            Method getFallback = clazz.getMethod("getFallbackText");
            gestureData.textAlternative = toMojoString((String) getFallback.invoke(gesture));

            // RectF SelectRangeGesture#getSelectionStartArea()
            Method getStartArea = clazz.getMethod("getSelectionStartArea");
            gestureData.startRect = toMojoRect((RectF) getStartArea.invoke(gesture));

            // RectF SelectRangeGesture#getSelectionEndArea()
            Method getEndArea = clazz.getMethod("getSelectionEndArea");
            gestureData.endRect = toMojoRect((RectF) getEndArea.invoke(gesture));
            return gestureData;
        } catch (Throwable e) {
            Log.e(TAG, "Could not unpack gesture object into SelectRangeGesture");
        }
        return null;
    }

    /**
     * Creates a StylusWritingGestureData object from an abstract DeleteRangeGesture object.
     * @param gesture The abstract DeleteRangeGesture object to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private StylusWritingGestureData createDeleteRangeGesture(Object gesture) {
        try {
            Class clazz = Class.forName("android.view.inputmethod.DeleteRangeGesture");
            StylusWritingGestureData gestureData = new StylusWritingGestureData();
            gestureData.action = StylusWritingGestureAction.DELETE_TEXT;

            // int DeleteRangeGesture#getGranularity()
            Method getGranularity = clazz.getMethod("getGranularity");
            gestureData.granularity = ((int) getGranularity.invoke(gesture)) == GRANULARITY_WORD
                    ? StylusWritingGestureGranularity.WORD
                    : StylusWritingGestureGranularity.CHARACTER;

            // String DeleteRangeGesture#getFallbackText()
            Method getFallback = clazz.getMethod("getFallbackText");
            gestureData.textAlternative = toMojoString((String) getFallback.invoke(gesture));

            // RectF DeleteRangeGesture#getDeletionStartArea()
            Method getStartArea = clazz.getMethod("getDeletionStartArea");
            gestureData.startRect = toMojoRect((RectF) getStartArea.invoke(gesture));

            // RectF DeleteRangeGesture#getDeletionEndArea()
            Method getEndArea = clazz.getMethod("getDeletionEndArea");
            gestureData.endRect = toMojoRect((RectF) getEndArea.invoke(gesture));
            return gestureData;
        } catch (Throwable e) {
            Log.e(TAG, "Could not unpack gesture object into DeleteRangeGesture");
        }
        return null;
    }

    /**
     * Takes an Android RectF and converts it to a Mojo Rect object.
     * @param rect The Android representation of a rectangle with four floats representing the left,
     * top, right and bottom positions of the rectangle.
     * @return A Mojo rectangle which consists of a point (represented by x and y integers) and a
     * size (represented by width and height integers).
     */
    private static Rect toMojoRect(RectF rect) {
        Rect mojoRect = new Rect();
        mojoRect.x = Math.round(rect.left);
        mojoRect.y = Math.round(rect.top);
        mojoRect.width = Math.round(rect.right - rect.left);
        mojoRect.height = Math.round(rect.bottom - rect.top);
        return mojoRect;
    }

    /**
     * Takes an Android PointF and converts it to a zero-sized Mojo Rect object.
     * @param point The Android representation of a point with two floats for the x and y position.
     * @return A Mojo rectangle with an area of 0 at the provided point.
     */
    private static Rect toMojoRect(PointF point) {
        Rect rect = new Rect();
        rect.x = Math.round(point.x);
        rect.y = Math.round(point.y);
        rect.width = 0;
        rect.height = 0;
        return rect;
    }

    /**
     * Converts an Android RectF object to an array of 2 Mojo Rect objects. These Rect objects have
     * an area of 0 and represent the left center and right center of the given RectF.
     * @param area The Android RectF to convert to two Mojo Rect objects.
     * @return An area of 2 Mojo Rect objects representing the left and right centers of the RectF.
     */
    private static Rect[] toTwoMojoRects(RectF area) {
        PointF left = new PointF(area.left, (area.top + area.bottom) / 2.0f);
        PointF right = new PointF(area.right, (area.top + area.bottom) / 2.0f);
        Rect areas[] = new Rect[2];
        areas[0] = toMojoRect(left);
        areas[1] = toMojoRect(right);
        return areas;
    }

    /**
     * Converts a Java String object to the String16 representation compatible with Mojo.
     * @param string A Java String to convert to the String16 format.
     * @return A String16 object which wraps an array of short integers for each character in the
     * string.
     */
    private static String16 toMojoString(String string) {
        short[] data = new short[string.length()];
        for (int i = 0; i < data.length; i++) {
            data[i] = (short) string.charAt(i);
        }
        String16 mojoString = new String16();
        mojoString.data = data;
        return mojoString;
    }
}
