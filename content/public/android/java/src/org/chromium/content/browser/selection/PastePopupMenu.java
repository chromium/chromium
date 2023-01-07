// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.graphics.Rect;

/**
 * Paste popup implementation based on TextView.PastePopupMenu.
 */
public interface PastePopupMenu {
    /**
     * Provider of paste functionality for the given popup.
     */
    public interface PastePopupMenuDelegate {
        /**
         * Called to initiate a paste after the paste option has been tapped.
         */
        void paste();

        /**
         * Called to initiate a paste as plain text after the popup has been tapped.
         */
        void pasteAsPlainText();

        /**
         * @return Whether clipboard is nonempty.
         */
        boolean canPaste();

        /**
         * Called to initiate a select all after the select all option has been tapped.
         */
        void selectAll();

        /**
         * @return Whether the select all option should be shown.
         */
        boolean canSelectAll();

        /**
         * @return Whether paste as plain text is needed.
         */
        boolean canPasteAsPlainText();
    }

    /**
     * Shows the paste popup at an appropriate location relative to the specified selection.
     */
    public void show(Rect selectionRect);

    /**
     * Hides the paste popup.
     */
    public void hide();
}
