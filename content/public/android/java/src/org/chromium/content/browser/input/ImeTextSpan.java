// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.mojom.ImeTextSpanType;

/** Data for a text span with IME suggestions. */
@JNINamespace("content")
@NullMarked
public class ImeTextSpan {
    private final int mStartOffset;
    private final int mEndOffset;
    private final String[] mSuggestions;
    private final @ImeTextSpanType.EnumType int mType;
    private final boolean mShouldHideSuggestionMenu;

    private ImeTextSpan(
            int startOffset,
            int endOffset,
            String[] suggestions,
            @ImeTextSpanType.EnumType int type,
            boolean shouldHideSuggestionMenu) {
        mStartOffset = startOffset;
        mEndOffset = endOffset;
        mSuggestions = suggestions;
        mType = type;
        mShouldHideSuggestionMenu = shouldHideSuggestionMenu;
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

    /**
     * @return The boolean value which indicates if the suggestion menu should be hidden.
     */
    public boolean shouldHideSuggestionMenu() {
        return mShouldHideSuggestionMenu;
    }

    /**
     * @return The type for the span. See {@link ImeTextSpan.MojomImeTextSpanType}
     */
    public @ImeTextSpanType.EnumType int getType() {
        return mType;
    }

    @CalledByNative
    private static ImeTextSpan create(
            int startOffset,
            int endOffset,
            String[] suggestions,
            @ImeTextSpanType.EnumType int type,
            boolean shouldHideeSuggestionMenu) {
        return new ImeTextSpan(
                startOffset, endOffset, suggestions, type, shouldHideeSuggestionMenu);
    }
}
