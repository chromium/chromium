// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import org.chromium.blink.mojom.EditorBoundsInfo;
import org.chromium.blink.mojom.InputCursorAnchorInfo;
import org.chromium.blink.mojom.TextAppearanceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.skia.mojom.SkColor;

import java.util.Arrays;
import java.util.Objects;

/**
 * Compares two InputCursorAnchorInfo objects to check if they are deeply equal. We don't implement
 * the Comparator interface as it doesn't make sense to compare two structs for order.
 */
@NullMarked
@SuppressWarnings("ReferenceEquality")
final class InputCursorAnchorInfoComparator {
    public static boolean equals(
            @Nullable InputCursorAnchorInfo o1, @Nullable InputCursorAnchorInfo o2) {
        if (o1 == o2) {
            return true;
        }
        if (o1 == null || o2 == null) {
            return false;
        }
        // o1 and o2 are non null.
        if (!Arrays.deepEquals(o1.characterBounds, o2.characterBounds)) {
            return false;
        }
        if (!editorBoundsAreEqual(o1.editorBoundsInfo, o2.editorBoundsInfo)) {
            return false;
        }
        if (!textAppearanceInfoAreEqual(o1.textAppearanceInfo, o2.textAppearanceInfo)) {
            return false;
        }
        if (!Arrays.deepEquals(o1.visibleLineBounds, o2.visibleLineBounds)) {
            return false;
        }
        return true;
    }

    private static boolean editorBoundsAreEqual(
            @Nullable EditorBoundsInfo o1, @Nullable EditorBoundsInfo o2) {
        if (o1 == o2) {
            return true;
        }
        if (o1 == null || o2 == null) {
            return false;
        }
        // o1 and o2 are non null.
        if (!Objects.equals(o1.editorBounds, o2.editorBounds)) {
            return false;
        }
        if (!Objects.equals(o1.handwritingBounds, o2.handwritingBounds)) {
            return false;
        }
        return true;
    }

    private static boolean textAppearanceInfoAreEqual(
            @Nullable TextAppearanceInfo o1, @Nullable TextAppearanceInfo o2) {
        if (o1 == o2) {
            return true;
        }
        if (o1 == null || o2 == null) {
            return false;
        }
        // o1 and o2 are non null.
        if (!skColorsAreEqual(o1.textColor, o2.textColor)) {
            return false;
        }
        return true;
    }

    private static boolean skColorsAreEqual(@Nullable SkColor o1, @Nullable SkColor o2) {
        if (o1 == o2) {
            return true;
        }
        if (o1 == null || o2 == null) {
            return false;
        }
        // o1 and o2 are non null.
        return o1.value == o2.value;
    }

    private InputCursorAnchorInfoComparator() {}
}
