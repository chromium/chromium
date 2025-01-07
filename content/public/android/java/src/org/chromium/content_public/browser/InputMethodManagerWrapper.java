// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.os.IBinder;
import android.os.ResultReceiver;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;

import org.chromium.ui.base.WindowAndroid;

/** Wrapper around Android's InputMethodManager so that the implementation can be swapped out. */
public interface InputMethodManagerWrapper {
    /** An embedder may implement this for multi-display support. */
    public interface Delegate {
        /** Whether the delegate has established an input connection. */
        boolean hasInputConnection();
    }

    /**
     * @see android.view.inputmethod.InputMethodManager#restartInput(View)
     */
    void restartInput(View view);

    /**
     * @see android.view.inputmethod.InputMethodManager#showSoftInput(View, int, ResultReceiver)
     */
    void showSoftInput(View view, int flags, ResultReceiver resultReceiver);

    /**
     * @see android.view.inputmethod.InputMethodManager#isActive(View)
     */
    boolean isActive(View view);

    /**
     * @see android.view.inputmethod.InputMethodManager#hideSoftInputFromWindow(IBinder, int,
     * ResultReceiver)
     */
    boolean hideSoftInputFromWindow(IBinder windowToken, int flags, ResultReceiver resultReceiver);

    /**
     * @see android.view.inputmethod.InputMethodManager#updateSelection(View, int, int, int, int)
     */
    void updateSelection(
            View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd);

    /**
     * @see android.view.inputmethod.InputMethodManager#updateCursorAnchorInfo(View,
     * CursorAnchorInfo)
     */
    void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo);

    /**
     * @see android.view.inputmethod.InputMethodManager
     * #updateExtractedText(View,int, ExtractedText)
     */
    void updateExtractedText(View view, int token, android.view.inputmethod.ExtractedText text);

    /**
     * Call this when WindowAndroid object has changed.
     * @param newWindowAndroid The new WindowAndroid object.
     */
    void onWindowAndroidChanged(WindowAndroid newWindowAndroid);

    /** Call this when non-null InputConnection has been created. */
    void onInputConnectionCreated();
}
