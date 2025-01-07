// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

// import org.chromium.content_public.browser.NavigationEntry;

import java.util.ArrayList;

/**
 * {@link NavigationHistory} captures a snapshot of the navigation history of a
 * {@link WebContents}. It is a copy and will not be updated as navigation
 * occurs on the source {@link WebContents}.
 */
public class NavigationHistory {

    private final ArrayList<NavigationEntry> mEntries = new ArrayList<NavigationEntry>();
    private int mCurrentEntryIndex;

    public void addEntry(NavigationEntry entry) {
        mEntries.add(entry);
    }

    public void setCurrentEntryIndex(int currentEntryIndex) {
        mCurrentEntryIndex = currentEntryIndex;
    }

    /**
     * @return The number of entries in the history.
     */
    public int getEntryCount() {
        return mEntries.size();
    }

    /** Returns the {@link NavigationEntry} for the given index. */
    public NavigationEntry getEntryAtIndex(int index) {
        return mEntries.get(index);
    }

    /**
     * Returns the index of the entry the {@link WebContents} was navigated to
     * when the history was fetched.
     */
    public int getCurrentEntryIndex() {
        return mCurrentEntryIndex;
    }
}
