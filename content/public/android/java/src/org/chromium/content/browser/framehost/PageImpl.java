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
    // WebContents. As a workaround, an entry is kept in a static map from the native pointer to the
    // Java object to prevent garbage collection.
    private static final LongSparseArray<PageImpl> sPages = new LongSparseArray<>();

    private boolean mIsPrerendering;
    private GURL mUrl = GURL.emptyGURL();
    private long mNativePage;

    private @Nullable PageDeletionListener mListener;

    // Holds the most recent PageState snapshot taken, this is taken every time mUrl changes
    private volatile PageState mMostRecentPageState;

    @Override
    public void setPageDeletionListener(PageDeletionListener listener) {
        mListener = listener;
    }

    @CalledByNative
    public PageImpl(long nativePage, boolean isPrerendering) {
        mNativePage = nativePage;
        mIsPrerendering = isPrerendering;

        takePageSnapshot();

        if (mNativePage != 0) {
            assert sPages.get(mNativePage) == null;
            sPages.put(mNativePage, this);
        }
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
        assert mNativePage != 0;
        var removedValue = sPages.get(mNativePage);
        sPages.remove(mNativePage);
        assert removedValue == this;
        mNativePage = 0;
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
    private static @Nullable PageImpl getJavaObject(long nativePage) {
        return sPages.get(nativePage);
    }

    @Override
    @AnyThread
    public PageState getMostRecentPageState() {
        return mMostRecentPageState;
    }

    /** Take a snapshot of the current state of Page */
    private void takePageSnapshot() {
        mMostRecentPageState = new PageState(mUrl);
    }
}
