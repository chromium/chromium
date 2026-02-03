// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.util.Pair;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;

// This class provides a mock implementation for extended selection. For
// simplicity this class keeps track of only one extended selection.
@NullMarked
@ServiceImpl(AconfigFlaggedApiDelegate.class)
public class FakeAconfigFlaggedApiDelegate implements AconfigFlaggedApiDelegate {
    private int mStartVirtualDescendantId = -1;
    private int mStartOffset;
    private int mEndVirtualDescendantId;
    private int mEndOffset;

    @Override
    public void setSelection(
            AccessibilityNodeInfoCompat info,
            android.view.View view,
            int startVirtualDescendantId,
            int startOffset,
            int endVirtualDescendantId,
            int endOffset) {
        mStartVirtualDescendantId = startVirtualDescendantId;
        mStartOffset = startOffset;
        mEndVirtualDescendantId = endVirtualDescendantId;
        mEndOffset = endOffset;
    }

    @Override
    public void clearSelection(AccessibilityNodeInfoCompat info) {
        mStartVirtualDescendantId = -1;
    }

    @Override
    public @Nullable Pair<Integer, Integer> getExtendedSelectionStart(
            AccessibilityNodeInfoCompat info) {
        return mStartVirtualDescendantId == -1
                ? null
                : new Pair<>(mStartVirtualDescendantId, mStartOffset);
    }

    @Override
    public @Nullable Pair<Integer, Integer> getExtendedSelectionEnd(
            AccessibilityNodeInfoCompat info) {
        return mStartVirtualDescendantId == -1
                ? null
                : new Pair<>(mEndVirtualDescendantId, mEndOffset);
    }
}
