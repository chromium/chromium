// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.graphics.Bitmap;

/**
 * Represents one entry in the navigation history of a page.
 */
public class NavigationEntry {

    private final int mIndex;
    private final String mUrl;
    private final String mOriginalUrl;
    private final String mVirtualUrl;
    private final String mReferrerUrl;
    private final String mTitle;
    private Bitmap mFavicon;
    private int mTransition;
    private long mTimestamp;

    /**
     * Default constructor.
     */
    public NavigationEntry(int index, String url, String virtualUrl, String originalUrl,
            String referrerUrl, String title, Bitmap favicon, int transition, long timestamp) {
        mIndex = index;
        mUrl = url;
        mVirtualUrl = virtualUrl;
        mOriginalUrl = originalUrl;
        mReferrerUrl = referrerUrl;
        mTitle = title;
        mFavicon = favicon;
        mTransition = transition;
        mTimestamp = timestamp;
    }

    /**
     * @return The index into the navigation history that this entry represents.
     */
    public int getIndex() {
        return mIndex;
    }

    /**
     * @return The actual URL of the page. For some about pages, this may be a
     *         scary data: URL or something like that. Use GetVirtualURL() for
     *         showing to the user.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * @return The virtual URL, when nonempty, will override the actual URL of
     *         the page when we display it to the user. This allows us to have
     *         nice and friendly URLs that the user sees for things like about:
     *         URLs, but actually feed the renderer a data URL that results in
     *         the content loading.
     *         <p/>
     *         GetVirtualURL() will return the URL to display to the user in all
     *         cases, so if there is no overridden display URL, it will return
     *         the actual one.
     */
    public String getVirtualUrl() {
        return mVirtualUrl;
    }

    /**
     * @return The URL that caused this NavigationEntry to be created.
     */
    public String getOriginalUrl() {
        return mOriginalUrl;
    }

    /**
     * @return The referring URL, can be empty.
     */
    public String getReferrerUrl() {
        return mReferrerUrl;
    }

    /**
     * @return The title as set by the page. This will be empty if there is no
     *         title set. The caller is responsible for detecting when there is
     *         no title and displaying the appropriate "Untitled" label if this
     *         is being displayed to the user.
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @return The favicon of the page. This may be null.
     */
    public Bitmap getFavicon() {
        return mFavicon;
    }

    /**
     * @param favicon The updated favicon to replace the existing one with.
     */
    public void updateFavicon(Bitmap favicon) {
        mFavicon = favicon;
    }

    public int getTransition() {
        return mTransition;
    }

    /**
     * @return The Timestamp when the last navigation completed.
     */
    public long getTimestamp() {
        return mTimestamp;
    }
}
