// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.graphics.Rect;
import android.os.Build;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.RequiresApi;

import org.chromium.content_public.browser.ActionModeCallbackHelper;

/**
 * A class thatextends ActionMode.Callback2 to support floating ActionModes.
 */
@RequiresApi(Build.VERSION_CODES.M)
public class FloatingActionModeCallback extends ActionMode.Callback2 {
    private final ActionModeCallbackHelper mHelper;
    private final ActionMode.Callback mCallback;

    public FloatingActionModeCallback(ActionModeCallbackHelper helper,
            ActionMode.Callback callback) {
        mHelper = helper;
        mCallback = callback;
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        // If the created ActionMode isn't actually floating, abort creation altogether.
        if (mode.getType() != ActionMode.TYPE_FLOATING) return false;
        return mCallback.onCreateActionMode(mode, menu);
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        return mCallback.onPrepareActionMode(mode, menu);
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        return mCallback.onActionItemClicked(mode, item);
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {
        mCallback.onDestroyActionMode(mode);
    }

    @Override
    public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
        mHelper.onGetContentRect(mode, view, outRect);
    }
}
