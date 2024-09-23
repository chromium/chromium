// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.graphics.PointF;
import android.graphics.RectF;
import android.os.Build;
import android.view.inputmethod.DeleteGesture;
import android.view.inputmethod.DeleteRangeGesture;
import android.view.inputmethod.HandwritingGesture;
import android.view.inputmethod.InsertGesture;
import android.view.inputmethod.JoinOrSplitGesture;
import android.view.inputmethod.RemoveSpaceGesture;
import android.view.inputmethod.SelectGesture;
import android.view.inputmethod.SelectRangeGesture;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.StylusWritingGestureAction;
import org.chromium.blink.mojom.StylusWritingGestureData;
import org.chromium.blink.mojom.StylusWritingGestureGranularity;
import org.chromium.gfx.mojom.Rect;
import org.chromium.mojo_base.mojom.String16;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Converts stylus rich gestures from their Android representation to their Blink representation.
 */
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
public class StylusGestureConverter {
    // Should be kept in sync with StylusHandwritingGesture in tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // Entries with the DW prefix are used by Samsung's DirectWriting service. All other entries are
    // used by Android stylus handwriting.
    @IntDef({
        UmaGestureType.DW_DELETE_TEXT,
        UmaGestureType.DW_ADD_SPACE_OR_TEXT,
        UmaGestureType.DW_REMOVE_SPACES,
        UmaGestureType.DW_SPLIT_OR_MERGE,
        UmaGestureType.SELECT,
        UmaGestureType.INSERT,
        UmaGestureType.DELETE,
        UmaGestureType.REMOVE_SPACE,
        UmaGestureType.JOIN_OR_SPLIT,
        UmaGestureType.SELECT_RANGE,
        UmaGestureType.DELETE_RANGE,
        UmaGestureType.NUM_ENTRIES
    })
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

    public static void logGestureType(@UmaGestureType int gestureType) {
        RecordHistogram.recordEnumeratedHistogram(
                "InputMethod.StylusHandwriting.Gesture", gestureType, UmaGestureType.NUM_ENTRIES);
    }

    public static StylusWritingGestureData createGestureData(HandwritingGesture gesture) {
        if (gesture instanceof SelectGesture) {
            logGestureType(UmaGestureType.SELECT);
            return createGestureData((SelectGesture) gesture);
        } else if (gesture instanceof InsertGesture) {
            logGestureType(UmaGestureType.INSERT);
            return createGestureData((InsertGesture) gesture);
        } else if (gesture instanceof DeleteGesture) {
            logGestureType(UmaGestureType.DELETE);
            return createGestureData((DeleteGesture) gesture);
        } else if (gesture instanceof RemoveSpaceGesture) {
            logGestureType(UmaGestureType.REMOVE_SPACE);
            return createGestureData((RemoveSpaceGesture) gesture);
        } else if (gesture instanceof JoinOrSplitGesture) {
            logGestureType(UmaGestureType.JOIN_OR_SPLIT);
            return createGestureData((JoinOrSplitGesture) gesture);
        } else if (gesture instanceof SelectRangeGesture) {
            logGestureType(UmaGestureType.SELECT_RANGE);
            return createGestureData((SelectRangeGesture) gesture);
        } else if (gesture instanceof DeleteRangeGesture) {
            logGestureType(UmaGestureType.DELETE_RANGE);
            return createGestureData((DeleteRangeGesture) gesture);
        }
        return null;
    }

    /**
     * Creates a StylusWritingGestureData object from a SelectGesture.
     * @param gesture The SelectGesture to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private static StylusWritingGestureData createGestureData(SelectGesture gesture) {
        StylusWritingGestureData gestureData = new StylusWritingGestureData();
        gestureData.action = StylusWritingGestureAction.SELECT_TEXT;
        gestureData.granularity =
                gesture.getGranularity() == HandwritingGesture.GRANULARITY_WORD
                        ? StylusWritingGestureGranularity.WORD
                        : StylusWritingGestureGranularity.CHARACTER;
        gestureData.textAlternative = toMojoString(gesture.getFallbackText());
        Rect[] areas = toTwoMojoRects(gesture.getSelectionArea());
        gestureData.startRect = areas[0];
        gestureData.endRect = areas[1];
        return gestureData;
    }

    /**
     * Creates a StylusWritingGestureData object from an InsertGesture.
     * @param gesture The InsertGesture to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private static StylusWritingGestureData createGestureData(InsertGesture gesture) {
        StylusWritingGestureData gestureData = new StylusWritingGestureData();
        gestureData.action = StylusWritingGestureAction.ADD_SPACE_OR_TEXT;
        gestureData.granularity = StylusWritingGestureGranularity.CHARACTER;
        gestureData.textAlternative = toMojoString(gesture.getFallbackText());
        gestureData.textToInsert = toMojoString(gesture.getTextToInsert());
        gestureData.startRect = toMojoRect(gesture.getInsertionPoint());
        return gestureData;
    }

    /**
     * Creates a StylusWritingGestureData object from a DeleteGesture.
     * @param gesture The DeleteGesture to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private static StylusWritingGestureData createGestureData(DeleteGesture gesture) {
        StylusWritingGestureData gestureData = new StylusWritingGestureData();
        gestureData.action = StylusWritingGestureAction.DELETE_TEXT;
        gestureData.granularity =
                gesture.getGranularity() == HandwritingGesture.GRANULARITY_WORD
                        ? StylusWritingGestureGranularity.WORD
                        : StylusWritingGestureGranularity.CHARACTER;
        gestureData.textAlternative = toMojoString(gesture.getFallbackText());
        Rect[] areas = toTwoMojoRects(gesture.getDeletionArea());
        gestureData.startRect = areas[0];
        gestureData.endRect = areas[1];
        return gestureData;
    }

    /**
     * Creates a StylusWritingGestureData object from a RemoveSpaceGesture.
     * @param gesture The RemoveSpaceGesture object to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private static StylusWritingGestureData createGestureData(RemoveSpaceGesture gesture) {
        StylusWritingGestureData gestureData = new StylusWritingGestureData();
        gestureData.action = StylusWritingGestureAction.REMOVE_SPACES;
        gestureData.granularity = StylusWritingGestureGranularity.CHARACTER;
        gestureData.textAlternative = toMojoString(gesture.getFallbackText());
        gestureData.startRect = toMojoRect(gesture.getStartPoint());
        gestureData.endRect = toMojoRect(gesture.getEndPoint());
        return gestureData;
    }

    /**
     * Creates a StylusWritingGestureData object from a JoinOrSplitGesture.
     * @param gesture The JoinOrSplitGesture to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private static StylusWritingGestureData createGestureData(JoinOrSplitGesture gesture) {
        StylusWritingGestureData gestureData = new StylusWritingGestureData();
        gestureData.action = StylusWritingGestureAction.SPLIT_OR_MERGE;
        gestureData.granularity = StylusWritingGestureGranularity.CHARACTER;
        gestureData.textAlternative = toMojoString(gesture.getFallbackText());
        gestureData.startRect = toMojoRect(gesture.getJoinOrSplitPoint());
        return gestureData;
    }

    /**
     * Creates a StylusWritingGestureData object from a SelectRangeGesture.
     * @param gesture The SelectRangeGesture to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private static StylusWritingGestureData createGestureData(SelectRangeGesture gesture) {
        StylusWritingGestureData gestureData = new StylusWritingGestureData();
        gestureData.action = StylusWritingGestureAction.SELECT_TEXT;
        gestureData.granularity =
                gesture.getGranularity() == HandwritingGesture.GRANULARITY_WORD
                        ? StylusWritingGestureGranularity.WORD
                        : StylusWritingGestureGranularity.CHARACTER;
        gestureData.textAlternative = toMojoString(gesture.getFallbackText());
        gestureData.startRect = toMojoRect(gesture.getSelectionStartArea());
        gestureData.endRect = toMojoRect(gesture.getSelectionEndArea());
        return gestureData;
    }

    /**
     * Creates a StylusWritingGestureData object from a DeleteRangeGesture.
     * @param gesture The DeleteRangeGesture to extract data from.
     * @return A StylusWritingGestureData object to pass through Mojo to blink.
     */
    private static StylusWritingGestureData createGestureData(DeleteRangeGesture gesture) {
        StylusWritingGestureData gestureData = new StylusWritingGestureData();
        gestureData.action = StylusWritingGestureAction.DELETE_TEXT;
        gestureData.granularity =
                gesture.getGranularity() == HandwritingGesture.GRANULARITY_WORD
                        ? StylusWritingGestureGranularity.WORD
                        : StylusWritingGestureGranularity.CHARACTER;
        gestureData.textAlternative = toMojoString(gesture.getFallbackText());
        gestureData.startRect = toMojoRect(gesture.getDeletionStartArea());
        gestureData.endRect = toMojoRect(gesture.getDeletionEndArea());
        return gestureData;
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
        Rect[] areas = new Rect[2];
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
        int len = string != null ? string.length() : 0;
        short[] data = new short[len];
        for (int i = 0; i < data.length; i++) {
            data[i] = (short) string.charAt(i);
        }
        String16 mojoString = new String16();
        mojoString.data = data;
        return mojoString;
    }

    private StylusGestureConverter() {}
}
