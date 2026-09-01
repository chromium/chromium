// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Context;
import android.view.Menu;
import android.view.MenuItem;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.browser.selection.SelectActionMenuHelper;
import org.chromium.content_public.browser.SelectionMenuItem.ItemGroupOffset;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/**
 * Represents a selection menu while it is being created. This class holds the menu items which will
 * be added to the menu and handles the logic to populate the menu when all items have been added
 * regardless of whether the menu is a dropdown or floating menu.
 *
 * <p>Items are stored in an unsorted list but before the menu is finalized, we perform a stable
 * sort on the items based on each item's order field. This ensures that menu items with equal
 * ordering appear in the order they were added.
 */
@NullMarked
public final class PendingSelectionMenu {
    /**
     * These groupings represent sections of the menu and should only be referred to internally by
     * this class. Logical groupings DO NOT refer to the same grouping as SelectionMenuItem#groupId.
     * The groupId refers mostly to what created the menu item and is used in click handling.
     * LogicalGroup is determined based on item order and is used to split the menu into logical
     * sections. SelectionMenuItem#ItemGroupOffset is used to determine the start of each section.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        LogicalGroup.ASSIST_ITEMS,
        LogicalGroup.DEFAULT_ITEMS,
        LogicalGroup.SECONDARY_ASSIST_ITEMS,
        LogicalGroup.TEXT_PROCESSING_ITEMS
    })
    private @interface LogicalGroup {
        int ASSIST_ITEMS = 0;
        int DEFAULT_ITEMS = 1;
        int SECONDARY_ASSIST_ITEMS = 2;
        int TEXT_PROCESSING_ITEMS = 3;
        int NUM_ENTRIES = 4;
    }

    private final Context mContext;
    private final ArrayList<SelectionMenuItem> mItems;

    // These arrays store whether a group contains an icon and how many items belong to that group
    // respectively. They should be accessed by index with one of the LogicalGroup constants.
    private final boolean[] mGroupsWithIcon;
    private final int[] mGroupTotals;

    public PendingSelectionMenu(Context context) {
        mItems = new ArrayList<>();
        mContext = context;
        mGroupsWithIcon = new boolean[LogicalGroup.NUM_ENTRIES];
        mGroupTotals = new int[LogicalGroup.NUM_ENTRIES];
    }

    public void addMenuItem(SelectionMenuItem menuItem) {
        int group = determineGroup(menuItem);
        mGroupsWithIcon[group] |= menuItem.isEnabled && menuItem.getIcon(mContext) != null;
        mGroupTotals[group]++;
        mItems.add(menuItem);
    }

    public void addAll(List<SelectionMenuItem> menuItems) {
        for (SelectionMenuItem item : menuItems) {
            addMenuItem(item);
        }
    }

    /**
     * Adds the items from mItems to a ModelList. start and end are used to query specific ranges of
     * the list of items corresponding to logical groupings of menu items. Each group will be
     * displayed with a divider in between. Important note: the groups here are not equivalent to
     * groupId in the SelectionMenuItem.
     *
     * @param delegate used to create ListItems from the SelectionMenuItem data.
     * @return a model list populated with all enabled items in mItems.
     */
    public MVCListAdapter.ModelList getMenuAsDropdown(SelectionDropdownMenuDelegate delegate) {
        MVCListAdapter.ModelList items = new MVCListAdapter.ModelList();
        // Stable sort the items based on order.
        mItems.sort(Comparator.comparingInt(menuItem -> menuItem.order));

        if (mGroupTotals[LogicalGroup.ASSIST_ITEMS] != 0) {
            addDropdownMenuItemsForRange(
                    0,
                    mGroupTotals[LogicalGroup.ASSIST_ITEMS],
                    items,
                    delegate,
                    LogicalGroup.ASSIST_ITEMS);
        }
        int start = mGroupTotals[LogicalGroup.ASSIST_ITEMS];
        int end =
                mGroupTotals[LogicalGroup.ASSIST_ITEMS] + mGroupTotals[LogicalGroup.DEFAULT_ITEMS];
        if (mGroupTotals[LogicalGroup.DEFAULT_ITEMS] != 0) {
            addDropdownMenuItemsForRange(start, end, items, delegate, LogicalGroup.DEFAULT_ITEMS);
        }
        start += mGroupTotals[LogicalGroup.DEFAULT_ITEMS];
        end += mGroupTotals[LogicalGroup.SECONDARY_ASSIST_ITEMS];
        if (mGroupTotals[LogicalGroup.SECONDARY_ASSIST_ITEMS] != 0) {
            addDropdownMenuItemsForRange(
                    start, end, items, delegate, LogicalGroup.SECONDARY_ASSIST_ITEMS);
        }
        start += mGroupTotals[LogicalGroup.SECONDARY_ASSIST_ITEMS];
        end += mGroupTotals[LogicalGroup.TEXT_PROCESSING_ITEMS];
        if (mGroupTotals[LogicalGroup.TEXT_PROCESSING_ITEMS] != 0) {
            addDropdownMenuItemsForRange(
                    start, end, items, delegate, LogicalGroup.TEXT_PROCESSING_ITEMS);
        }
        return items;
    }

    /**
     * Add the items from mItems to the provided menu. This method populates the necessary fields of
     * the MenuItem and ensures that group IDs and item IDs are set appropriately for click handling
     * later.
     *
     * @param menu the menu to populate with the contents of mItems.
     */
    public void getMenuAsActionMode(Menu menu) {
        SelectActionMenuHelper.removeAllAddedGroupsFromMenu(menu);
        // Stable sort the items based on order.
        mItems.sort(Comparator.comparingInt(menuItem -> menuItem.order));

        for (SelectionMenuItem item : mItems) {
            if (!item.isEnabled) continue;
            MenuItem menuItem =
                    menu.add(item.groupId, item.id, item.order, item.getTitle(mContext));
            menuItem.setShowAsActionFlags(item.showAsActionFlags)
                    .setIcon(item.getIcon(mContext))
                    .setContentDescription(item.contentDescription)
                    .setIntent(item.intent);

            @Nullable Character alphabeticShortcut = item.alphabeticShortcut;
            if (alphabeticShortcut != null) {
                menuItem.setAlphabeticShortcut(alphabeticShortcut);
            }
        }
    }

    /**
     * Determine which group a menu item should belong to based on its position in the menu.
     *
     * @param item the item to classify.
     * @return one of the logical groupings based on the determination.
     */
    @VisibleForTesting
    public @LogicalGroup int determineGroup(SelectionMenuItem item) {
        int order = item.order;
        if (order >= ItemGroupOffset.TEXT_PROCESSING_ITEMS) {
            return LogicalGroup.TEXT_PROCESSING_ITEMS;
        } else if (order >= ItemGroupOffset.SECONDARY_ASSIST_ITEMS) {
            return LogicalGroup.SECONDARY_ASSIST_ITEMS;
        } else if (order >= ItemGroupOffset.DEFAULT_ITEMS) {
            return LogicalGroup.DEFAULT_ITEMS;
        } else if (order >= ItemGroupOffset.ASSIST_ITEMS) {
            return LogicalGroup.ASSIST_ITEMS;
        }
        throw new IllegalStateException("Invalid order. Must be >= 0");
    }

    /** Helper method to add items from a range [start,end) in mItems to a ModelList. */
    private void addDropdownMenuItemsForRange(
            int start,
            int end,
            MVCListAdapter.ModelList modelList,
            SelectionDropdownMenuDelegate delegate,
            @LogicalGroup int group) {
        if (needsDivider(group)) {
            modelList.add(delegate.getDivider());
        }
        for (int i = start; i < end; i++) {
            SelectionMenuItem item = mItems.get(i);
            CharSequence title = item.getTitle(mContext);
            CharSequence contentDescription = item.contentDescription;
            modelList.add(
                    delegate.getMenuItem(
                            title != null ? title.toString() : null,
                            contentDescription != null ? contentDescription.toString() : null,
                            item.groupId,
                            item.id,
                            item.getIcon(mContext),
                            item.isIconTintable,
                            mGroupsWithIcon[group],
                            true,
                            item.intent,
                            item.order));
        }
    }

    private boolean needsDivider(@LogicalGroup int group) {
        // All groups should be preceded by a divider except ASSIST_ITEMS or DEFAULT_ITEMS if there
        // are no ASSIST_ITEMS.
        if (group == LogicalGroup.ASSIST_ITEMS) return false;
        return group != LogicalGroup.DEFAULT_ITEMS || mGroupTotals[LogicalGroup.ASSIST_ITEMS] != 0;
    }

    public ArrayList<SelectionMenuItem> getMenuItemsForTesting() {
        // Stable sort the items based on order.
        mItems.sort(Comparator.comparingInt(menuItem -> menuItem.order));
        return mItems;
    }
}
