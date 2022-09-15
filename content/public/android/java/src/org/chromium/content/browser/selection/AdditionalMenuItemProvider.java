// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.textclassifier.TextClassification;

import java.util.List;

/**
 * An interface for adding more menu items.
 */
public interface AdditionalMenuItemProvider {
    /**
     * Add menu items to the menu passed in.
     * @param context The context from app.
     * @param menu Add menu items to this menu.
     * @param classification Providing info to generate menu items.
     */
    void addMenuItems(
            Context context, Menu menu, TextClassification classification, List<Drawable> icons);

    /**
     * Call this to trigger internal cleanup.
     */
    void clearMenuItemListeners();

    /**
     * Perform action for menu item.
     * @param item The clicked menu item.
     * @param view Perform action on this view.
     */
    void performAction(MenuItem item, View view);
}
