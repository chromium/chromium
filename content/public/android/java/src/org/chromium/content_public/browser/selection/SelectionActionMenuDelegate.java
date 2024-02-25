// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.selection;

import android.content.pm.ResolveInfo;

import androidx.annotation.NonNull;

import org.chromium.content_public.browser.SelectionMenuItem;

import java.util.List;

/**
 * Interface for modifying text selection menu functionality. Content embedders can provide
 * implementation to provide text selection menu item custom behavior.
 */
public interface SelectionActionMenuDelegate {
    /**
     * Allows the delegate make changes to default menu items created by {@link
     * SelectionPopupController}. These menu items are present in both text selection and no text
     * selection scenario if they satisfy respective conditions and are present before additional
     * menu items from {@link #getAdditionalNonSelectionItems()} and {@link
     * #getAdditionalTextProcessingItems()}.
     *
     * @param menuItemBuilders default menu item builder list which need to be modified.
     * @param isSelectionPassword True if current selection is of password type, False otherwise.
     * @param selectedText The selected text (empty if no text selected).
     */
    void modifyDefaultMenuItems(
            List<SelectionMenuItem.Builder> menuItemBuilders,
            boolean isSelectionPassword,
            @NonNull String selectedText);

    /**
     * Allows filtering of text processing activities.
     *
     * @param activities list to text processing activities to be filtered.
     * @return list of text processing activities after filtering.
     */
    List<ResolveInfo> filterTextProcessingActivities(List<ResolveInfo> activities);

    /**
     * Provides additional menu items when no text is selected and while editing text with a cursor.
     * These menu items are ordered after default menu items from {@link
     * #modifyDefaultMenuItems(List)} if any.
     *
     * @return list of additional non selection secondary menu items if any.
     */
    @NonNull
    List<SelectionMenuItem> getAdditionalNonSelectionItems();

    /**
     * Provides additional menu items which registers for text processing when text is selected.
     * These menu items are ordered after default menu items from {@link
     * #modifyDefaultMenuItems(List)} if any.
     *
     * @return list of additional text selection menu items handling text processing if any.
     */
    @NonNull
    List<SelectionMenuItem> getAdditionalTextProcessingItems();
}
