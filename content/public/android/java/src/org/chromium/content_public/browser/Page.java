// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.AnyThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/**
 * JNI bridge with content::Page
 *
 * <p>All methods should be called on the UI thread with exception to getMostRecentPageState
 */
@NullMarked
public interface Page {
    /** Listener for when the native C++ Page object is destructed. */
    interface PageDeletionListener {
        void onWillDeletePage(Page page);
    }

    /**
     * Sets a listener to be notified when the native C++ Page object is destructed.
     *
     * @param listener The listener to set.
     */
    void setPageDeletionListener(PageDeletionListener listener);

    /**
     * @return Whether the page is currently prerendering.
     */
    boolean isPrerendering();

    /**
     * Sets whether the page is currently prerendering.
     *
     * @param isPrerendering Whether the page is prerendering.
     */
    void setIsPrerendering(boolean isPrerendering);

    /**
     * @return The URL of the page.
     */
    GURL getUrl();

    /**
     * Sets the URL of the page.
     *
     * @param url The URL to set.
     */
    void setUrl(GURL url);

    /** Returns the most recent {@link PageState} */
    @AnyThread
    PageState getMostRecentPageState();
}
