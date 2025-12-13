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

import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A placeholder {@link SelectionDropdownMenuDelegate} to be used with tests. */
public class TestSelectionDropdownMenuDelegate implements SelectionDropdownMenuDelegate {
    private static final WritableIntPropertyKey ID = new WritableIntPropertyKey();
    private static final WritableObjectPropertyKey<String> TITLE =
            new WritableObjectPropertyKey<>();
    private static final PropertyKey[] TEST_KEYS = {ID, TITLE};

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
            HierarchicalMenuController hierarchicalMenuController,
            int x,
            int y) {}

    @Override
    public void dismiss() {}

    @Override
    public SelectionMenuItem getMinimalMenuItem(PropertyModel itemModel) {
        return new SelectionMenuItem.Builder(
                        PropertyModel.getFromModelOrDefault(itemModel, TITLE, ""))
                .setId(PropertyModel.getFromModelOrDefault(itemModel, ID, 0))
                .build();
    }

    @Override
    public MVCListAdapter.ListItem getDivider() {
        return new MVCListAdapter.ListItem(ListMenuItemType.DIVIDER, new PropertyModel(TEST_KEYS));
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
            @Nullable Intent intent,
            int order) {
        PropertyModel model = new PropertyModel(TEST_KEYS);
        model.set(ID, id);
        model.set(TITLE, title);
        return new MVCListAdapter.ListItem(ListMenuItemType.MENU_ITEM, model);
    }
}
