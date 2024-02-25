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
import android.view.View;

import androidx.annotation.AttrRes;
import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import java.util.SortedSet;

/** Data class representing an item in the text selection menu. */
public final class SelectionMenuItem implements Comparable<SelectionMenuItem> {
    private final @AttrRes int mIconAttr;
    private final @Nullable Drawable mIcon;
    private final @StringRes int mTitleRes;
    private final @Nullable CharSequence mTitle;
    public final @IdRes int id;
    public final @Nullable Character alphabeticShortcut;
    public final int orderInCategory;
    public final int showAsActionFlags;
    public final @Nullable CharSequence contentDescription;
    public final @Nullable View.OnClickListener clickListener;
    public final @Nullable Intent intent;
    public final boolean isEnabled;
    public final boolean isIconTintable;

    private SelectionMenuItem(
            @IdRes int id,
            @AttrRes int iconAttr,
            @Nullable Drawable icon,
            @StringRes int titleRes,
            @Nullable CharSequence title,
            @Nullable Character alphabeticShortcut,
            int orderInCategory,
            int showAsActionFlags,
            @Nullable CharSequence contentDescription,
            @Nullable View.OnClickListener clickListener,
            @Nullable Intent intent,
            boolean isEnabled,
            boolean isIconTintable) {
        mIconAttr = iconAttr;
        mIcon = icon;
        mTitleRes = titleRes;
        mTitle = title;
        this.id = id;
        this.alphabeticShortcut = alphabeticShortcut;
        this.orderInCategory = orderInCategory;
        this.showAsActionFlags = showAsActionFlags;
        this.contentDescription = contentDescription;
        this.clickListener = clickListener;
        this.intent = intent;
        this.isEnabled = isEnabled;
        this.isIconTintable = isIconTintable;
    }

    /** Convenience method to return the title. */
    @Nullable
    public CharSequence getTitle(Context context) {
        if (mTitleRes != 0) {
            return context.getString(mTitleRes);
        }
        return mTitle;
    }

    /** Convenience method to return the icon, if any. */
    @Nullable
    public Drawable getIcon(Context context) {
        if (mIconAttr != 0) {
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
        return orderInCategory - otherItem.orderInCategory;
    }

    /** The builder class for {@link SelectionMenuItem}. */
    public static class Builder {
        private final @StringRes int mTitleRes;
        private final @Nullable CharSequence mTitle;
        public @IdRes int mId;
        private @AttrRes int mIconAttr;
        private @Nullable Drawable mIcon;
        private @Nullable Character mAlphabeticShortcut;
        private int mOrderInCategory;
        private int mShowAsActionFlags;
        private @Nullable CharSequence mContentDescription;
        private @Nullable View.OnClickListener mClickListener;
        private @Nullable Intent mIntent;
        private boolean mIsEnabled;
        private boolean mIsIconTintable;

        /** Pass in a non-null title. */
        public Builder(@NonNull CharSequence title) {
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
            mOrderInCategory = Menu.NONE;
            mShowAsActionFlags = Menu.NONE;
            mContentDescription = null;
            mClickListener = null;
            mIntent = null;
            mIsEnabled = true;
            mIsIconTintable = false;
        }

        /** The id of the menu item. */
        public Builder setId(@IdRes int id) {
            mId = id;
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
         * @param orderInCategory the order, must be >= 0.
         */
        public Builder setOrderInCategory(int orderInCategory) {
            if (orderInCategory < 0) {
                throw new IllegalArgumentException("Invalid order in category. Must be >= 0");
            }
            mOrderInCategory = orderInCategory;
            return this;
        }

        /**
         * Must be a {@link MenuItem} constant bit combination
         * (i.e. MenuItem.SHOW_AS_ACTION_IF_ROOM) otherwise 0.
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

        /** Click listener for when the menu item is clicked. */
        public Builder setClickListener(@Nullable View.OnClickListener clickListener) {
            mClickListener = clickListener;
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
                    mIconAttr,
                    mIcon,
                    mTitleRes,
                    mTitle,
                    mAlphabeticShortcut,
                    mOrderInCategory,
                    mShowAsActionFlags,
                    mContentDescription,
                    mClickListener,
                    mIntent,
                    mIsEnabled,
                    mIsIconTintable);
        }
    }
}
