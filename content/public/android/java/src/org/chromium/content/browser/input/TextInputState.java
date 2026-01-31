// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.Build;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.view.inputmethod.SurroundingText;
import android.view.inputmethod.InputConnection;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;

import java.util.Locale;

/**
 * An immutable class to contain text, selection range, composition range, and whether
 * it's single line or multiple lines that are being edited.
 */
@NullMarked
public class TextInputState {
    private final CharSequence mText;
    private final Range mSelection;
    private final Range mComposition;
    private final boolean mSingleLine;
    private final boolean mReplyToRequest;

    /**
     * Class added for junit test, because junit doesn't have SurroundingText from the Android
     * framework yet.
     *
     * TODO(ctzsm): Replace its usage with the framework SurrroundingText class once junit supports
     * it.
     */
    /* package */ static class SurroundingTextInternal {
        public final CharSequence mText;
        public final int mSelectionStart;
        public final int mSelectionEnd;
        public final int mOffset;

        public SurroundingTextInternal(
                CharSequence text, int selectionStart, int selectionEnd, int offset) {
            mText = text;
            mSelectionStart = selectionStart;
            mSelectionEnd = selectionEnd;
            mOffset = offset;
        }
    }

    public TextInputState(
            CharSequence text,
            Range selection,
            Range composition,
            boolean singleLine,
            boolean replyToRequest) {
        selection.clamp(0, text.length());
        if (composition.start() != -1 || composition.end() != -1) {
            composition.clamp(0, text.length());
        }
        mText = text;
        mSelection = selection;
        mComposition = composition;
        mSingleLine = singleLine;
        mReplyToRequest = replyToRequest;
    }

    public CharSequence text() {
        return mText;
    }

    public Range selection() {
        return mSelection;
    }

    public Range composition() {
        return mComposition;
    }

    public boolean singleLine() {
        return mSingleLine;
    }

    public boolean replyToRequest() {
        return mReplyToRequest;
    }

    private static CharSequence subsequence(CharSequence source, int start, int end, int flags) {
        if (ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_IME_GET_FORMATTED_TEXT)
                && flags == InputConnection.GET_TEXT_WITH_STYLES) {
            return new SpannableStringBuilder(source, start, end);
        }
        return TextUtils.substring(source, start, end);
    }

    public @Nullable CharSequence getSelectedText(int flags) {
        if (mSelection.start() == mSelection.end()) return null;
        return subsequence(mText, mSelection.start(), mSelection.end(), flags);
    }

    public CharSequence getTextAfterSelection(int maxChars, int flags) {
        // Clamp the maxChars to avoid integer overflow or negative value.
        maxChars = Math.max(0, Math.min(maxChars, mText.length() - mSelection.end()));
        int afterSelectionEnd = Math.min(mText.length(), mSelection.end() + maxChars);
        return subsequence(mText, mSelection.end(), afterSelectionEnd, flags);
    }

    public CharSequence getTextBeforeSelection(int maxChars, int flags) {
        // Clamp the maxChars to the valid value.
        maxChars = Math.max(0, Math.min(maxChars, mSelection.start()));
        int beforeSelectionStart = Math.max(0, mSelection.start() - maxChars);
        return subsequence(mText, beforeSelectionStart, mSelection.start(), flags);
    }

    @RequiresApi(Build.VERSION_CODES.S)
    public SurroundingText getSurroundingText(int beforeLength, int afterLength, int flags) {
        SurroundingTextInternal surroundingText =
                getSurroundingTextInternal(beforeLength, afterLength, flags);
        return new SurroundingText(
                surroundingText.mText,
                surroundingText.mSelectionStart,
                surroundingText.mSelectionEnd,
                surroundingText.mOffset);
    }

    @VisibleForTesting
    /* package */ SurroundingTextInternal getSurroundingTextInternal(
            int beforeLength, int afterLength, int flags) {
        beforeLength = Math.max(0, Math.min(beforeLength, mSelection.start()));
        afterLength = Math.max(0, Math.min(afterLength, mText.length() - mSelection.end()));
        int start = mSelection.start() - beforeLength;
        int end = mSelection.end() + afterLength;
        CharSequence text;
        if ((ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_IME_GET_FORMATTED_TEXT)
                && flags == InputConnection.GET_TEXT_WITH_STYLES) || mText instanceof Spanned) {
            text = new SpannableStringBuilder(mText, start, end);
        } else {
            text = mText.subSequence(start, end);
        }

        return new SurroundingTextInternal(
                text,
                /* selectionStart= */ beforeLength,
                /* selectionEnd= */ beforeLength + mSelection.end() - mSelection.start(),
                /* offset= */ start);
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof TextInputState)) return false;
        TextInputState t = (TextInputState) o;
        if (t == this) return true;
        return TextUtils.equals(mText, t.mText)
                && mSelection.equals(t.mSelection)
                && mComposition.equals(t.mComposition)
                && mSingleLine == t.mSingleLine
                && mReplyToRequest == t.mReplyToRequest;
    }

    @Override
    public int hashCode() {
        return mText.hashCode() * 7
                + mSelection.hashCode() * 11
                + mComposition.hashCode() * 13
                + (mSingleLine ? 19 : 0)
                + (mReplyToRequest ? 23 : 0);
    }

    @SuppressWarnings("unused")
    public boolean shouldUnblock() {
        return false;
    }

    @Override
    public String toString() {
        return String.format(
                Locale.US,
                "TextInputState {[%s] SEL%s COM%s %s%s}",
                mText,
                mSelection,
                mComposition,
                mSingleLine ? "SIN" : "MUL",
                mReplyToRequest ? " ReplyToRequest" : "");
    }
}
