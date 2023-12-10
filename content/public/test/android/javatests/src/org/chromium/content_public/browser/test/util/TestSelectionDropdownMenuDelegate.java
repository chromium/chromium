// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A placeholder {@link SelectionDropdownMenuDelegate} to be used with tests. */
public class TestSelectionDropdownMenuDelegate implements SelectionDropdownMenuDelegate {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListMenuItemType.DIVIDER, ListMenuItemType.MENU_ITEM})
    public @interface ListMenuItemType {
        int DIVIDER = 0;
        int MENU_ITEM = 1;
    }

    @Override
    public void show(
            Context context,
            View rootView,
            MVCListAdapter.ModelList items,
            ItemClickListener clickListener,
            int x,
            int y) {}

    @Override
    public void dismiss() {}

    @Override
    public int getGroupId(PropertyModel itemModel) {
        return 0;
    }

    @Override
    public int getItemId(PropertyModel itemModel) {
        return 0;
    }

    @Nullable
    @Override
    public Intent getItemIntent(PropertyModel itemModel) {
        return null;
    }

    @Nullable
    @Override
    public View.OnClickListener getClickListener(PropertyModel itemModel) {
        return null;
    }

    @Override
    public MVCListAdapter.ListItem getDivider() {
        return new MVCListAdapter.ListItem(ListMenuItemType.DIVIDER, new PropertyModel());
    }

    @Override
    public MVCListAdapter.ListItem getMenuItem(
            String title,
            @Nullable String contentDescription,
            int groupId,
            int id,
            @Nullable Drawable startIcon,
            boolean isIconTintable,
            boolean groupContainsIcon,
            boolean enabled,
            @Nullable View.OnClickListener clickListener,
            @Nullable Intent intent) {
        return new MVCListAdapter.ListItem(ListMenuItemType.MENU_ITEM, new PropertyModel());
    }
}
