// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.selection;

import android.content.pm.ResolveInfo;
import android.view.View;

import org.chromium.base.SelectionActionMenuClientWrapper.DefaultItem;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

/**
 * Interface for modifying text selection menu functionality. Content embedders can provide
 * implementation to provide text selection menu item custom behavior.
 */
@NullMarked
public interface SelectionActionMenuDelegate {
    static @DefaultItem int[] getDefaultMenuItemOrder() {
        return new @DefaultItem int[] {
            DefaultItem.CUT,
            DefaultItem.COPY,
            DefaultItem.PASTE,
            DefaultItem.PASTE_AS_PLAIN_TEXT,
            DefaultItem.SHARE,
            DefaultItem.SELECT_ALL,
            DefaultItem.WEB_SEARCH
        };
    }

    /**
     * Returns an array of menu items representing the order in which they should be shown in the
     * menu. Delegate implementations can either return their custom order or use the default order
     * by calling the super (default) implementation below.
     *
     * @param menuType whether the menu is a floating action mode menu or a dropdown menu.
     * @return the desired order.
     */
    default @DefaultItem int[] getDefaultMenuItemOrder(@MenuType int menuType) {
        return SelectionActionMenuDelegate.getDefaultMenuItemOrder();
    }

    /**
     * Lets a delegate implementation provide additional menu items for the selection menu.
     * Delegates may wish to show different menu items based on the arguments provided.
     *
     * @param menuType whether the menu is a floating action mode menu or a dropdown menu.
     * @param isSelectionPassword {@code true} if the input field is a password field.
     * @param isSelectionReadOnly {@code true} if the input field is not editable.
     * @param selectedText the highlighted text for which this menu is being shown.
     * @return a list of additional menu items or an empty list otherwise.
     */
    List<SelectionMenuItem> getAdditionalMenuItems(
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText);

    /**
     * Allows filtering of text processing activities.
     *
     * @param menuType whether the menu is a floating action mode menu or a dropdown menu.
     * @param activities list of text processing activities to be filtered.
     * @return list of text processing activities after filtering.
     */
    List<ResolveInfo> filterTextProcessingActivities(
            @MenuType int menuType, List<ResolveInfo> activities);

    /**
     * Queries if selection menu item cache can be reused. Selection menu's items can be cached for
     * repeated selections. Delegate can add menu items using {@link #getAdditionalMenuItems(int,
     * boolean, boolean, String)} API due to which repeated selections can result in different
     * selection menu items being shown.
     *
     * @param menuType whether the menu is a floating action mode menu or a dropdown menu.
     * @return True, if cached selection menu items can be reused for repeated selection, False
     *     otherwise.
     */
    boolean canReuseCachedSelectionMenu(@MenuType int menuType);

    /**
     * Handles when an item in the selection menu is clicked by the user or activated using the
     * relevant shortcut. This method is only called for menu items supplied via the
     * getAdditionalMenuItems method.
     *
     * @return True if the click was handled by this class or false otherwise.
     */
    default boolean handleMenuItemClick(
            SelectionMenuItem item, WebContents webContents, @Nullable View containerView) {
        return false;
    }
}
