// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.content_public.browser;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;
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
     * Determines whether or not the given accessibility action can be handled.
     * @param action The action to perform.
     * @return Whether or not this action is supported.
     */
    boolean supportsAction(int action);

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
     *  Add a spelling error.
     */
    @VisibleForTesting
    void addSpellingErrorForTesting(int virtualViewId, int startOffset, int endOffset);

    /**
     * Attempts to perform an accessibility action on the web content.  If the accessibility action
     * cannot be processed, it returns {@code null}, allowing the caller to know to call the
     * super {@link View#performAccessibilityAction(int, Bundle)} method and use that return value.
     * Otherwise the return value from this method should be used.
     * @param action The action to perform.
     * @param arguments Optional action arguments.
     * @return Whether the action was performed or {@code null} if the call should be delegated to
     *         the super {@link View} class.
     */
    boolean performAction(int action, Bundle arguments);

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
    @TargetApi(Build.VERSION_CODES.M)
    void onProvideVirtualStructure(ViewStructure structure, boolean ignoreScrollOffset);

    /**
     * Set whether or not the web contents are obscured by another view.
     * If true, we won't return an accessibility node provider or respond
     * to touch exploration events.
     */
    void setObscuredByAnotherView(boolean isObscured);

    /**
     * Returns true if accessibility is on and touch exploration is enabled.
     */
    boolean isTouchExplorationEnabled();

    /**
     * Turns browser accessibility on or off.
     * If |state| is |false|, this turns off both native and injected accessibility.
     * Otherwise, if accessibility script injection is enabled, this will enable the injected
     * accessibility scripts. Native accessibility is enabled on demand.
     */
    void setState(boolean state);

    /**
     * Sets whether or not we should set accessibility focus on page load.
     * This only applies if an accessibility service like TalkBack is running.
     * This is desirable behavior for a browser window, but not for an embedded
     * WebView.
     */
    void setShouldFocusOnPageLoad(boolean on);

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
}
