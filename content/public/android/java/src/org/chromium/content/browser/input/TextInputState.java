// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.Build;
import android.text.TextUtils;
import android.view.inputmethod.SurroundingText;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import java.util.Locale;

/**
 * An immutable class to contain text, selection range, composition range, and whether
 * it's single line or multiple lines that are being edited.
 */
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

    public CharSequence getSelectedText() {
        if (mSelection.start() == mSelection.end()) return null;
        return TextUtils.substring(mText, mSelection.start(), mSelection.end());
    }

    public CharSequence getTextAfterSelection(int maxChars) {
        // Clamp the maxChars to avoid integer overflow or negative value.
        maxChars = Math.max(0, Math.min(maxChars, mText.length() - mSelection.end()));
        return TextUtils.substring(
                mText, mSelection.end(), Math.min(mText.length(), mSelection.end() + maxChars));
    }

    public CharSequence getTextBeforeSelection(int maxChars) {
        // Clamp the maxChars to the valid value.
        maxChars = Math.max(0, Math.min(maxChars, mSelection.start()));
        return TextUtils.substring(
                mText, Math.max(0, mSelection.start() - maxChars), mSelection.start());
    }

    @RequiresApi(Build.VERSION_CODES.S)
    public SurroundingText getSurroundingText(int beforeLength, int afterLength) {
        SurroundingTextInternal surroundingText =
                getSurroundingTextInternal(beforeLength, afterLength);
        return new SurroundingText(
                surroundingText.mText,
                surroundingText.mSelectionStart,
                surroundingText.mSelectionEnd,
                surroundingText.mOffset);
    }

    @VisibleForTesting
    /* package */ SurroundingTextInternal getSurroundingTextInternal(
            int beforeLength, int afterLength) {
        beforeLength = Math.max(0, Math.min(beforeLength, mSelection.start()));
        afterLength = Math.max(0, Math.min(afterLength, mText.length() - mSelection.end()));
        CharSequence text =
                TextUtils.substring(
                        mText, mSelection.start() - beforeLength, mSelection.end() + afterLength);
        return new SurroundingTextInternal(
                text, beforeLength, mSelection.end() - (mSelection.start() - beforeLength), -1);
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
