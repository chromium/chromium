// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.selection;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Interface that provides dropdown text selection context menu functionality.
 * Each content embedder will need to provide an implementation of this to enable
 * the behavior when showing the context menu for mouse & trackpad.
 */
public interface SelectionDropdownMenuDelegate {
    /** Listener for handling list item click events. */
    @FunctionalInterface
    interface ItemClickListener {
        /** Called when an item is clicked within the dropdown menu. */
        void onItemClick(PropertyModel itemModel);
    }

    /**
     * Attempts to show the dropdown anchored by its top-left corner at the passed in
     * x and y offset if there is room. Otherwise it will pick another corner to
     * ensure the entire dropdown fits on the screen.
     * @param context the context needed to show the dropdown menu.
     * @param rootView the root view of the dropdown menu.
     * @param items the items that will be shown inside the dropdown menu.
     * @param clickListener the click listener for the items in the dropdown menu.
     * @param x The x offset of the dropdown menu relative to the container View.
     * @param y The y offset of the dropdown menu relative to the container View.
     */
    void show(
            Context context,
            View rootView,
            MVCListAdapter.ModelList items,
            ItemClickListener clickListener,
            @Px int x,
            @Px int y);

    /** Dismisses the dropdown menu. */
    void dismiss();

    /** Returns the group id for an item if it's present. Otherwise returns 0. */
    @IdRes
    int getGroupId(PropertyModel itemModel);

    /** Returns the id for an item if it's present. Otherwise returns 0. */
    @IdRes
    int getItemId(PropertyModel itemModel);

    /** Returns the intent for an item if it's present. Otherwise null is returned. */
    @Nullable
    Intent getItemIntent(PropertyModel itemModel);

    /**
     * Returns the {@link android.view.View.OnClickListener} for an item if there is
     * one. Otherwise returns null.
     */
    @Nullable
    View.OnClickListener getClickListener(PropertyModel itemModel);

    /** Returns a divider menu item to be shown in the dropdown menu. */
    ListItem getDivider();

    /**
     * Returns a menu item. Pass 0 for attributes that aren't
     * applicable to the menu item (e.g. if there is no icon or text).
     * @param title The text on the menu item.
     * @param contentDescription The content description of the menu item.
     * @param groupId The group id of the menu item.
     * @param id Id of the menu item.
     * @param startIcon The icon at the start of the menu item.
     * @param isIconTintable True if the icon can be tinted.
     * @param groupContainsIcon True if this or any other item in group has an icon.
     * @param enabled Whether or not this menu item should be enabled.
     * @param clickListener Optional click listener for the menu item.
     * @param intent Optional intent for the menu item.
     * @return ListItem with text and optionally an icon.
     */
    ListItem getMenuItem(
            String title,
            @Nullable String contentDescription,
            @IdRes int groupId,
            @IdRes int id,
            @Nullable Drawable startIcon,
            boolean isIconTintable,
            boolean groupContainsIcon,
            boolean enabled,
            @Nullable View.OnClickListener clickListener,
            @Nullable Intent intent);
}
