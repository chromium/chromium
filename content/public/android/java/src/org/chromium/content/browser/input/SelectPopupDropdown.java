// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.view.View;
import android.widget.AdapterView;
import android.widget.PopupWindow;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.DropdownAdapter;
import org.chromium.ui.DropdownPopupWindow;

import java.util.List;

/**
 * Handles the dropdown popup for the <select> HTML tag support.
 */
public class SelectPopupDropdown implements SelectPopup.Ui {
    private final Callback<int[]> mSelectionChangedCallback;
    private final DropdownPopupWindow mDropdownPopupWindow;

    private boolean mSelectionNotified;

    public SelectPopupDropdown(Context context, Callback<int[]> selectionChangedCallback,
            View anchorView, List<SelectPopupItem> items, int[] selected, boolean rightAligned,
            WebContents webContents) {
        mSelectionChangedCallback = selectionChangedCallback;
        mDropdownPopupWindow = new DropdownPopupWindow(context, anchorView);
        mDropdownPopupWindow.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                notifySelection(new int[] {position});
                hide(false);
            }
        });

        int initialSelection = -1;
        if (selected.length > 0) {
            initialSelection = selected[0];
        }
        mDropdownPopupWindow.setInitialSelection(initialSelection);
        mDropdownPopupWindow.setAdapter(new DropdownAdapter(context, items, null /* separators */));
        mDropdownPopupWindow.setRtl(rightAligned);
        mDropdownPopupWindow.setOnDismissListener(
                new PopupWindow.OnDismissListener() {
                    @Override
                    public void onDismiss() {
                        notifySelection(null);
                    }
                });
        GestureListenerManager.fromWebContents(webContents).addListener(new GestureStateListener() {
            @Override
            public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
                hide(true);
            }
        });
    }

    private void notifySelection(int[] indicies) {
        if (mSelectionNotified) return;
        mSelectionChangedCallback.onResult(indicies);
        mSelectionNotified = true;
    }

    @Override
    public void show() {
        // postShow() to make sure show() happens after the layout of the anchor view has been
        // changed.
        mDropdownPopupWindow.postShow();
    }

    @Override
    public void hide(boolean sendsCancelMessage) {
        if (sendsCancelMessage) {
            mDropdownPopupWindow.dismiss();
            notifySelection(null);
        } else {
            mSelectionNotified = true;
            mDropdownPopupWindow.dismiss();
        }
    }
}
