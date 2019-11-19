// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import androidx.annotation.VisibleForTesting;

import java.text.BreakIterator;
import java.util.regex.Pattern;

/**
 * Converts character indices to relative word indices.
 * Since start offset from showSelectionMenu() event is not very reliable due to JavaScript
 * triggered DOM changes, this class is doing best effort to make sure that we could log as much as
 * we can by detecting such changes.
 *
 * Usage:
 * When the logging session started, setInitialStartOffset() should be called to set the start
 * offset at that time point.
 *
 * Each time, before calling getWordDelta() for the current selection modification/action,
 * updateSelectionState() must be called. If updateSelectionState() or getWordDelta() returns false,
 * we should end the current logging session immediately since there must be a DOM change.
 */
public class SelectionIndicesConverter {
    private static final Pattern PATTERN_WHITESPACE = Pattern.compile("[\\p{javaSpaceChar}\\s]+");

    // Tracking the overall selection during current logging session.
    private String mGlobalSelectionText;
    private int mGlobalStartOffset;

    // Tracking previous selection.
    private String mLastSelectionText;
    private int mLastStartOffset;

    // The start offset from SelectionStarted call.
    private int mInitialStartOffset;

    // Try to update global selection with the current selection so we could use global selection
    // information to calculate relative word indices. Due to the DOM tree change, startOffset is
    // not very reliable. We need to invalidate loging session if we detected such changes.
    public boolean updateSelectionState(String selectionText, int startOffset) {
        if (mGlobalSelectionText == null) {
            updateLastSelection(selectionText, startOffset);
            updateGlobalSelection(selectionText, startOffset);
            return true;
        }

        boolean update = false;
        int endOffset = startOffset + selectionText.length();
        int lastEndOffset = mLastStartOffset + mLastSelectionText.length();
        // Handle overlapping case.
        if (overlap(mLastStartOffset, lastEndOffset, startOffset, endOffset)) {
            // We need to compare the overlapping part to make sure that we can update it.
            int l = Math.max(mLastStartOffset, startOffset);
            int r = Math.min(lastEndOffset, endOffset);
            update = mLastSelectionText.regionMatches(
                    l - mLastStartOffset, selectionText, l - startOffset, r - l);
        }

        // Handle adjacent cases.
        if (mLastStartOffset == endOffset || lastEndOffset == startOffset) {
            update = true;
        }

        if (!update) {
            mGlobalSelectionText = null;
            mLastSelectionText = null;
            return false;
        }

        updateLastSelection(selectionText, startOffset);
        combineGlobalSelection(selectionText, startOffset);
        return true;
    }

    public boolean getWordDelta(int start, int end, int[] wordIndices) {
        assert wordIndices.length == 2;
        wordIndices[0] = wordIndices[1] = 0;

        start = start - mGlobalStartOffset;
        end = end - mGlobalStartOffset;
        if (start >= end) return false;
        if (start < 0 || start >= mGlobalSelectionText.length()) return false;
        if (end <= 0 || end > mGlobalSelectionText.length()) return false;

        int initialStartOffset = mInitialStartOffset - mGlobalStartOffset;

        BreakIterator breakIterator = BreakIterator.getWordInstance();
        breakIterator.setText(mGlobalSelectionText);

        if (start <= initialStartOffset) {
            wordIndices[0] = -countWordsForward(start, initialStartOffset, breakIterator);
        } else {
            // start > initialStartOffset
            wordIndices[0] = countWordsBackward(start, initialStartOffset, breakIterator);
            // For the selection start index, avoid counting a partial word backwards.
            // Use "New York City" as an example, if the selection started with "New", now we want
            // to count "York" by calling countWordBackward(4, 0). We will count "Y" and "New" as
            // two words, but we want the result be 1, so taking one step back here.
            if (!breakIterator.isBoundary(start)
                    && !isWhitespace(
                               breakIterator.preceding(start), breakIterator.following(start))) {
                // We counted a partial word. Remove it.
                wordIndices[0]--;
            }
        }

        if (end <= initialStartOffset) {
            wordIndices[1] =
                    -countWordsForward(/* start = */ end, initialStartOffset, breakIterator);
        } else {
            // end > initialStartOffset
            wordIndices[1] =
                    countWordsBackward(/* start = */ end, initialStartOffset, breakIterator);
        }

        return true;
    }

    public void setInitialStartOffset(int offset) {
        mInitialStartOffset = offset;
    }

    @VisibleForTesting
    protected String getGlobalSelectionText() {
        return mGlobalSelectionText;
    }

    @VisibleForTesting
    protected int getGlobalStartOffset() {
        return mGlobalStartOffset;
    }

    // Count how many words from "start" to "end", "start" should greater than or equal
    // to "end", punctuations are counted as words. Duplicated from Android.
    @VisibleForTesting
    protected int countWordsBackward(int start, int end, BreakIterator iterator) {
        assert start >= end;
        int wordCount = 0;
        int offset = start;
        while (offset > end) {
            int i = iterator.preceding(offset);
            if (!isWhitespace(i, offset)) {
                wordCount++;
            }
            offset = i;
        }
        return wordCount;
    }

    // Count how many words from "start" to "end", "start" should less than or equal
    // to "end", punctuations are counted as words. Duplicated from Android.
    @VisibleForTesting
    protected int countWordsForward(int start, int end, BreakIterator iterator) {
        assert start <= end;
        int wordCount = 0;
        int offset = start;
        while (offset < end) {
            int i = iterator.following(offset);
            if (!isWhitespace(offset, i)) {
                wordCount++;
            }
            offset = i;
        }
        return wordCount;
    }

    @VisibleForTesting
    protected boolean isWhitespace(int start, int end) {
        return PATTERN_WHITESPACE.matcher(mGlobalSelectionText.substring(start, end)).matches();
    }

    // Check if [al, ar) overlaps [bl, br).
    @VisibleForTesting
    protected static boolean overlap(int al, int ar, int bl, int br) {
        if (al <= bl) {
            return bl < ar;
        }
        return br > al;
    }

    // Check if [al, ar) contains [bl, br).
    @VisibleForTesting
    protected static boolean contains(int al, int ar, int bl, int br) {
        return al <= bl && br <= ar;
    }

    private void updateLastSelection(String selectionText, int startOffset) {
        mLastSelectionText = selectionText;
        mLastStartOffset = startOffset;
    }

    private void updateGlobalSelection(String selectionText, int startOffset) {
        mGlobalSelectionText = selectionText;
        mGlobalStartOffset = startOffset;
    }

    // Use current selection to gradually update global selection.
    // Within each selection logging session, we obtain the next selection from shrink, expand or
    // reverse select the current selection. To update global selection, we only need to extend both
    // sides of the last global selection with current selection if necessary.
    private void combineGlobalSelection(String selectionText, int startOffset) {
        int endOffset = startOffset + selectionText.length();
        int globalEndOffset = mGlobalStartOffset + mGlobalSelectionText.length();

        // Extends left if necessary.
        if (startOffset < mGlobalStartOffset) {
            updateGlobalSelection(selectionText.substring(0, mGlobalStartOffset - startOffset)
                            + mGlobalSelectionText,
                    startOffset);
        }

        // Extends right if necessary.
        if (endOffset > globalEndOffset) {
            updateGlobalSelection(mGlobalSelectionText
                            + selectionText.substring(
                                      globalEndOffset - startOffset, selectionText.length()),
                    mGlobalStartOffset);
        }
    }
}
