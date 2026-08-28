// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.AnyThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.net.NetError;
import org.chromium.url.GURL;

import java.util.Map;

/**
 * {@link NavigationState} is an incomplete copy of {@link NavigationHandle} at a certain point in
 * time, which occurs after every time after a value is set.
 */
@AnyThread
@NullMarked
public class NavigationState {

    private final GURL mUrl;
    private final boolean mIsRendererInitiated;
    private final boolean mIsSameDocument;
    private final boolean mIsReload;
    private final boolean mIsHistory;
    private final boolean mIsRestore;
    private final boolean mIsBack;
    private final boolean mIsForward;
    private final boolean mHasCommitted;
    private final boolean mIsErrorPage;
    private final int mHttpStatusCode;
    private final @NetError int mErrorCode;
    private final @Nullable String mErrorDescription;
    private final @Nullable Map<String, String> mResponseHeaders;

    private static final String TAG = "NavigationState";

    private final NavigationHandle mNavigationHandle;

    private final boolean mStarted;

    NavigationState(
            GURL url,
            boolean isRendererInitiated,
            boolean isSameDocument,
            boolean isReload,
            boolean isHistory,
            boolean isRestore,
            boolean isBack,
            boolean isForward,
            boolean hasCommitted,
            boolean isErrorPage,
            int httpStatusCode,
            @NetError int errorCode,
            @Nullable String errorDescription,
            @Nullable Map<String, String> responseHeaders,
            boolean started,
            NavigationHandle navigationHandle) {
        mUrl = url;
        mIsRendererInitiated = isRendererInitiated;
        mIsSameDocument = isSameDocument;
        mIsReload = isReload;
        mIsHistory = isHistory;
        mIsRestore = isRestore;
        mIsBack = isBack;
        mIsForward = isForward;
        mHasCommitted = hasCommitted;
        mIsErrorPage = isErrorPage;
        mHttpStatusCode = httpStatusCode;
        mErrorCode = errorCode;
        mErrorDescription = errorDescription;

        if (responseHeaders == null) {
            mResponseHeaders = null;
        } else {
            mResponseHeaders = Map.copyOf(responseHeaders);
        }

        mStarted = started;
        mNavigationHandle = navigationHandle;
    }

    public GURL getUrl() {
        assert mStarted;
        return mUrl;
    }

    public boolean isRendererInitiated() {
        return mIsRendererInitiated;
    }

    public @Nullable Map<String, String> getResponseHeaders() {
        return mResponseHeaders;
    }

    public boolean isSameDocument() {
        assert mStarted;
        return mIsSameDocument;
    }

    public @NetError int errorCode() {
        assert mStarted;
        return mErrorCode;
    }

    public @Nullable String errorDescription() {
        assert mStarted;
        return mErrorDescription;
    }

    public boolean hasCommitted() {
        assert mStarted;
        return mHasCommitted;
    }

    public int httpStatusCode() {
        assert mStarted;
        return mHttpStatusCode;
    }

    public boolean isErrorPage() {
        assert mStarted;
        return mIsErrorPage;
    }

    public boolean isReload() {
        assert mStarted;
        return mIsReload;
    }

    public boolean isHistory() {
        assert mStarted;
        return mIsHistory;
    }

    public boolean isBack() {
        return mIsBack;
    }

    public boolean isForward() {
        return mIsForward;
    }

    public boolean isRestore() {
        return mIsRestore;
    }

    public NavigationHandle getNavigation() {
        return mNavigationHandle;
    }
}
