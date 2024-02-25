// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.mock;

import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.NavigationHistory;

/** Mock NavigationController implementation for Test. */
public class MockNavigationController implements NavigationController {
    @Override
    public boolean canGoBack() {
        return false;
    }

    @Override
    public boolean canGoForward() {
        return false;
    }

    @Override
    public boolean canGoToOffset(int offset) {
        return false;
    }

    @Override
    public void goToOffset(int offset) {}

    @Override
    public void goBack() {}

    @Override
    public void goForward() {}

    @Override
    public boolean isInitialNavigation() {
        return false;
    }

    @Override
    public void loadIfNecessary() {}

    @Override
    public boolean needsReload() {
        return false;
    }

    @Override
    public void setNeedsReload() {}

    @Override
    public void reload(boolean checkForRepost) {}

    @Override
    public void reloadBypassingCache(boolean checkForRepost) {}

    @Override
    public void cancelPendingReload() {}

    @Override
    public void continuePendingReload() {}

    @Override
    public NavigationHandle loadUrl(LoadUrlParams params) {
        return null;
    }

    @Override
    public void clearHistory() {}

    @Override
    public NavigationHistory getNavigationHistory() {
        return null;
    }

    @Override
    public void clearSslPreferences() {}

    @Override
    public boolean getUseDesktopUserAgent() {
        return false;
    }

    @Override
    public void setUseDesktopUserAgent(boolean override, boolean reloadOnChange, int caller) {}

    @Override
    public NavigationEntry getEntryAtIndex(int index) {
        return null;
    }

    @Override
    public NavigationEntry getVisibleEntry() {
        return null;
    }

    @Override
    public NavigationEntry getPendingEntry() {
        return null;
    }

    @Override
    public NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit) {
        return null;
    }

    @Override
    public void goToNavigationIndex(int index) {}

    @Override
    public int getLastCommittedEntryIndex() {
        return -1;
    }

    @Override
    public boolean removeEntryAtIndex(int index) {
        return false;
    }

    @Override
    public void pruneForwardEntries() {}

    @Override
    public String getEntryExtraData(int index, String key) {
        return null;
    }

    @Override
    public void setEntryExtraData(int index, String key, String value) {}
}
