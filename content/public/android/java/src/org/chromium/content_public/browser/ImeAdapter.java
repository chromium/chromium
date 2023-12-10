// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Context;
import android.os.ResultReceiver;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import androidx.annotation.VisibleForTesting;

import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.ui.base.WindowAndroid;

/** Adapts and plumbs android IME service onto the chrome text input API. */
public interface ImeAdapter {
    /** Composition key code sent when user either hit a key or hit a selection. */
    static final int COMPOSITION_KEY_CODE = 229;

    /**
     * @param webContents {@link WebContents} object.
     * @return {@link ImeAdapter} object used for the give WebContents.
     *         {@code null} if not available.
     */
    static ImeAdapter fromWebContents(WebContents webContents) {
        return ImeAdapterImpl.fromWebContents(webContents);
    }

    /**
     * @return the default {@link InputMethodManagerWrapper} that the ImeAdapter uses to
     * make calls to the InputMethodManager.
     */
    static InputMethodManagerWrapper createDefaultInputMethodManagerWrapper(
            Context context,
            WindowAndroid windowAndroid,
            InputMethodManagerWrapper.Delegate delegate) {
        return ImeAdapterImpl.createDefaultInputMethodManagerWrapper(
                context, windowAndroid, delegate);
    }

    /**
     * @return the active {@link InputConnection} that the IME uses to communicate updates to its
     * clients.
     */
    InputConnection getActiveInputConnection();

    /**
     * Add {@link ImeEventObserver} object to {@link ImeAdapter}.
     * @param observer imeEventObserver instance to add.
     */
    void addEventObserver(ImeEventObserver observer);

    /**
     * @see View#onCreateInputConnection(EditorInfo)
     */
    InputConnection onCreateInputConnection(EditorInfo outAttrs);

    /**
     * @see View#onCheckIsTextEditor()
     */
    boolean onCheckIsTextEditor();

    /**
     * Overrides the InputMethodManagerWrapper that ImeAdapter uses to make calls to
     * InputMethodManager.
     * @param immw InputMethodManagerWrapper that should be used to call InputMethodManager.
     */
    void setInputMethodManagerWrapper(InputMethodManagerWrapper immw);

    /**
     * Advances the focus to next input field in the current form.
     *
     * @param focusType indicates whether to advance forward or backward direction.
     */
    void advanceFocusForIME(int focusType);

    /**
     * @return a newly instantiated {@link ResultReceiver} used to scroll to the editable
     *     node at the right timing.
     */
    @VisibleForTesting
    ResultReceiver getNewShowKeyboardReceiver();

    /** Get the current input connection for testing purposes. */
    InputConnection getInputConnectionForTest();

    /**
     * Replace the currently composing text with the given text, and set the new cursor position.
     * @param text The composing text.
     * @param newCursorPosition The new cursor position around the text.
     */
    void setComposingTextForTest(final CharSequence text, final int newCursorPosition);

    /**
     * Call this when we get result from ResultReceiver passed in calling showSoftInput().
     * @param resultCode The result of showSoftInput() as defined in InputMethodManager.
     */
    @VisibleForTesting
    void onShowKeyboardReceiveResult(int resultCode);
}
