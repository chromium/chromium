// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

/** Builder for constructing {@link NavigationHandle} instances in tests. */
@NullMarked
public class NavigationHandleTestBuilder {
    private final GURL mUrl;
    private boolean mIsInPrimaryMainFrame = true;
    private boolean mIsSameDocument;
    private boolean mIsRendererInitiated;
    private @PageTransition int mTransition = PageTransition.LINK;
    private boolean mHasUserGesture;
    private boolean mIsReload;
    private boolean mIsHistory;
    private boolean mIsBack;
    private boolean mIsForward;
    private long mNavigationId;

    public NavigationHandleTestBuilder(GURL url) {
        mUrl = url;
    }

    public NavigationHandleTestBuilder setIsInPrimaryMainFrame(boolean isInPrimaryMainFrame) {
        mIsInPrimaryMainFrame = isInPrimaryMainFrame;
        return this;
    }

    public NavigationHandleTestBuilder setIsSameDocument(boolean isSameDocument) {
        mIsSameDocument = isSameDocument;
        return this;
    }

    public NavigationHandleTestBuilder setIsRendererInitiated(boolean isRendererInitiated) {
        mIsRendererInitiated = isRendererInitiated;
        return this;
    }

    public NavigationHandleTestBuilder setTransition(@PageTransition int transition) {
        mTransition = transition;
        return this;
    }

    public NavigationHandleTestBuilder setHasUserGesture(boolean hasUserGesture) {
        mHasUserGesture = hasUserGesture;
        return this;
    }

    public NavigationHandleTestBuilder setIsReload(boolean isReload) {
        mIsReload = isReload;
        return this;
    }

    public NavigationHandleTestBuilder setIsHistory(boolean isHistory) {
        mIsHistory = isHistory;
        return this;
    }

    public NavigationHandleTestBuilder setIsBack(boolean isBack) {
        mIsBack = isBack;
        if (isBack) mIsHistory = true;
        return this;
    }

    public NavigationHandleTestBuilder setIsForward(boolean isForward) {
        mIsForward = isForward;
        if (isForward) mIsHistory = true;
        return this;
    }

    public NavigationHandleTestBuilder setNavigationId(long navigationId) {
        mNavigationId = navigationId;
        return this;
    }

    public NavigationHandle build() {
        return NavigationHandle.createForTesting(
                mUrl,
                mIsInPrimaryMainFrame,
                mIsSameDocument,
                mIsRendererInitiated,
                mTransition,
                mHasUserGesture,
                mIsReload,
                mIsHistory,
                mIsBack,
                mIsForward,
                mNavigationId);
    }
}
