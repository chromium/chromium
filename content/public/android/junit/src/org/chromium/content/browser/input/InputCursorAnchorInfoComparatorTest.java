// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.blink.mojom.EditorBoundsInfo;
import org.chromium.blink.mojom.InputCursorAnchorInfo;
import org.chromium.blink.mojom.TextAppearanceInfo;
import org.chromium.gfx.mojom.Rect;
import org.chromium.gfx.mojom.RectF;
import org.chromium.skia.mojom.SkColor;

/**
 * Tests for the InputCursorAnchorInfoComparator class which compares two InputCursorAnchorInfo
 * objects for deep equality.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class InputCursorAnchorInfoComparatorTest {
    InputCursorAnchorInfo mCursorAnchorInfo;

    @Before
    public void setUp() {
        mCursorAnchorInfo = new InputCursorAnchorInfo();
        mCursorAnchorInfo.characterBounds =
                new Rect[] {rect(0, 0, 5, 10), rect(6, 0, 5, 10), rect(12, 0, 5, 10)};
        mCursorAnchorInfo.textAppearanceInfo = new TextAppearanceInfo();
        mCursorAnchorInfo.textAppearanceInfo.textColor = new SkColor();
        mCursorAnchorInfo.textAppearanceInfo.textColor.value = 255;
        mCursorAnchorInfo.editorBoundsInfo = new EditorBoundsInfo();
        mCursorAnchorInfo.editorBoundsInfo.editorBounds = rectF(0, 0, 20, 15);
        mCursorAnchorInfo.editorBoundsInfo.handwritingBounds = rectF(-10, -10, 40, 35);
        mCursorAnchorInfo.visibleLineBounds = new Rect[] {rect(0, 0, 17, 10), rect(0, 11, 0, 0)};
    }

    @Test
    @SmallTest
    public void sameReferencesAreEqual() {
        InputCursorAnchorInfo sameOne = mCursorAnchorInfo;
        assertTrue(InputCursorAnchorInfoComparator.equals(mCursorAnchorInfo, sameOne));
    }

    @Test
    @SmallTest
    public void copiesAreEqual() {
        assertTrue(
                InputCursorAnchorInfoComparator.equals(mCursorAnchorInfo, copy(mCursorAnchorInfo)));
    }

    @Test
    @SmallTest
    public void changeCharacterBounds_notEqual() {
        InputCursorAnchorInfo differentCharacterBounds = copy(mCursorAnchorInfo);
        // Change one of the rects' width.
        differentCharacterBounds.characterBounds[1] = rect(6, 0, 8, 10);
        assertFalse(
                InputCursorAnchorInfoComparator.equals(
                        mCursorAnchorInfo, differentCharacterBounds));

        // Extra null rect.
        differentCharacterBounds.characterBounds =
                new Rect[] {rect(0, 0, 5, 10), rect(6, 0, 5, 10), rect(12, 0, 5, 10), null};
        assertFalse(
                InputCursorAnchorInfoComparator.equals(
                        mCursorAnchorInfo, differentCharacterBounds));

        // One less rect.
        differentCharacterBounds.characterBounds =
                new Rect[] {rect(0, 0, 5, 10), rect(6, 0, 5, 10)};
        assertFalse(
                InputCursorAnchorInfoComparator.equals(
                        mCursorAnchorInfo, differentCharacterBounds));
    }

    @Test
    @SmallTest
    public void changeTextAppearanceInfo_notEqual() {
        InputCursorAnchorInfo differentTextAppearanceInfo = copy(mCursorAnchorInfo);

        // Change color value.
        differentTextAppearanceInfo.textAppearanceInfo.textColor.value = 1234;
        assertFalse(
                InputCursorAnchorInfoComparator.equals(
                        mCursorAnchorInfo, differentTextAppearanceInfo));

        // Null text color.
        differentTextAppearanceInfo.textAppearanceInfo.textColor = null;
        assertFalse(
                InputCursorAnchorInfoComparator.equals(
                        mCursorAnchorInfo, differentTextAppearanceInfo));

        // Null TextAppearanceInfo
        differentTextAppearanceInfo.textAppearanceInfo = null;
        assertFalse(
                InputCursorAnchorInfoComparator.equals(
                        mCursorAnchorInfo, differentTextAppearanceInfo));
    }

    @Test
    @SmallTest
    public void changeEditorBounds_notEqual() {
        InputCursorAnchorInfo differentEditorBounds = copy(mCursorAnchorInfo);

        // Change x value of editor bounds.
        differentEditorBounds.editorBoundsInfo.editorBounds = rectF(1, 0, 20, 15);
        assertFalse(
                InputCursorAnchorInfoComparator.equals(mCursorAnchorInfo, differentEditorBounds));

        // Null editor bounds.
        differentEditorBounds.editorBoundsInfo.editorBounds = null;
        assertFalse(
                InputCursorAnchorInfoComparator.equals(mCursorAnchorInfo, differentEditorBounds));

        // Null EditorBoundsInfo.
        differentEditorBounds.editorBoundsInfo = null;
        assertFalse(
                InputCursorAnchorInfoComparator.equals(mCursorAnchorInfo, differentEditorBounds));
    }

    @Test
    @SmallTest
    public void changeHandwritingBounds_notEqual() {
        InputCursorAnchorInfo differentHandwritingBounds = copy(mCursorAnchorInfo);

        // Change y value of handwriting bounds.
        differentHandwritingBounds.editorBoundsInfo.handwritingBounds = rectF(-10, -11, 40, 35);
        assertFalse(
                InputCursorAnchorInfoComparator.equals(
                        mCursorAnchorInfo, differentHandwritingBounds));

        // Null handwriting bounds.
        differentHandwritingBounds.editorBoundsInfo.handwritingBounds = null;
        assertFalse(
                InputCursorAnchorInfoComparator.equals(
                        mCursorAnchorInfo, differentHandwritingBounds));
    }

    @Test
    @SmallTest
    public void changeVisibleLineBounds_notEqual() {
        InputCursorAnchorInfo differentLineBounds = copy(mCursorAnchorInfo);

        // Change y value of first line bound.
        differentLineBounds.visibleLineBounds = new Rect[] {rect(0, 1, 17, 10), rect(0, 11, 0, 0)};
        assertFalse(InputCursorAnchorInfoComparator.equals(mCursorAnchorInfo, differentLineBounds));

        // Extra null line bound.
        differentLineBounds.visibleLineBounds =
                new Rect[] {rect(0, 1, 17, 10), rect(0, 11, 0, 0), null};
        assertFalse(InputCursorAnchorInfoComparator.equals(mCursorAnchorInfo, differentLineBounds));

        // Extra rect line bound.
        differentLineBounds.visibleLineBounds =
                new Rect[] {rect(0, 1, 17, 10), rect(0, 11, 0, 0), rect(1, 2, 3, 4)};
        assertFalse(InputCursorAnchorInfoComparator.equals(mCursorAnchorInfo, differentLineBounds));

        // Null visible line bounds.
        differentLineBounds.visibleLineBounds = null;
        assertFalse(InputCursorAnchorInfoComparator.equals(mCursorAnchorInfo, differentLineBounds));
    }

    private static RectF rectF(float x, float y, float width, float height) {
        RectF rect = new RectF();
        rect.x = x;
        rect.y = y;
        rect.width = width;
        rect.height = height;
        return rect;
    }

    private static Rect rect(int x, int y, int width, int height) {
        Rect rect = new Rect();
        rect.x = x;
        rect.y = y;
        rect.width = width;
        rect.height = height;
        return rect;
    }

    private static InputCursorAnchorInfo copy(InputCursorAnchorInfo cursorAnchorInfo) {
        InputCursorAnchorInfo copy = new InputCursorAnchorInfo();
        if (cursorAnchorInfo.characterBounds != null) {
            copy.characterBounds = cursorAnchorInfo.characterBounds.clone();
        }
        if (cursorAnchorInfo.textAppearanceInfo != null) {
            copy.textAppearanceInfo = new TextAppearanceInfo();
            if (cursorAnchorInfo.textAppearanceInfo.textColor != null) {
                copy.textAppearanceInfo.textColor = new SkColor();
                copy.textAppearanceInfo.textColor.value =
                        cursorAnchorInfo.textAppearanceInfo.textColor.value;
            }
        }
        if (cursorAnchorInfo.editorBoundsInfo != null) {
            copy.editorBoundsInfo = new EditorBoundsInfo();
            if (cursorAnchorInfo.editorBoundsInfo.editorBounds != null) {
                copy.editorBoundsInfo.editorBounds = new RectF();
                if (cursorAnchorInfo.editorBoundsInfo.editorBounds != null) {
                    copy.editorBoundsInfo.editorBounds = new RectF();
                    copy.editorBoundsInfo.editorBounds.x =
                            cursorAnchorInfo.editorBoundsInfo.editorBounds.x;
                    copy.editorBoundsInfo.editorBounds.y =
                            cursorAnchorInfo.editorBoundsInfo.editorBounds.y;
                    copy.editorBoundsInfo.editorBounds.width =
                            cursorAnchorInfo.editorBoundsInfo.editorBounds.width;
                    copy.editorBoundsInfo.editorBounds.height =
                            cursorAnchorInfo.editorBoundsInfo.editorBounds.height;
                }
                if (cursorAnchorInfo.editorBoundsInfo.handwritingBounds != null) {
                    copy.editorBoundsInfo.handwritingBounds = new RectF();
                    copy.editorBoundsInfo.handwritingBounds.x =
                            cursorAnchorInfo.editorBoundsInfo.handwritingBounds.x;
                    copy.editorBoundsInfo.handwritingBounds.y =
                            cursorAnchorInfo.editorBoundsInfo.handwritingBounds.y;
                    copy.editorBoundsInfo.handwritingBounds.width =
                            cursorAnchorInfo.editorBoundsInfo.handwritingBounds.width;
                    copy.editorBoundsInfo.handwritingBounds.height =
                            cursorAnchorInfo.editorBoundsInfo.handwritingBounds.height;
                }
            }
        }
        if (cursorAnchorInfo.visibleLineBounds != null) {
            copy.visibleLineBounds = cursorAnchorInfo.visibleLineBounds.clone();
        }
        return copy;
    }
}
