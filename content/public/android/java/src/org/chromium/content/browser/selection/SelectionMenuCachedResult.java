// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionMenuGroup;

import java.util.Objects;
import java.util.SortedSet;

/**
 * Stores text selection state and corresponding menu items for caching purposes. The {@link
 * #canReuseResult} method handles cache checking logic. We want to check all possible scenarios for
 * {@link #mClassificationResult}:
 *
 * <ol>
 *   <li>when it's null for only one state, for e.g. current selection state but not the cached one.
 *   <li>when it's null for both states. In that case compare other params to determine equivalency.
 *   <li>when it contains result in both states. Then access the text inside it, compare it, and
 *       compare other params.
 * </ol>
 */
public class SelectionMenuCachedResult {
    private final @Nullable SelectionClient.Result mClassificationResult;
    private final boolean mIsSelectionPassword;
    private final boolean mIsSelectionReadOnly;
    private final String mSelectedText;
    private final SortedSet<SelectionMenuGroup> mLastSelectionMenuItems;

    public SelectionMenuCachedResult(
            @Nullable SelectionClient.Result classificationResult,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText,
            SortedSet<SelectionMenuGroup> lastSelectionMenuItems) {
        mClassificationResult = classificationResult;
        mIsSelectionPassword = isSelectionPassword;
        mIsSelectionReadOnly = isSelectionReadOnly;
        mSelectedText = selectedText;
        mLastSelectionMenuItems = lastSelectionMenuItems;
    }

    public SortedSet<SelectionMenuGroup> getResult() {
        return mLastSelectionMenuItems;
    }

    /**
     * Handles caching logic by comparing the last selection state (params) with the current one so
     * we can utilise the cached result (menu items), if there is one.
     *
     * @param classificationResult the text classification result.
     * @param isSelectionPassword true if the selection is password.
     * @param isSelectionReadOnly true if the selection is non-editable.
     * @param selectedText the current selected text.
     * @return true if params are equivalent otherwise false.
     */
    public boolean canReuseResult(
            @Nullable SelectionClient.Result classificationResult,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText) {
        if (mIsSelectionPassword != isSelectionPassword
                || mIsSelectionReadOnly != isSelectionReadOnly
                || !Objects.equals(mSelectedText, selectedText)) {
            return false;
        }
        if ((mClassificationResult == null) != (classificationResult == null)) {
            return false;
        }
        if (mClassificationResult == null) {
            return true;
        }
        if ((mClassificationResult.textClassification == null)
                != (classificationResult.textClassification == null)) {
            return false;
        }
        if (mClassificationResult.textClassification == null) {
            return true;
        } else {
            return Objects.equals(
                    mClassificationResult.textClassification.getText(),
                    classificationResult.textClassification.getText());
        }
    }
}
