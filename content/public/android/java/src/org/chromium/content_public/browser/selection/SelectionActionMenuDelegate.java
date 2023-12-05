// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.selection;

import org.chromium.content_public.browser.SelectionMenuItem;

import java.util.List;

/**
 * Interface for modifying text selection menu functionality. Content embedders can provide
 * implementation to provide text selection menu item custom behavior.
 */
public interface SelectionActionMenuDelegate {
    /**
     * Used to modify menu items.
     *
     * @param menuItemBuilders menu item builder list which need to be modified.
     */
    void getModifiedMenuItems(List<SelectionMenuItem.Builder> menuItemBuilders);
}
