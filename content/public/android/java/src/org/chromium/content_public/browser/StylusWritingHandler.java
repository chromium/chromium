// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorBoundsInfo;
import android.view.inputmethod.EditorInfo;

import androidx.annotation.Nullable;

/**
 * Interface that provides Stylus handwriting to text input functionality in HTML edit fields. This
 * is implemented by a corresponding Stylus writing class that would handle the events and messages
 * in this interface outside of //content (i.e. //components/stylus_handwriting) and these are
 * called from the current WebContents related classes in //content.
 */
public interface StylusWritingHandler {
    /**
     * @return true if soft keyboard can be shown during stylus writing.
     */
    boolean canShowSoftKeyboard();

    /**
     * Check if stylus writing can be started for input field in web page.
     *
     * @return true if stylus writing can be started, false otherwise.
     */
    boolean shouldInitiateStylusWriting();

    /**
     * Update current input state parameters to stylus writing system.
     *  @param text the input text
     * @param selectionStart the input selection start offset
     * @param selectionEnd the input selection end offset
     */
    default void updateInputState(String text, int selectionStart, int selectionEnd) {}

    /**
     * Notify focused node has changed in web page.
     *
     * @param editableBoundsOnScreenDip the Editable element bounds Rect in dip
     * @param isEditable     is true if focused node is of editable type.
     * @param currentView the {@link View} in which the focused node changed.
     */
    @Nullable
    default EditorBoundsInfo onFocusedNodeChanged(
            Rect editableBoundsOnScreenDip,
            boolean isEditable,
            View currentView,
            float scaleFactor,
            int contentOffsetY) {
        return null;
    }

    /**
     * Handle touch events if needed for stylus writing.
     *
     * @param event {@link MotionEvent} to be handled.
     * @param currentView the {@link View} which is receiving the events.
     * @return true if event is consumed by stylus writing system.
     */
    default boolean handleTouchEvent(MotionEvent event, View currentView) {
        return false;
    }

    /**
     * Handle hover events for Direct writing.
     *
     * @param event {@link MotionEvent} to be handled.
     * @param currentView the {@link View} which is receiving the events.
     */
    default void handleHoverEvent(MotionEvent event, View currentView) {}

    /**
     * Update the editor attributes corresponding to current input.
     *
     * @param editorInfo {@link EditorInfo} object
     */
    default void updateEditorInfo(EditorInfo editorInfo) {}

    /**
     * Notify the view is detached from window.
     *
     * @param context the current context
     */
    default void onDetachedFromWindow(Context context) {}

    /**
     * Notify current view focus has changed
     *
     * @param hasFocus whether view gained or lost focus
     */
    default void onFocusChanged(boolean hasFocus) {}

    /**
     * This message is sent when the stylus writable element has been focused.
     *
     * @param focusedEditBounds the input field bounds in view
     * @param cursorPosition the input cursor Position point in pix
     * @param scaleFactor current device scale factor
     * @param contentOffsetY the Physical on-screen Y offset amount below the browser controls
     * @param view the view on which to start stylus handwriting
     */
    @Nullable
    default EditorBoundsInfo onEditElementFocusedForStylusWriting(
            Rect focusedEditBounds,
            Point cursorPosition,
            float scaleFactor,
            int contentOffsetY,
            View view) {
        return null;
    }

    /** Notify that ImeAdapter is destroyed. */
    default void onImeAdapterDestroyed() {}
}
