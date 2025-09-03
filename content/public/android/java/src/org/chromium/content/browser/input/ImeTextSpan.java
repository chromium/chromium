// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/** Data for a text span with IME suggestions. */
@JNINamespace("content")
@NullMarked
public class ImeTextSpan {
    private final int mStartOffset;
    private final int mEndOffset;
    private final String[] mSuggestions;

    private ImeTextSpan(int startOffset, int endOffset, String[] suggestions) {
        mStartOffset = startOffset;
        mEndOffset = endOffset;
        mSuggestions = suggestions;
    }

    /**
     * @return The start offset of the span.
     */
    public int getStartOffset() {
        return mStartOffset;
    }

    /**
     * @return The end offset of the span.
     */
    public int getEndOffset() {
        return mEndOffset;
    }

    /**
     * @return The suggestions for the span.
     */
    public String[] getSuggestions() {
        return mSuggestions;
    }

    @CalledByNative
    private static ImeTextSpan create(int startOffset, int endOffset, String[] suggestions) {
        return new ImeTextSpan(startOffset, endOffset, suggestions);
    }
}
