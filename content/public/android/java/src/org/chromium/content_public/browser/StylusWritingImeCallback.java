// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.view.View;

import org.chromium.blink.mojom.StylusWritingGestureData;

/**
 * This interface implements the IME functionality like committing text, showing or hiding soft
 * keyboard, handling writing gestures, etc. for the Stylus handwriting feature. These APIs are
 * called from classes in stylus writing module (i.e. //components/stylus_handwriting) and are
 * implemented within //content by class responsible to provide the above Ime functions in the
 * current focused input field (i.e. ImeAdapterImpl).
 */
public interface StylusWritingImeCallback {
    /**
     * Send a request to set the selection to given range.
     *
     * @param start Selection start index.
     * @param end Selection end index.
     */
    void setEditableSelectionOffsets(int start, int end);

    /**
     * Send a request to input text to the HTML input
     *
     * @param text the input text
     * @param newCursorPosition new cursor position
     * @param isCommit whether to commit and stop composing
     */
    void sendCompositionToNative(CharSequence text, int newCursorPosition, boolean isCommit);

    /**
     * Send a request to perform editor action.
     *
     * @param actionCode action code from {@link android.view.inputmethod.EditorInfo}
     */
    void performEditorAction(int actionCode);

    /** Send a request to show soft keyboard. */
    void showSoftKeyboard();

    /** Send a request to hide the soft keyboard. */
    void hideKeyboard();

    /**
     * Get the view in consideration for the current Ime.
     *
     * @return the current container view
     */
    View getContainerView();

    /**
     * Reset the current Gesture detection. This is needed when the writing system takes over
     * handling the touch events.
     */
    void resetGestureDetection();

    /**
     * Handle the action for gestures recognized by stylus writing service.
     *
     * @param id the unique id of this gesture. This is used by the gesture callback to inform
     *           Android of the gesture's result. For Android gestures, the gesture IDs are stored
     *           in {@link org.chromium.content.browser.input.ImeAdapterImpl#mOngoingGestures}. For
     *           DirectWriting, pass in -1 as no callback needs to be run.
     * @param gestureData the gesture data object that contains information regarding gesture type,
     *                    gesture coordinates, text to insert, alternative text to insert when the
     *                    gesture is invalid in current input, as applicable wrt gesture type.
     */
    void handleStylusWritingGestureAction(int id, StylusWritingGestureData gestureData);

    /** Finish current text composition in the input field. */
    void finishComposingText();
}
