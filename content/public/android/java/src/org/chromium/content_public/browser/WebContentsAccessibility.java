// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.content_public.browser;

import android.view.MotionEvent;
import android.view.View;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityNodeProvider;

import androidx.annotation.VisibleForTesting;

import org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl;

/**
 * Interface providing native accessibility for a {@link WebContents}. Actual native
 * accessibility part is lazily created upon the first request from Android framework on
 *{@link AccessibilityNodeProvider}, and shares the lifetime with {@link WebContents}.
 */
public interface WebContentsAccessibility {
    /**
     * @param webContents {@link WebContents} object.
     * @return {@link WebContentsAccessibility} object used for the give WebContents.
     *         {@code null} if not available.
     */
    static WebContentsAccessibility fromWebContents(WebContents webContents) {
        return WebContentsAccessibilityImpl.fromWebContents(webContents);
    }

    /**
     *  Determines if a11y enabled.
     *  @return {@code true} if a11y is enabled.
     */
    boolean isAccessibilityEnabled();

    /**
     *  Enables a11y for testing.
     */
    @VisibleForTesting
    void setAccessibilityEnabledForTesting();

    /**
     * Enables a11y service mask flags in the BrowserAccessibilityState for testing.
     */
    @VisibleForTesting
    void setBrowserAccessibilityStateForTesting();

    /**
     *  Add a spelling error.
     */
    @VisibleForTesting
    void addSpellingErrorForTesting(int virtualViewId, int startOffset, int endOffset);

    /**
     * If native accessibility is enabled and no other views are temporarily
     * obscuring this one, returns an AccessibilityNodeProvider that
     * implements native accessibility for this view. Returns null otherwise.
     * Lazily initializes native accessibility here if it's allowed.
     * @return The AccessibilityNodeProvider, if available, or null otherwise.
     */
    AccessibilityNodeProvider getAccessibilityNodeProvider();

    /**
     * @see View#onProvideVirtualStructure().
     */
    void onProvideVirtualStructure(ViewStructure structure, boolean ignoreScrollOffset);

    /**
     * Set whether or not the web contents are obscured by another view.
     * If true, we won't return an accessibility node provider or respond
     * to touch exploration events.
     */
    void setObscuredByAnotherView(boolean isObscured);

    /**
     * Sets whether or not we should set accessibility focus on page load.
     * This only applies if an accessibility service like TalkBack is running.
     * This is desirable behavior for a browser window, but not for an embedded
     * WebView.
     */
    void setShouldFocusOnPageLoad(boolean on);

    /**
     * Sets whether or not this instance is a candidate for the image descriptions feature to be
     * enabled. This feature is dependent on embedder behavior and screen reader state.
     * See BrowserAccessibilityState.java.
     */
    void setIsImageDescriptionsCandidate(boolean isImageDescriptionsCandidate);

    /**
     * Called when autofill popup is displayed. Used to upport navigation through the view.
     * @param autofillPopupView The displayed autofill popup view.
     */
    void onAutofillPopupDisplayed(View autofillPopupView);

    /**
     * Called when autofill popup is dismissed.
     */
    void onAutofillPopupDismissed();

    /**
     * Called when the a11y focus gets cleared on the autofill popup.
     */
    void onAutofillPopupAccessibilityFocusCleared();

    /**
     * Called directly from A {@link View} in the absence of a WebView and renderer.
     * @return Whether the hover event was consumed.
     */
    boolean onHoverEventNoRenderer(MotionEvent event);

    /**
     * Called to reset focus state to nothing.
     */
    void resetFocus();

    /**
     * Set the a11y focus to the DOM element that had it just before the focus
     * gets out WebContents, e.g. by focusing a native view node.
     */
    void restoreFocus();
}
