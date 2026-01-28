// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.icu.text.BreakIterator;
import android.view.inputmethod.CorrectionInfo;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A class that manages the visual presentation and state of autocorrect underline. This involves
 * showing the underline as well as removing the underline.
 */
@NullMarked
public class AutocorrectManager {
    private static final String TAG = "AutocorrectManager";
    private static final boolean DEBUG_LOGS = false;

    private final Delegate mDelegate;
    private @Nullable CorrectionInfo mCorrectionInfo;

    public interface Delegate {
        void appendAutocorrectUnderlineSpan(int start, int end);
    }

    public AutocorrectManager(Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Processes a {@link CorrectionInfo} object provided by the IME. This method is called when the
     * IME suggests an autocorrection, triggering UI updates and state changes.
     *
     * @param correctionInfo Details about the suggested correction.
     */
    public void handlePendingCorrection(CorrectionInfo correctionInfo) {
        if (DEBUG_LOGS) {
            Log.i(
                    TAG,
                    "handlePendingCorrection [%s]",
                    ImeUtils.getCorrectionInfoDebugString(correctionInfo));
        }
        // Only the latest correction is stored; if multiple are sent, the last one wins.
        // This is sufficient because IMEs like GBoard trigger autocorrect by sending
        // a commitCorrection followed immediately by commitText.
        mCorrectionInfo = correctionInfo;
    }

    public void maybeAppendAutocorrectUnderlineSpan() {
        if (DEBUG_LOGS) Log.i(TAG, "maybeAppendAutocorrectUnderlineSpan");
        if (mCorrectionInfo == null) {
            return;
        }
        int start = mCorrectionInfo.getOffset();
        int end = start + findLengthOfTextToUnderline();

        mDelegate.appendAutocorrectUnderlineSpan(start, end);
        mCorrectionInfo = null;
    }

    /**
     * Calculates the length of the text to be underlined during a correction.
     *
     * <p>Note: We cannot compare new text vs. old text because Gboard clears the old text before
     * calling commitCorrection, leaving getOldText() empty. This logic ensures that if the new text
     * ends in a non-word character (like a space or punctuation), that trailing character is NOT
     * included in the underline span.
     *
     * <ul>
     *   Example:
     *   <li>"Hello" -> Underlines 5 chars ("Hello")
     *   <li>"Hello " -> Underlines 5 chars ("Hello"), excluding the trailing space.
     * </ul>
     */
    private int findLengthOfTextToUnderline() {
        if (mCorrectionInfo == null) return 0;
        CharSequence text = mCorrectionInfo.getNewText();
        BreakIterator boundary = BreakIterator.getWordInstance();
        boundary.setText(text);

        boundary.last();
        int start = boundary.previous();
        if (boundary.getRuleStatus() != BreakIterator.WORD_NONE) {
            return start;
        }
        return text.length();
    }
}
