// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.content.Context;
import android.graphics.Rect;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.content.R;
import org.chromium.content_public.browser.SelectionMenuGroup;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashMap;
import java.util.Map;
import java.util.SortedSet;

/** Paste popup implementation based on floating ActionModes. */
// TODO(crbug.com/40925113): Merge this class with SelectionPopupControllerImpl and remove.
public class PasteActionModeCallback extends ActionMode.Callback2 {
    private final View mParent;
    private final SelectionPopupControllerImpl mDelegate;
    private final Context mContext;
    private final Rect mSelectionRect;
    private final @Nullable SelectionActionMenuDelegate mSelectionActionMenuDelegate;
    private final Map<MenuItem, View.OnClickListener> mCustomMenuItemClickListeners;

    public PasteActionModeCallback(
            View parent,
            Context context,
            SelectionPopupControllerImpl delegate,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate,
            Rect selectionRect) {
        mParent = parent;
        mDelegate = delegate;
        mContext = context;
        mSelectionActionMenuDelegate = selectionActionMenuDelegate;
        mSelectionRect = selectionRect;
        mCustomMenuItemClickListeners = new HashMap<>();
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        mode.setTitle(
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                        ? mContext.getString(R.string.actionbar_textselection_title)
                        : null);
        mode.setSubtitle(null);
        return true;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        SortedSet<SelectionMenuGroup> nonSelectionMenuItems =
                SelectionPopupControllerImpl.getNonSelectionMenuItems(
                        mContext, mDelegate, mSelectionActionMenuDelegate);
        mCustomMenuItemClickListeners.clear();
        SelectionPopupControllerImpl.initializeActionMenu(
                mContext, nonSelectionMenuItems, menu, mCustomMenuItemClickListeners, null);
        return true;
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        View.OnClickListener customMenuItemClickListener = mCustomMenuItemClickListeners.get(item);
        if (customMenuItemClickListener != null) {
            customMenuItemClickListener.onClick(mParent);
            mode.finish();
        } else {
            int id = item.getItemId();
            if (id == R.id.select_action_menu_paste) {
                mDelegate.paste();
                mDelegate.dismissTextHandles();
                mode.finish();
            } else if (id == R.id.select_action_menu_paste_as_plain_text) {
                mDelegate.pasteAsPlainText();
                mDelegate.dismissTextHandles();
                mode.finish();
            } else if (id == R.id.select_action_menu_select_all) {
                mDelegate.selectAll();
                mode.finish();
            }
        }
        return true;
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {}

    @Override
    public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
        outRect.set(mSelectionRect);
    }
}
