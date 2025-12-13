// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.view.ActionMode;

import org.chromium.build.annotations.NullMarked;

/**
 * An {@link ActionMode.Callback2} adapter that adds APIs that are not dependent on
 * {@link ActionMode}, {@link android.view.Menu} or {@link android.view.MenuItem}.
 */
@NullMarked
public abstract class ActionModeCallback extends ActionMode.Callback2 {
    /**
     * Callback for handling drop-down menu item clicks.
     *
     * @param item a minimal representation of the item clicked. See
     *     SelectionDropdownMenuDelegate#getMinimalMenuItem for a list of the fields that are valid
     *     to read.
     * @param closeMenu whether the menu should be closed after clicking the item.
     * @return true if this callback handled the event, false if the standard handling should
     *     continue.
     */
    public abstract boolean onDropdownItemClicked(SelectionMenuItem item, boolean closeMenu);
}
