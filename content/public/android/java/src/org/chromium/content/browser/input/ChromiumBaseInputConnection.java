// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import androidx.annotation.VisibleForTesting;

/** An interface to help switch between AdapterInputConnection and ChromiumInputConnection. */
public interface ChromiumBaseInputConnection extends InputConnection {
    /** A factory class to create or reuse ChromiumBaseInputConnection. */
    public interface Factory {
        ChromiumBaseInputConnection initializeAndGet(
                View view,
                ImeAdapterImpl imeAdapter,
                int inputType,
                int inputFlags,
                int inputMode,
                int inputAction,
                int selectionStart,
                int selectionEnd,
                String lastText,
                EditorInfo outAttrs);

        @VisibleForTesting
        Handler getHandler();

        void onWindowFocusChanged(boolean gainFocus);

        void onViewFocusChanged(boolean gainFocus);

        void onViewAttachedToWindow();

        void onViewDetachedFromWindow();

        void setTriggerDelayedOnCreateInputConnection(boolean trigger);
    }

    /**
     * Updates the internal representation of the text being edited and its selection and
     * composition properties.
     *
     * @param text The String contents of the field being edited.
     * @param selectionStart The character offset of the selection start, or the caret position if
     *                       there is no selection.
     * @param selectionEnd The character offset of the selection end, or the caret position if there
     *                     is no selection.
     * @param compositionStart The character offset of the composition start, or -1 if there is no
     *                         composition.
     * @param compositionEnd The character offset of the composition end, or -1 if there is no
     *                       selection.
     * @param replyToRequest True when the update was made in a reply to IME's request.
     */
    void updateStateOnUiThread(
            String text,
            int selectionStart,
            int selectionEnd,
            int compositionStart,
            int compositionEnd,
            boolean singleLine,
            boolean replyToRequest);

    /**
     * Send key event on UI thread.
     * @param event A key event.
     */
    boolean sendKeyEventOnUiThread(KeyEvent event);

    /** Call this when restartInput() is called. */
    void onRestartInputOnUiThread();

    /**
     * @return The {@link Handler} used for this InputConnection.
     */
    @Override
    @VisibleForTesting
    Handler getHandler();

    /**
     * Unblock thread function if needed, e.g. we found that we will
     * never get state update.
     */
    void unblockOnUiThread();
}
