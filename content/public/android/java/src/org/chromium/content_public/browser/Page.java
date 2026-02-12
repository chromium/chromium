// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** JNI bridge with content::Page */
@JNINamespace("content")
@NullMarked
public class Page {
    // Using ScopedJavaGlobalRef in the owning C++ object to keep the Java object alive consumes an
    // entry per instance in the finite global ref table. This scales poorly with a large number of
    // WebContents. As a workaround, the C++ owner uses a JavaObjectWeakGlobalRef and an entry is
    // kept in the a static map of the native pointer to Java objects to prevent garbage collection.
    private static final Map<Long, Page> sPages = new HashMap<>();

    private boolean mIsPrerendering;
    private GURL mUrl = GURL.emptyGURL();
    private long mNativePage;

    private @Nullable PageDeletionListener mListener;

    public static Page createForTesting() {
        return new Page(/* nativePage= */ 0, /* isPrerendering= */ false);
    }

    // Listener for when the native C++ Page object is destructed.
    public interface PageDeletionListener {
        void onWillDeletePage(Page page);
    }

    public void setPageDeletionListener(PageDeletionListener listener) {
        mListener = listener;
    }

    @CalledByNative
    private Page(long nativePage, boolean isPrerendering) {
        mNativePage = nativePage;
        mIsPrerendering = isPrerendering;
        if (mNativePage != 0) {
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
        var removedValue = sPages.remove(mNativePage);
        assert removedValue != null;
        mNativePage = 0;
    }

    public boolean isPrerendering() {
        return mIsPrerendering;
    }

    public void setIsPrerendering(boolean isPrerendering) {
        mIsPrerendering = isPrerendering;
    }

    public GURL getUrl() {
        return mUrl;
    }

    public void setUrl(GURL url) {
        mUrl = url;
    }
}
