// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.url.GURL;

/**
 * Provides the details of a committed navigation entry for the
 * {@link WebContentsObserver#navigationEntryCommitted(LoadCommittedDetails)}.
 */
@JNINamespace("content")
public class LoadCommittedDetails {
    private final boolean mDidReplaceEntry;
    private final int mPreviousEntryIndex;
    private final GURL mPreviousMainFrameUrl;
    private final boolean mIsSameDocument;
    private final boolean mIsMainFrame;
    private final int mHttpStatusCode;

    @CalledByNative
    public LoadCommittedDetails(
            int previousEntryIndex,
            GURL previousMainFrameUrl,
            boolean didReplaceEntry,
            boolean isSameDocument,
            boolean isMainFrame,
            int httpStatusCode) {
        mPreviousEntryIndex = previousEntryIndex;
        mPreviousMainFrameUrl = previousMainFrameUrl;
        mDidReplaceEntry = didReplaceEntry;
        mIsSameDocument = isSameDocument;
        mIsMainFrame = isMainFrame;
        mHttpStatusCode = httpStatusCode;
    }

    public boolean didReplaceEntry() {
        return mDidReplaceEntry;
    }

    public int getPreviousEntryIndex() {
        return mPreviousEntryIndex;
    }

    public GURL getPreviousMainFrameUrl() {
        return mPreviousMainFrameUrl;
    }

    public boolean isSameDocument() {
        return mIsSameDocument;
    }

    public boolean isMainFrame() {
        return mIsMainFrame;
    }

    public int getHttpStatusCode() {
        return mHttpStatusCode;
    }
}
