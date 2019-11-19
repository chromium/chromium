// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.framehost;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.common.ResourceRequestBody;

/**
 * The NavigationControllerImpl Java wrapper to allow communicating with the native
 * NavigationControllerImpl object.
 */
@JNINamespace("content")
// TODO(tedchoc): Remove the package restriction once this class moves to a non-public content
//                package whose visibility will be enforced via DEPS.
/* package */ class NavigationControllerImpl implements NavigationController {

    private long mNativeNavigationControllerAndroid;

    private NavigationControllerImpl(long nativeNavigationControllerAndroid) {
        mNativeNavigationControllerAndroid = nativeNavigationControllerAndroid;
    }

    @CalledByNative
    private static NavigationControllerImpl create(long nativeNavigationControllerAndroid) {
        return new NavigationControllerImpl(nativeNavigationControllerAndroid);
    }

    @CalledByNative
    private void destroy() {
        mNativeNavigationControllerAndroid = 0;
    }

    @Override
    public boolean canGoBack() {
        return mNativeNavigationControllerAndroid != 0
                && NavigationControllerImplJni.get().canGoBack(
                        mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
    }

    @Override
    public boolean canGoForward() {
        return mNativeNavigationControllerAndroid != 0
                && NavigationControllerImplJni.get().canGoForward(
                        mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
    }

    @Override
    @VisibleForTesting
    public boolean canGoToOffset(int offset) {
        return mNativeNavigationControllerAndroid != 0
                && NavigationControllerImplJni.get().canGoToOffset(
                        mNativeNavigationControllerAndroid, NavigationControllerImpl.this, offset);
    }

    @Override
    public void goToOffset(int offset) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().goToOffset(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this, offset);
        }
    }

    @Override
    public void goToNavigationIndex(int index) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().goToNavigationIndex(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this, index);
        }
    }

    @Override
    public void goBack() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().goBack(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }
    }

    @Override
    public void goForward() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().goForward(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }
    }

    @Override
    public boolean isInitialNavigation() {
        return mNativeNavigationControllerAndroid != 0
                && NavigationControllerImplJni.get().isInitialNavigation(
                        mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
    }

    @Override
    public void loadIfNecessary() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().loadIfNecessary(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }
    }

    @Override
    public boolean needsReload() {
        return mNativeNavigationControllerAndroid != 0
                && NavigationControllerImplJni.get().needsReload(
                        mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
    }

    @Override
    public void setNeedsReload() {
        NavigationControllerImplJni.get().setNeedsReload(
                mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
    }

    @Override
    public void reload(boolean checkForRepost) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().reload(mNativeNavigationControllerAndroid,
                    NavigationControllerImpl.this, checkForRepost);
        }
    }

    @Override
    public void reloadBypassingCache(boolean checkForRepost) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().reloadBypassingCache(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this,
                    checkForRepost);
        }
    }

    @Override
    public void cancelPendingReload() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().cancelPendingReload(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }
    }

    @Override
    public void continuePendingReload() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().continuePendingReload(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }
    }

    @Override
    public void loadUrl(LoadUrlParams params) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().loadUrl(mNativeNavigationControllerAndroid,
                    NavigationControllerImpl.this, params.getUrl(), params.getLoadUrlType(),
                    params.getTransitionType(),
                    params.getReferrer() != null ? params.getReferrer().getUrl() : null,
                    params.getReferrer() != null ? params.getReferrer().getPolicy() : 0,
                    params.getUserAgentOverrideOption(), params.getExtraHeadersString(),
                    params.getPostData(), params.getBaseUrl(), params.getVirtualUrlForDataUrl(),
                    params.getDataUrlAsString(), params.getCanLoadLocalResources(),
                    params.getIsRendererInitiated(), params.getShouldReplaceCurrentEntry());
        }
    }

    @Override
    public void clearHistory() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().clearHistory(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }
    }

    @Override
    public NavigationHistory getNavigationHistory() {
        if (mNativeNavigationControllerAndroid == 0) return null;
        NavigationHistory history = new NavigationHistory();
        int currentIndex = NavigationControllerImplJni.get().getNavigationHistory(
                mNativeNavigationControllerAndroid, NavigationControllerImpl.this, history);
        history.setCurrentEntryIndex(currentIndex);
        return history;
    }

    @Override
    public NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit) {
        if (mNativeNavigationControllerAndroid == 0) return null;
        NavigationHistory history = new NavigationHistory();
        NavigationControllerImplJni.get().getDirectedNavigationHistory(
                mNativeNavigationControllerAndroid, NavigationControllerImpl.this, history,
                isForward, itemLimit);
        return history;
    }

    @Override
    public void clearSslPreferences() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().clearSslPreferences(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }
    }

    @Override
    public boolean getUseDesktopUserAgent() {
        if (mNativeNavigationControllerAndroid == 0) return false;
        return NavigationControllerImplJni.get().getUseDesktopUserAgent(
                mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
    }

    @Override
    public void setUseDesktopUserAgent(boolean override, boolean reloadOnChange) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().setUseDesktopUserAgent(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this, override,
                    reloadOnChange);
        }
    }

    @Override
    public NavigationEntry getEntryAtIndex(int index) {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get().getEntryAtIndex(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this, index);
        }

        return null;
    }

    @Override
    public NavigationEntry getVisibleEntry() {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get().getVisibleEntry(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }

        return null;
    }

    @Override
    public NavigationEntry getPendingEntry() {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get().getPendingEntry(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }

        return null;
    }

    @Override
    public int getLastCommittedEntryIndex() {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get().getLastCommittedEntryIndex(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
        }
        return -1;
    }

    @Override
    public boolean removeEntryAtIndex(int index) {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get().removeEntryAtIndex(
                    mNativeNavigationControllerAndroid, NavigationControllerImpl.this, index);
        }
        return false;
    }

    @Override
    public void pruneForwardEntries() {
        if (mNativeNavigationControllerAndroid == 0) return;
        NavigationControllerImplJni.get().pruneForwardEntries(
                mNativeNavigationControllerAndroid, NavigationControllerImpl.this);
    }

    @Override
    public String getEntryExtraData(int index, String key) {
        if (mNativeNavigationControllerAndroid == 0) return null;
        return NavigationControllerImplJni.get().getEntryExtraData(
                mNativeNavigationControllerAndroid, NavigationControllerImpl.this, index, key);
    }

    @Override
    public void setEntryExtraData(int index, String key, String value) {
        if (mNativeNavigationControllerAndroid == 0) return;
        NavigationControllerImplJni.get().setEntryExtraData(mNativeNavigationControllerAndroid,
                NavigationControllerImpl.this, index, key, value);
    }

    @Override
    public boolean isEntryMarkedToBeSkipped(int index) {
        if (mNativeNavigationControllerAndroid == 0) return false;
        return NavigationControllerImplJni.get().isEntryMarkedToBeSkipped(
                mNativeNavigationControllerAndroid, NavigationControllerImpl.this, index);
    }

    @CalledByNative
    private static void addToNavigationHistory(Object history, Object navigationEntry) {
        ((NavigationHistory) history).addEntry((NavigationEntry) navigationEntry);
    }

    @CalledByNative
    private static NavigationEntry createNavigationEntry(int index, String url, String virtualUrl,
            String originalUrl, String referrerUrl, String title, Bitmap favicon, int transition,
            long timestamp) {
        return new NavigationEntry(index, url, virtualUrl, originalUrl, referrerUrl, title, favicon,
                transition, timestamp);
    }

    @NativeMethods
    interface Natives {
        boolean canGoBack(long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        boolean canGoForward(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        boolean isInitialNavigation(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        void loadIfNecessary(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        boolean needsReload(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        void setNeedsReload(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        boolean canGoToOffset(long nativeNavigationControllerAndroid,
                NavigationControllerImpl caller, int offset);
        void goBack(long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        void goForward(long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        void goToOffset(long nativeNavigationControllerAndroid, NavigationControllerImpl caller,
                int offset);
        void goToNavigationIndex(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller, int index);
        void cancelPendingReload(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        void continuePendingReload(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        void reload(long nativeNavigationControllerAndroid, NavigationControllerImpl caller,
                boolean checkForRepost);
        void reloadBypassingCache(long nativeNavigationControllerAndroid,
                NavigationControllerImpl caller, boolean checkForRepost);
        void loadUrl(long nativeNavigationControllerAndroid, NavigationControllerImpl caller,
                String url, int loadUrlType, int transitionType, String referrerUrl,
                int referrerPolicy, int uaOverrideOption, String extraHeaders,
                ResourceRequestBody postData, String baseUrlForDataUrl, String virtualUrlForDataUrl,
                String dataUrlAsString, boolean canLoadLocalResources, boolean isRendererInitiated,
                boolean shouldReplaceCurrentEntry);
        void clearHistory(long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        int getNavigationHistory(long nativeNavigationControllerAndroid,
                NavigationControllerImpl caller, Object history);
        void getDirectedNavigationHistory(long nativeNavigationControllerAndroid,
                NavigationControllerImpl caller, NavigationHistory history, boolean isForward,
                int itemLimit);
        void clearSslPreferences(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        boolean getUseDesktopUserAgent(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        void setUseDesktopUserAgent(long nativeNavigationControllerAndroid,
                NavigationControllerImpl caller, boolean override, boolean reloadOnChange);
        NavigationEntry getEntryAtIndex(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller, int index);
        NavigationEntry getVisibleEntry(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        NavigationEntry getPendingEntry(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        int getLastCommittedEntryIndex(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        boolean removeEntryAtIndex(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller, int index);
        void pruneForwardEntries(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller);
        String getEntryExtraData(long nativeNavigationControllerAndroid,
                NavigationControllerImpl caller, int index, String key);
        void setEntryExtraData(long nativeNavigationControllerAndroid,
                NavigationControllerImpl caller, int index, String key, String value);
        boolean isEntryMarkedToBeSkipped(
                long nativeNavigationControllerAndroid, NavigationControllerImpl caller, int index);
    }
}
