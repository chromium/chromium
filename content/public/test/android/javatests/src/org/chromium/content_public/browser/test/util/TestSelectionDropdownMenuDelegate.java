// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A placeholder {@link SelectionDropdownMenuDelegate} to be used with tests.
 */
public class TestSelectionDropdownMenuDelegate implements SelectionDropdownMenuDelegate {
    @Override
    public void show(Context context, View rootView, MVCListAdapter.ModelList items,
            ItemClickListener clickListener, int x, int y) {}

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
    public View.OnClickListener getClickListener(PropertyModel itemModel) {
        return null;
    }

    @Override
    public MVCListAdapter.ListItem getDivider() {
        return null;
    }

    @Override
    public MVCListAdapter.ListItem getMenuItem(String title, @Nullable String contentDescription,
            int groupId, int id, @Nullable Drawable startIcon, boolean isIconTintable,
            boolean groupContainsIcon, boolean enabled,
            @Nullable View.OnClickListener clickListener) {
        return null;
    }
}
