// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.framehost;

import android.graphics.Bitmap;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.AdditionalNavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/**
 * The NavigationControllerImpl Java wrapper to allow communicating with the native
 * NavigationControllerImpl object.
 */
@JNINamespace("content")
// TODO(tedchoc): Remove the package restriction once this class moves to a non-public content
//                package whose visibility will be enforced via DEPS.
@NullMarked
/* package */ class NavigationControllerImpl implements NavigationController {
    private static final String TAG = "NavigationController";

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
                && NavigationControllerImplJni.get().canGoBack(mNativeNavigationControllerAndroid);
    }

    @Override
    public boolean canGoForward() {
        return mNativeNavigationControllerAndroid != 0
                && NavigationControllerImplJni.get()
                        .canGoForward(mNativeNavigationControllerAndroid);
    }

    @Override
    @VisibleForTesting
    public boolean canGoToOffset(int offset) {
        return mNativeNavigationControllerAndroid != 0
                && NavigationControllerImplJni.get()
                        .canGoToOffset(mNativeNavigationControllerAndroid, offset);
    }

    @Override
    public void goToOffset(int offset) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get()
                    .goToOffset(mNativeNavigationControllerAndroid, offset);
        }
    }

    @Override
    public void goToNavigationIndex(int index) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get()
                    .goToNavigationIndex(mNativeNavigationControllerAndroid, index);
        }
    }

    @Override
    public void goBack() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().goBack(mNativeNavigationControllerAndroid);
        }
    }

    @Override
    public void goForward() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().goForward(mNativeNavigationControllerAndroid);
        }
    }

    @Override
    public boolean isInitialNavigation() {
        return mNativeNavigationControllerAndroid != 0
                && NavigationControllerImplJni.get()
                        .isInitialNavigation(mNativeNavigationControllerAndroid);
    }

    @Override
    public void loadIfNecessary() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().loadIfNecessary(mNativeNavigationControllerAndroid);
        }
    }

    @Override
    public boolean needsReload() {
        return mNativeNavigationControllerAndroid != 0
                && NavigationControllerImplJni.get()
                        .needsReload(mNativeNavigationControllerAndroid);
    }

    @Override
    public void setNeedsReload() {
        NavigationControllerImplJni.get().setNeedsReload(mNativeNavigationControllerAndroid);
    }

    @Override
    public void reload(boolean checkForRepost) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get()
                    .reload(mNativeNavigationControllerAndroid, checkForRepost);
        }
    }

    @Override
    public void reloadBypassingCache(boolean checkForRepost) {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get()
                    .reloadBypassingCache(mNativeNavigationControllerAndroid, checkForRepost);
        }
    }

    @Override
    public void cancelPendingReload() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get()
                    .cancelPendingReload(mNativeNavigationControllerAndroid);
        }
    }

    @Override
    public void continuePendingReload() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get()
                    .continuePendingReload(mNativeNavigationControllerAndroid);
        }
    }

    @Override
    public @Nullable NavigationHandle loadUrl(LoadUrlParams params) {
        NavigationHandle navigationHandle = null;
        if (mNativeNavigationControllerAndroid != 0) {
            String headers =
                    params.getExtraHeaders() == null
                            ? params.getVerbatimHeaders()
                            : params.getExtraHeadersString();
            long inputStart =
                    params.getInputStartTimestamp() == 0
                            ? params.getIntentReceivedTimestamp()
                            : params.getInputStartTimestamp();
            RecordHistogram.recordTimesHistogram(
                    "Android.Omnibox.InputToNavigationControllerStart",
                    SystemClock.uptimeMillis() - inputStart);
            navigationHandle =
                    NavigationControllerImplJni.get()
                            .loadUrl(
                                    mNativeNavigationControllerAndroid,
                                    params.getUrl(),
                                    params.getLoadUrlType(),
                                    params.getTransitionType(),
                                    params.getReferrer() != null
                                            ? params.getReferrer().getUrl()
                                            : null,
                                    params.getReferrer() != null
                                            ? params.getReferrer().getPolicy()
                                            : 0,
                                    params.getUserAgentOverrideOption(),
                                    headers,
                                    params.getPostData(),
                                    params.getBaseUrl(),
                                    params.getVirtualUrlForSpecialCases(),
                                    params.getDataUrlAsString(),
                                    params.getCanLoadLocalResources(),
                                    params.getIsRendererInitiated(),
                                    params.getShouldReplaceCurrentEntry(),
                                    params.getInitiatorOrigin(),
                                    params.getHasUserGesture(),
                                    params.getShouldClearHistoryList(),
                                    params.getAdditionalNavigationParams(),
                                    inputStart,
                                    params.getNavigationUIDataSupplier() == null
                                            ? 0
                                            : params.getNavigationUIDataSupplier().get(),
                                    params.getIsPdf());
            // Use the navigation handle object to store user data passed in.
            if (navigationHandle != null) {
                navigationHandle.setUserDataHost(params.takeNavigationHandleUserData());
            }
        }
        return navigationHandle;
    }

    @Override
    public void clearHistory() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get().clearHistory(mNativeNavigationControllerAndroid);
        }
    }

    @Override
    public @Nullable NavigationHistory getNavigationHistory() {
        if (mNativeNavigationControllerAndroid == 0) return null;
        NavigationHistory history = new NavigationHistory();
        int currentIndex =
                NavigationControllerImplJni.get()
                        .getNavigationHistory(mNativeNavigationControllerAndroid, history);

        history.setCurrentEntryIndex(currentIndex);
        return history;
    }

    @Override
    public @Nullable NavigationHistory getDirectedNavigationHistory(
            boolean isForward, int itemLimit) {
        if (mNativeNavigationControllerAndroid == 0) return null;
        NavigationHistory history = new NavigationHistory();
        NavigationControllerImplJni.get()
                .getDirectedNavigationHistory(
                        mNativeNavigationControllerAndroid, history, isForward, itemLimit);
        return history;
    }

    @Override
    public void clearSslPreferences() {
        if (mNativeNavigationControllerAndroid != 0) {
            NavigationControllerImplJni.get()
                    .clearSslPreferences(mNativeNavigationControllerAndroid);
        }
    }

    @Override
    public boolean getUseDesktopUserAgent() {
        if (mNativeNavigationControllerAndroid == 0) return false;
        return NavigationControllerImplJni.get()
                .getUseDesktopUserAgent(mNativeNavigationControllerAndroid);
    }

    @Override
    public void setUseDesktopUserAgent(
            boolean override, boolean reloadOnChange, boolean skipOnInitialNavigation) {
        if (mNativeNavigationControllerAndroid != 0) {
            Log.i(
                    TAG,
                    "Thread dump for debugging, override: "
                            + override
                            + " reloadOnChange: "
                            + reloadOnChange);
            Thread.dumpStack();

            NavigationControllerImplJni.get()
                    .setUseDesktopUserAgent(
                            mNativeNavigationControllerAndroid,
                            override,
                            reloadOnChange,
                            skipOnInitialNavigation);
        }
    }

    @Override
    public @Nullable NavigationEntry getEntryAtIndex(int index) {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get()
                    .getEntryAtIndex(mNativeNavigationControllerAndroid, index);
        }

        return null;
    }

    @Override
    public @Nullable NavigationEntry getVisibleEntry() {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get()
                    .getVisibleEntry(mNativeNavigationControllerAndroid);
        }

        return null;
    }

    @Override
    public @Nullable NavigationEntry getPendingEntry() {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get()
                    .getPendingEntry(mNativeNavigationControllerAndroid);
        }

        return null;
    }

    @Override
    public int getLastCommittedEntryIndex() {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get()
                    .getLastCommittedEntryIndex(mNativeNavigationControllerAndroid);
        }
        return -1;
    }

    @Override
    public boolean canViewSource() {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get()
                    .canViewSource(mNativeNavigationControllerAndroid);
        }
        return false;
    }

    @Override
    public boolean removeEntryAtIndex(int index) {
        if (mNativeNavigationControllerAndroid != 0) {
            return NavigationControllerImplJni.get()
                    .removeEntryAtIndex(mNativeNavigationControllerAndroid, index);
        }
        return false;
    }

    @Override
    public void pruneForwardEntries() {
        if (mNativeNavigationControllerAndroid == 0) return;
        NavigationControllerImplJni.get().pruneForwardEntries(mNativeNavigationControllerAndroid);
    }

    @Override
    public @Nullable String getEntryExtraData(int index, String key) {
        if (mNativeNavigationControllerAndroid == 0) return null;
        return NavigationControllerImplJni.get()
                .getEntryExtraData(mNativeNavigationControllerAndroid, index, key);
    }

    @Override
    public void setEntryExtraData(int index, String key, String value) {
        if (mNativeNavigationControllerAndroid == 0) return;
        NavigationControllerImplJni.get()
                .setEntryExtraData(mNativeNavigationControllerAndroid, index, key, value);
    }

    @Override
    public void copyStateFrom(NavigationController sourceNavigationController) {
        if (mNativeNavigationControllerAndroid == 0) return;
        NavigationControllerImpl sourceImpl = (NavigationControllerImpl) sourceNavigationController;
        if (sourceImpl.mNativeNavigationControllerAndroid == 0) return;
        NavigationControllerImplJni.get()
                .copyStateFrom(
                        mNativeNavigationControllerAndroid,
                        sourceImpl.mNativeNavigationControllerAndroid,
                        /* needsReload= */ true);
    }

    @CalledByNative
    private static void addToNavigationHistory(Object history, Object navigationEntry) {
        ((NavigationHistory) history).addEntry((NavigationEntry) navigationEntry);
    }

    @CalledByNative
    private static NavigationEntry createNavigationEntry(
            int index,
            GURL url,
            GURL virtualUrl,
            GURL originalUrl,
            String title,
            Bitmap favicon,
            int transition,
            long timestamp,
            boolean isInitialEntry) {
        return new NavigationEntry(
                index,
                url,
                virtualUrl,
                originalUrl,
                title,
                favicon,
                transition,
                timestamp,
                isInitialEntry);
    }

    @NativeMethods
    interface Natives {
        boolean canGoBack(long nativeNavigationControllerAndroid);

        boolean canGoForward(long nativeNavigationControllerAndroid);

        boolean isInitialNavigation(long nativeNavigationControllerAndroid);

        void loadIfNecessary(long nativeNavigationControllerAndroid);

        boolean needsReload(long nativeNavigationControllerAndroid);

        void setNeedsReload(long nativeNavigationControllerAndroid);

        boolean canGoToOffset(long nativeNavigationControllerAndroid, int offset);

        void goBack(long nativeNavigationControllerAndroid);

        void goForward(long nativeNavigationControllerAndroid);

        void goToOffset(long nativeNavigationControllerAndroid, int offset);

        void goToNavigationIndex(long nativeNavigationControllerAndroid, int index);

        void cancelPendingReload(long nativeNavigationControllerAndroid);

        void continuePendingReload(long nativeNavigationControllerAndroid);

        void reload(long nativeNavigationControllerAndroid, boolean checkForRepost);

        void reloadBypassingCache(long nativeNavigationControllerAndroid, boolean checkForRepost);

        NavigationHandle loadUrl(
                long nativeNavigationControllerAndroid,
                String url,
                int loadUrlType,
                int transitionType,
                @Nullable String referrerUrl,
                int referrerPolicy,
                int uaOverrideOption,
                @Nullable String extraHeaders,
                @Nullable ResourceRequestBody postData,
                @Nullable String baseUrlForDataUrl,
                @Nullable String virtualUrlForSpecialCases,
                @Nullable String dataUrlAsString,
                boolean canLoadLocalResources,
                boolean isRendererInitiated,
                boolean shouldReplaceCurrentEntry,
                @Nullable Origin initiatorOrigin,
                boolean hasUserGesture,
                boolean shouldClearHistoryList,
                @Nullable AdditionalNavigationParams additionalNavigationParams,
                long inputStart,
                long navigationUIDataPtr,
                boolean isPdf);

        void clearHistory(long nativeNavigationControllerAndroid);

        int getNavigationHistory(long nativeNavigationControllerAndroid, Object history);

        void getDirectedNavigationHistory(
                long nativeNavigationControllerAndroid,
                NavigationHistory history,
                boolean isForward,
                int itemLimit);

        void clearSslPreferences(long nativeNavigationControllerAndroid);

        boolean getUseDesktopUserAgent(long nativeNavigationControllerAndroid);

        void setUseDesktopUserAgent(
                long nativeNavigationControllerAndroid,
                boolean override,
                boolean reloadOnChange,
                boolean skipOnInitialNavigation);

        NavigationEntry getEntryAtIndex(long nativeNavigationControllerAndroid, int index);

        NavigationEntry getVisibleEntry(long nativeNavigationControllerAndroid);

        NavigationEntry getPendingEntry(long nativeNavigationControllerAndroid);

        int getLastCommittedEntryIndex(long nativeNavigationControllerAndroid);

        boolean canViewSource(long nativeNavigationControllerAndroid);

        boolean removeEntryAtIndex(long nativeNavigationControllerAndroid, int index);

        void pruneForwardEntries(long nativeNavigationControllerAndroid);

        String getEntryExtraData(long nativeNavigationControllerAndroid, int index, String key);

        void setEntryExtraData(
                long nativeNavigationControllerAndroid, int index, String key, String value);

        void copyStateFrom(
                long nativeNavigationControllerAndroid,
                long nativeSourceNavigationControllerAndroid,
                boolean needsReload);
    }
}
