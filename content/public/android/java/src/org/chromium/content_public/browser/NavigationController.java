// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * The NavigationController Java wrapper to allow communicating with the native
 * NavigationController object.
 */
@NullMarked
public interface NavigationController {
    /**
     * @return Whether back navigation is possible from the "current entry".
     */
    boolean canGoBack();

    /**
     * @return Whether forward navigation is possible from the "current entry".
     */
    boolean canGoForward();

    /**
     * @param offset The offset into the navigation history.
     * @return Whether we can move in history by given offset
     */
    boolean canGoToOffset(int offset);

    /**
     * Navigates to the specified offset from the "current entry". Does nothing if the offset is
     * out of bounds.
     * @param offset The offset into the navigation history.
     */
    void goToOffset(int offset);

    /**
     * Navigates to the specified index in the navigation entry for this page.
     * @param index The navigation index to navigate to.
     */
    void goToNavigationIndex(int index);

    /** Goes to the first non-skippable navigation entry before the current. */
    void goBack();

    /** Goes to the first non-skippable navigation entry following the current. */
    void goForward();

    /**
     * @return Whether the tab is navigating to the URL the tab is opened with.
     */
    boolean isInitialNavigation();

    /** Loads the current navigation if there is a pending lazy load (after tab restore). */
    void loadIfNecessary();

    /**
     * @return Whether a reload has been requested.
     */
    boolean needsReload();

    /** Requests the current navigation to be loaded upon the next call to loadIfNecessary(). */
    void setNeedsReload();

    /** Reload the current page. */
    void reload(boolean checkForRepost);

    /** Reload the current page, bypassing the contents of the cache. */
    void reloadBypassingCache(boolean checkForRepost);

    /** Cancel the pending reload. */
    void cancelPendingReload();

    /** Continue the pending reload. */
    void continuePendingReload();

    /**
     * Load url without fixing up the url string. Consumers of NavigationController are responsible
     * for ensuring the URL passed in is properly formatted (i.e. the scheme has been added if left
     * off during user input).
     *
     * @param params Parameters for this load.
     * @return NavigationHandle for the initiated navigation (might be null if the navigation
     *     couldn't be started for some reason).
     */
    @Nullable NavigationHandle loadUrl(LoadUrlParams params);

    /** Clears NavigationController's page history in both backwards and forwards directions. */
    void clearHistory();

    /**
     * Get a copy of the navigation history of NavigationController.
     *
     * @return navigation history of NavigationController.
     */
    @Nullable NavigationHistory getNavigationHistory();

    /**
     * Get the navigation history of NavigationController from current navigation entry index with
     * direction (forward/backward)
     *
     * @param isForward determines forward or backward from current index
     * @param itemLimit maximum number of entries to be retrieved in specified diection.
     * @return navigation history by keeping above constraints.
     */
    @Nullable NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit);

    /** Clears SSL preferences for this NavigationController. */
    void clearSslPreferences();

    /**
     * Get whether or not we're using a desktop user agent for the currently loaded page.
     *
     * @return true, if use a desktop user agent and false for a mobile one.
     */
    boolean getUseDesktopUserAgent();

    /**
     * Set whether or not we're using a desktop user agent for the currently loaded page.
     *
     * @param override If true, use a desktop user agent. Use a mobile one otherwise.
     * @param reloadOnChange Reload the page if the UA has changed.
     * @param skipOnInitialNavigation If true, don't override the UA for the initial navigation.
     *     (TODO crbug.com/40063185: Remove this parameter once the bug is fixed.)
     */
    void setUseDesktopUserAgent(
            boolean override, boolean reloadOnChange, boolean skipOnInitialNavigation);

    /**
     * Return the NavigationEntry at the given index.
     *
     * @param index Index to retrieve the NavigationEntry for.
     * @return Entry containing info about the navigation, null if the index is out of bounds.
     */
    @Nullable NavigationEntry getEntryAtIndex(int index);

    /**
     * @return The {@link NavigationEntry} that is appropriate to be displayed in the address bar.
     */
    @Nullable NavigationEntry getVisibleEntry();

    /**
     * @return The pending {@link NavigationEntry} for this controller or {@code null} if none
     *     exists.
     */
    @Nullable NavigationEntry getPendingEntry();

    /**
     * @return The index of the last committed entry.
     */
    int getLastCommittedEntryIndex();

    /**
     * @return true, if the source for the current entry can be viewed.
     */
    boolean canViewSource();

    /**
     * Removes the entry at the specified |index|.
     *
     * @return false, if the index is the last committed index or the pending entry. Otherwise this
     *     call discards any transient or pending entries.
     */
    boolean removeEntryAtIndex(int index);

    /**
     * Discards any transient or pending entries, then discards all entries after the current entry
     * index.
     */
    void pruneForwardEntries();

    /**
     * Gets extra data on the {@link NavigationEntry} at {@code index}.
     * @param index The index of the navigation entry.
     * @param key The data key.
     * @return The data value, or null if not found.
     */
    @Nullable
    String getEntryExtraData(int index, String key);

    /**
     * Sets extra data on the {@link NavigationEntry} at {@code index}.
     *
     * @param index The index of the navigation entry.
     * @param key The data key.
     * @param value The data value.
     */
    void setEntryExtraData(int index, String key, String value);

    /**
     * Copies the navigation controller state from {@param sourceNavigationController} to this.
     *
     * @param sourceNavigationController The {@link NavigationController} to copy from.
     */
    void copyStateFrom(NavigationController sourceNavigationController);
}
