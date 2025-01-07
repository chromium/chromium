// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.res.Configuration;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.content.browser.ViewEventSinkImpl;

/** Interface for updating content with view events. */
public interface ViewEventSink {
    /**
     * Interface that consumers of WebContents must implement to allow the proper
     * dispatching of view methods through the containing view.
     *
     * <p>
     * All methods with the "super_" prefix should be routed to the parent of the
     * implementing container view.
     */
    @SuppressWarnings("javadoc")
    public interface InternalAccessDelegate {
        /**
         * @see View#onKeyUp(keyCode, KeyEvent)
         */
        boolean super_onKeyUp(int keyCode, KeyEvent event);

        /**
         * @see View#dispatchKeyEvent(KeyEvent)
         */
        boolean super_dispatchKeyEvent(KeyEvent event);

        /**
         * @see View#onGenericMotionEvent(MotionEvent)
         */
        boolean super_onGenericMotionEvent(MotionEvent event);

        /**
         * @see View#onScrollChanged(int, int, int, int)
         */
        void onScrollChanged(int lPix, int tPix, int oldlPix, int oldtPix);
    }

    /**
     * @return {@link ViewEventSink} instance for a given {@link WebContents}.
     */
    public static ViewEventSink from(WebContents webContents) {
        return ViewEventSinkImpl.from(webContents);
    }

    /**
     * @see View#onAttachedToWindow()
     */
    void onAttachedToWindow();

    /**
     * @see View#onDetachedFromWindow()
     */
    void onDetachedFromWindow();

    /**
     * @see View#onWindowFocusChanged(boolean)
     */
    void onWindowFocusChanged(boolean hasWindowFocus);

    /**
     * Called when view-level focus for the container view has changed.
     * @param gainFocus {@code true} if the focus is gained, otherwise {@code false}.
     */
    void onViewFocusChanged(boolean gainFocus);

    /**
     * Sets whether the keyboard should be hidden when losing input focus.
     * @param hideKeyboardOnBlur {@code true} if we should hide soft keyboard when losing focus.
     */
    void setHideKeyboardOnBlur(boolean hideKeyboardOnBlur);

    /**
     * @see View#onConfigurationChanged(Configuration)
     */
    void onConfigurationChanged(Configuration newConfig);

    /**
     * Set the Container view Internals.
     * @param internalDispatcher Handles dispatching all hidden or super methods to the
     *                           containerView.
     */
    void setAccessDelegate(InternalAccessDelegate internalDispatcher);

    void onPauseForTesting();

    void onResumeForTesting();
}
