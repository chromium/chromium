// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** JNI bridge with content::Page */
@JNINamespace("content")
@NullMarked
public class Page {
    private boolean mIsPrerendering;

    private @Nullable PageDeletionListener mListener;

    public static Page createForTesting() {
        return new Page(/* isPrerendering= */ false);
    }

    // Listener for when the native C++ Page object is destructed.
    public interface PageDeletionListener {
        void onWillDeletePage(Page page);
    }

    public void setPageDeletionListener(PageDeletionListener listener) {
        mListener = listener;
    }

    @CalledByNative
    private Page(boolean isPrerendering) {
        mIsPrerendering = isPrerendering;
    }

    /** The C++ page is about to be deleted. */
    @CalledByNative
    private void willDeletePage(boolean isPrerendering) {
        mIsPrerendering = isPrerendering;
        if (mListener != null) {
            mListener.onWillDeletePage(this);
        }
    }

    public boolean isPrerendering() {
        return mIsPrerendering;
    }

    public void setIsPrerendering(boolean isPrerendering) {
        mIsPrerendering = isPrerendering;
    }
}
