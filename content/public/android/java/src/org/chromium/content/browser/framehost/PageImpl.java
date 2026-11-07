// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.framehost;

import android.util.LongSparseArray;

import androidx.annotation.AnyThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.PageState;
import org.chromium.url.GURL;

/**
 * JNI bridge with content::Page
 *
 * <p>All methods should be called on the UI thread with exception to getMostRecentPageState
 */
@JNINamespace("content")
@NullMarked
public class PageImpl implements Page {
    // Using ScopedJavaGlobalRef in the owning C++ object to keep the Java object alive consumes an
    // entry per instance in the finite global ref table. This scales poorly with a large number of
    // WebContents. As a workaround, an entry is kept in a static map keyed by a Java-assigned peer
    // ID to prevent garbage collection.
    private static final LongSparseArray<PageImpl> sPages = new LongSparseArray<>();
    private static long sNextPeerId = 1;

    private boolean mIsPrerendering;
    private GURL mUrl = GURL.emptyGURL();
    private final long mPeerId;

    private @Nullable PageDeletionListener mListener;

    // Holds the most recent PageState snapshot taken, this is taken every time mUrl changes
    private volatile PageState mMostRecentPageState;

    @Override
    public void setPageDeletionListener(PageDeletionListener listener) {
        mListener = listener;
    }

    public PageImpl(long peerId, boolean isPrerendering) {
        mPeerId = peerId;
        mIsPrerendering = isPrerendering;

        takePageSnapshot();
    }

    @CalledByNative
    private static long createAndGetPeerId(boolean isPrerendering) {
        long peerId = sNextPeerId++;
        PageImpl page = new PageImpl(peerId, isPrerendering);
        assert sPages.get(peerId) == null;
        sPages.put(peerId, page);
        return peerId;
    }

    /** The C++ page is about to be deleted. */
    @CalledByNative
    private void willDeletePage(boolean isPrerendering) {
        mIsPrerendering = isPrerendering;
        if (mListener != null) {
            mListener.onWillDeletePage(this);
        }
    }

    @CalledByNative
    private void destroy() {
        assert mPeerId != 0;
        var removedValue = sPages.get(mPeerId);
        sPages.remove(mPeerId);
        assert removedValue == this;
    }

    @Override
    public boolean isPrerendering() {
        return mIsPrerendering;
    }

    @Override
    public void setIsPrerendering(boolean isPrerendering) {
        mIsPrerendering = isPrerendering;
    }

    @Override
    public GURL getUrl() {
        return mUrl;
    }

    @Override
    public void setUrl(GURL url) {
        mUrl = url;
        takePageSnapshot();
    }

    @CalledByNative
    private static @Nullable PageImpl getJavaObject(long peerId) {
        return sPages.get(peerId);
    }

    @Override
    @AnyThread
    public PageState getMostRecentPageState() {
        return mMostRecentPageState;
    }

    /** Take a snapshot of the current state of Page */
    private void takePageSnapshot() {
        mMostRecentPageState = new PageState(this, mUrl);
    }
}
