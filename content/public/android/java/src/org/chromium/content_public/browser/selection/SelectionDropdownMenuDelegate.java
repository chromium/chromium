// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.selection;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.view.Menu;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Interface that provides dropdown text selection context menu functionality. Each content embedder
 * will need to provide an implementation of this to enable the behavior when showing the context
 * menu for mouse & trackpad.
 */
@NullMarked
public interface SelectionDropdownMenuDelegate {
    /** Listener for handling list item click events. */
    @FunctionalInterface
    interface ItemClickListener {
        /** Called when an item is clicked within the dropdown menu. */
        void onItemClick(PropertyModel itemModel);
    }

    /**
     * Attempts to show the dropdown anchored by its top-left corner at the passed in x and y offset
     * if there is room. Otherwise it will pick another corner to ensure the entire dropdown fits on
     * the screen.
     *
     * @param context The context needed to show the dropdown menu.
     * @param rootView The root view of the dropdown menu.
     * @param items The items that will be shown inside the dropdown menu.
     * @param clickListener The click listener for the items in the dropdown menu.
     * @param hierarchicalMenuController The {@code HierarchicalMenuController} to use to display
     *     nested menus.
     * @param x The x offset of the dropdown menu relative to the container View.
     * @param y The y offset of the dropdown menu relative to the container View.
     */
    void show(
            Context context,
            View rootView,
            MVCListAdapter.ModelList items,
            ItemClickListener clickListener,
            HierarchicalMenuController hierarchicalMenuController,
            @Px int x,
            @Px int y);

    /** Dismisses the dropdown menu. */
    void dismiss();

    /**
     * Return a minimal SelectionMenuItem with only the following fields set (add to this list if
     * you add more): Title, Id, GroupId, Order, Intent, ClickListener.
     */
    default SelectionMenuItem getMinimalMenuItem(PropertyModel itemModel) {
        return new SelectionMenuItem.Builder(
                        PropertyModel.getFromModelOrDefault(
                                itemModel, ListMenuItemProperties.TITLE, ""))
                .setId(
                        PropertyModel.getFromModelOrDefault(
                                itemModel, ListMenuItemProperties.MENU_ITEM_ID, 0))
                .setGroupId(
                        PropertyModel.getFromModelOrDefault(
                                itemModel, ListMenuItemProperties.GROUP_ID, 0))
                .setOrder(
                        PropertyModel.getFromModelOrDefault(
                                itemModel, ListMenuItemProperties.ORDER, Menu.CATEGORY_ALTERNATIVE))
                .setIntent(
                        PropertyModel.getFromModelOrDefault(
                                itemModel, ListMenuItemProperties.INTENT, null))
                .build();
    }

    /**
     * Returns the click listener for the given item. If the click listener is unset or the key
     * doesn't exist, return null.
     */
    default View.@Nullable OnClickListener getClickListener(PropertyModel itemModel) {
        return PropertyModel.getFromModelOrDefault(
                itemModel, ListMenuItemProperties.CLICK_LISTENER, null);
    }

    /** Returns a divider menu item to be shown in the dropdown menu. */
    ListItem getDivider();

    /**
     * Returns a menu item. Pass 0 for attributes that aren't applicable to the menu item (e.g. if
     * there is no icon or text).
     *
     * @param title The text on the menu item.
     * @param contentDescription The content description of the menu item.
     * @param groupId The group id of the menu item.
     * @param id Id of the menu item.
     * @param startIcon The icon at the start of the menu item.
     * @param isIconTintable True if the icon can be tinted.
     * @param groupContainsIcon True if this or any other item in group has an icon.
     * @param enabled Whether or not this menu item should be enabled.
     * @param intent Optional intent for the menu item.
     * @return ListItem with text and optionally an icon.
     */
    ListItem getMenuItem(
            @Nullable String title,
            @Nullable String contentDescription,
            @IdRes int groupId,
            @IdRes int id,
            @Nullable Drawable startIcon,
            boolean isIconTintable,
            boolean groupContainsIcon,
            boolean enabled,
            @Nullable Intent intent,
            int order);

    /** Returns a pointer to a native SelectionPopupDelegate. */
    default long getNativeDelegate() {
        return 0L;
    }
}
