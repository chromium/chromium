// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.view.Menu;
import android.view.MenuItem;

import androidx.annotation.AttrRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.SortedSet;

/** Data class representing an item in the text selection menu. */
@NullMarked
public final class SelectionMenuItem implements Comparable<SelectionMenuItem> {
    /**
     * ItemGroupOffset refers to the first order value for which an item will belong to that
     * section. For example, menu items with an order greater than or equal to DEFAULT_ITEMS and
     * less than SECONDARY_ASSIST_ITEMS will appear in the default items section. Each section is
     * separated by a divider in dropdown menus.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        ItemGroupOffset.ASSIST_ITEMS,
        ItemGroupOffset.DEFAULT_ITEMS,
        ItemGroupOffset.SECONDARY_ASSIST_ITEMS,
        ItemGroupOffset.TEXT_PROCESSING_ITEMS
    })
    public @interface ItemGroupOffset {
        int ASSIST_ITEMS = 0;
        int DEFAULT_ITEMS = 10;
        int SECONDARY_ASSIST_ITEMS = 20;
        int TEXT_PROCESSING_ITEMS = 30;
    }

    private final @AttrRes int mIconAttr;
    private final @Nullable Drawable mIcon;
    private final @StringRes int mTitleRes;
    private final @Nullable CharSequence mTitle;
    public final @IdRes int id;
    public final @IdRes int groupId;
    public final @Nullable Character alphabeticShortcut;
    public final int order;
    public final int showAsActionFlags;
    public final @Nullable CharSequence contentDescription;
    public final @Nullable Intent intent;
    public final boolean isEnabled;
    public final boolean isIconTintable;

    private SelectionMenuItem(
            @IdRes int id,
            @IdRes int groupId,
            @AttrRes int iconAttr,
            @Nullable Drawable icon,
            @StringRes int titleRes,
            @Nullable CharSequence title,
            @Nullable Character alphabeticShortcut,
            int order,
            int showAsActionFlags,
            @Nullable CharSequence contentDescription,
            @Nullable Intent intent,
            boolean isEnabled,
            boolean isIconTintable) {
        mIconAttr = iconAttr;
        mIcon = icon;
        mTitleRes = titleRes;
        mTitle = title;
        this.id = id;
        this.groupId = groupId;
        this.alphabeticShortcut = alphabeticShortcut;
        this.order = order;
        this.showAsActionFlags = showAsActionFlags;
        this.contentDescription = contentDescription;
        this.intent = intent;
        this.isEnabled = isEnabled;
        this.isIconTintable = isIconTintable;
    }

    /** Convenience method to return the title. */
    public @Nullable CharSequence getTitle(Context context) {
        if (mTitleRes != 0) {
            return context.getString(mTitleRes);
        }
        return mTitle;
    }

    /** Convenience method to return the icon, if any. */
    public @Nullable Drawable getIcon(@Nullable Context context) {
        if (mIconAttr != 0 && context != null) {
            try {
                TypedArray a = context.obtainStyledAttributes(new int[] {mIconAttr});
                int iconResId = a.getResourceId(0, 0);
                Drawable icon =
                        iconResId == 0 ? null : AppCompatResources.getDrawable(context, iconResId);
                a.recycle();
                return icon;
            } catch (Resources.NotFoundException e) {
                return null;
            }
        }
        return mIcon;
    }

    /** For comparison. Mainly to be enable {@link SortedSet} sorting by order. */
    @Override
    public int compareTo(SelectionMenuItem otherItem) {
        return order - otherItem.order;
    }

    /** The builder class for {@link SelectionMenuItem}. */
    public static class Builder {
        private final @StringRes int mTitleRes;
        private final @Nullable CharSequence mTitle;
        public @IdRes int mId;
        private @IdRes int mGroupId;
        private @AttrRes int mIconAttr;
        private @Nullable Drawable mIcon;
        private @Nullable Character mAlphabeticShortcut;
        private int mOrder;
        private int mShowAsActionFlags;
        private @Nullable CharSequence mContentDescription;
        private @Nullable Intent mIntent;
        private boolean mIsEnabled;
        private boolean mIsIconTintable;

        /** Pass in a non-null title. */
        public Builder(@Nullable CharSequence title) {
            mTitle = title;
            mTitleRes = 0;
            initDefaults();
        }

        /** Pass in a valid string res. */
        public Builder(@StringRes int titleRes) {
            mTitleRes = titleRes;
            mTitle = null;
            initDefaults();
        }

        /**
         * Sets default values to avoid inline initialization.
         * See https://issuetracker.google.com/issues/37124982.
         */
        private void initDefaults() {
            mId = Menu.NONE;
            mIconAttr = 0;
            mIcon = null;
            mAlphabeticShortcut = null;
            mOrder = Menu.NONE;
            mShowAsActionFlags = Menu.NONE;
            mContentDescription = null;
            mIntent = null;
            mIsEnabled = true;
            mIsIconTintable = false;
        }

        /** The id of the menu item. */
        public Builder setId(@IdRes int id) {
            mId = id;
            return this;
        }

        /**
         * An id to associate the item with where it was populated from. This should usually be one
         * of the resource identifiers defined in content/public/android/java/res/values/ids.xml.
         * The groupId is used to identify individual items to handle their clicks appropriately.
         * Items added by delegates will use SelectionActionMenuDelegate#handleMenuItemClick, text
         * processing items should be handled by starting the item's intent and default items will
         * be handled manually based on their item id.
         */
        public Builder setGroupId(@IdRes int groupId) {
            mGroupId = groupId;
            return this;
        }

        /** The attribute resource for the icon. Pass 0 for none. */
        public Builder setIconAttr(@AttrRes int iconAttr) {
            mIconAttr = iconAttr;
            return this;
        }

        /** The drawable icon. */
        public Builder setIcon(@Nullable Drawable icon) {
            mIcon = icon;
            return this;
        }

        /** The character keyboard shortcut. Default modifier is Ctrl. */
        public Builder setAlphabeticShortcut(@Nullable Character alphabeticShortcut) {
            mAlphabeticShortcut = alphabeticShortcut;
            return this;
        }

        /**
         * @param order the order, must be >= 0.
         * @param category the section of the menu in which this item should appear.
         */
        public Builder setOrderAndCategory(int order, @ItemGroupOffset int category) {
            if (order < 0) {
                throw new IllegalArgumentException("Invalid order. Must be >= 0");
            }
            // Make sure items don't spill over into the next category.
            mOrder =
                    switch (category) {
                        case ItemGroupOffset.ASSIST_ITEMS ->
                                Math.min(order + category, ItemGroupOffset.DEFAULT_ITEMS - 1);
                        case ItemGroupOffset.DEFAULT_ITEMS ->
                                Math.min(
                                        order + category,
                                        ItemGroupOffset.SECONDARY_ASSIST_ITEMS - 1);
                        case ItemGroupOffset.SECONDARY_ASSIST_ITEMS ->
                                Math.min(
                                        order + category,
                                        ItemGroupOffset.TEXT_PROCESSING_ITEMS - 1);
                        default -> order + category;
                    };
            return this;
        }

        /** Should not be used directly unless constructing from an existing SelectionMenuItem. */
        public Builder setOrder(int order) {
            if (order < 0) {
                throw new IllegalArgumentException("Invalid order. Must be >= 0");
            }
            mOrder = order;
            return this;
        }

        /**
         * Must be a {@link MenuItem} constant bit combination (i.e.
         * MenuItem.SHOW_AS_ACTION_IF_ROOM) otherwise 0.
         */
        public Builder setShowAsActionFlags(int showAsActionFlags) {
            mShowAsActionFlags = showAsActionFlags;
            return this;
        }

        /** Content description for a11y. */
        public Builder setContentDescription(@Nullable CharSequence contentDescription) {
            mContentDescription = contentDescription;
            return this;
        }

        /** The {@link Intent} for the menu item. */
        public Builder setIntent(@Nullable Intent intent) {
            mIntent = intent;
            return this;
        }

        /** Pass in true if the item is enabled. Otherwise false. */
        public Builder setIsEnabled(boolean isEnabled) {
            mIsEnabled = isEnabled;
            return this;
        }

        /** Pass in true if the icon can be safely tinted. Defaults to false. */
        public Builder setIsIconTintable(boolean isIconTintable) {
            mIsIconTintable = isIconTintable;
            return this;
        }

        /** Builds the menu item. */
        public SelectionMenuItem build() {
            return new SelectionMenuItem(
                    mId,
                    mGroupId,
                    mIconAttr,
                    mIcon,
                    mTitleRes,
                    mTitle,
                    mAlphabeticShortcut,
                    mOrder,
                    mShowAsActionFlags,
                    mContentDescription,
                    mIntent,
                    mIsEnabled,
                    mIsIconTintable);
        }
    }
}
