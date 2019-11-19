// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Intent;
import android.view.ActionMode;
import android.view.textclassifier.TextClassifier;

import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.ui.base.WindowAndroid;

/**
 * An interface that handles input-related web content selection UI like action mode
 * and paste popup view. It wraps an {@link ActionMode} created by the associated view,
 * providing modified interaction with it.
 *
 * Embedders can use {@link ActionModeCallbackHelper} provided by the implementation of
 * this interface to create {@link ActionMode.Callback} instance and configure the selection
 * action mode tasks to their requirements.
 */
public interface SelectionPopupController {
    /**
     * @param webContents {@link WebContents} object.
     * @return {@link SelectionPopupController} object used for the give WebContents.
     *         {@code null} if not available.
     */
    static SelectionPopupController fromWebContents(WebContents webContents) {
        return SelectionPopupControllerImpl.fromWebContents(webContents);
    }

    /**
     * Makes {@link SelectionPopupcontroller} to use the readback view from {@link WindowAndroid}
     */
    static void setShouldGetReadbackViewFromWindowAndroid() {
        SelectionPopupControllerImpl.setShouldGetReadbackViewFromWindowAndroid();
    }

    /**
     * Set {@link ActionMode.Callback} used by {@link SelectionPopupController}.
     * @param callback ActionMode.Callback instance.
     */
    void setActionModeCallback(ActionMode.Callback callback);

    /**
     * Set {@link ActionMode.Callback} used by {@link SelectionPopupController} when no text is
     * selected.
     * @param callback ActionMode.Callback instance.
     */
    void setNonSelectionActionModeCallback(ActionMode.Callback callback);

    /**
     * @return {@link SelectionClient.ResultCallback} instance.
     */
    SelectionClient.ResultCallback getResultCallback();

    /**
     * @return The selected text (empty if no text selected).
     */
    String getSelectedText();

    /**
     * @return Whether the current focused node is editable.
     */
    boolean isFocusedNodeEditable();

    /**
     * @return Whether the page has an active, touch-controlled selection region.
     */
    boolean hasSelection();

    /**
     * Hide action mode and put into destroyed state.
     */
    void destroySelectActionMode();

    boolean isSelectActionBarShowing();

    /**
     * @return {@link ActionModeCallbackHelper} object.
     */
    ActionModeCallbackHelper getActionModeCallbackHelper();

    /**
     * Clears the current text selection. Note that we will try to move cursor to selection
     * end if applicable.
     */
    void clearSelection();

    /**
     * Called when the processed text is replied from an activity that supports
     * Intent.ACTION_PROCESS_TEXT.
     * @param resultCode the code that indicates if the activity successfully processed the text
     * @param data the reply that contains the processed text.
     */
    void onReceivedProcessTextResult(int resultCode, Intent data);

    /** Sets the given {@link SelectionClient} in the selection popup controller. */
    void setSelectionClient(SelectionClient selectionClient);

    /**
     * Sets TextClassifier for Smart Text selection.
     */
    void setTextClassifier(TextClassifier textClassifier);

    /**
     * Returns TextClassifier that is used for Smart Text selection. If the custom classifier
     * has been set with setTextClassifier, returns that object, otherwise returns the system
     * classifier.
     */
    TextClassifier getTextClassifier();

    /**
     * Returns the TextClassifier which has been set with setTextClassifier(), or null.
     */
    TextClassifier getCustomTextClassifier();

    /**
     * Set the flag indicating where the selection is preserved the next time the view loses focus.
     * @param preserve {@code true} if the selection needs to be preserved.
     */
    void setPreserveSelectionOnNextLossOfFocus(boolean preserve);

    /**
     * Update the text selection UI depending on the focus of the page. This will hide the selection
     * handles and selection popups if focus is lost.
     * TODO(mdjones): This was added as a temporary measure to hide text UI while Reader Mode or
     * Contextual Search are showing. This should be removed in favor of proper focusing of the
     * panel's WebContents (which is currently not being added to the view hierarchy).
     * @param focused If the WebContents currently has focus.
     */
    void updateTextSelectionUI(boolean focused);
}
