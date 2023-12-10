// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Intent;
import android.view.ActionMode;
import android.view.View;

import androidx.annotation.Nullable;

/**
 * An {@link ActionMode.Callback2} adapter that adds APIs that are not dependent on
 * {@link ActionMode}, {@link android.view.Menu} or {@link android.view.MenuItem}.
 */
public abstract class ActionModeCallback extends ActionMode.Callback2 {
    /**
     * Callback for handling drop-down menu item clicks.
     * @param groupId the id of the group that the item belongs to.
     * @param id the id of item that was clicked.
     * @param intent the intent of the item that was clicked.
     * @param clickListener the custom click listener for the item that was clicked.
     * @return true if this callback handled the event, false if the standard handling should
     *         continue.
     */
    public abstract boolean onDropdownItemClicked(
            int groupId,
            int id,
            @Nullable Intent intent,
            @Nullable View.OnClickListener clickListener);
}
