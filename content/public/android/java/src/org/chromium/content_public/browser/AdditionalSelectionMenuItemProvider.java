// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import java.util.List;

/**
 * Interface for providing additional menu items w/ click handling to the {@link
 * SelectionPopupController}.
 */
public interface AdditionalSelectionMenuItemProvider {
    /**
     * Returns the list of items if any.
     */
    List<SelectionMenuItem> getItems();

    /**
     * Callback for when the menu is destroyed.
     */
    void onMenuDestroyed();
}
