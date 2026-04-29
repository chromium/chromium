// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.icu.text.BreakIterator;
import android.view.inputmethod.CorrectionInfo;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class that manages the visual presentation and state of autocorrect underline. This involves
 * showing the underline as well as removing the underline.
 */
@NullMarked
public class AutocorrectManager {
    private static final String TAG = "AutocorrectManager";
    private static final boolean DEBUG_LOGS = false;

    // In V1, the underline is applied after commitText(). That commitText() call also
    // decrements the counter, so we start at 5 to end up with 4 remaining actions.
    public static final int USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V1 = 5;

    // In V2, the underline is applied immediately upon commitCorrection(). There is no
    // subsequent commitText() to decrement the counter, so we start directly at 4.
    public static final int USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V2 = 4;

    @IntDef({
        UnderlineStrategy.DEFER_UNTIL_COMMIT_TEXT,
        UnderlineStrategy.APPLY_ON_COMMIT_CORRECTION
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface UnderlineStrategy {
        int DEFER_UNTIL_COMMIT_TEXT = 0;
        int APPLY_ON_COMMIT_CORRECTION = 1;
    }

    private final Delegate mDelegate;
    private final @UnderlineStrategy int mStrategy;
    private @Nullable CorrectionInfo mCorrectionInfo;

    // TODO: crbug.com/480717682 - Moving the cursor back into autocorrected text can trigger an
    // 'Undo' that clears the underline in UI. This can cause the state to temporarily desync.
    // This desync has no user visible impact as when the threshold is reached or another
    // autocorrect event is triggered it will call clearAllAutocorrectUnderlineSpans(), which acts
    // as a visual no-op and re-syncs the state.
    private int mRemainingUserActionsBeforeClear;

    public interface Delegate {
        void appendAutocorrectUnderlineSpan(int start, int end);

        void clearAllAutocorrectUnderlineSpans();
    }

    public AutocorrectManager(Delegate delegate) {
        mDelegate = delegate;
        mStrategy =
                ContentFeatureMap.isEnabled(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
                        ? UnderlineStrategy.APPLY_ON_COMMIT_CORRECTION
                        : UnderlineStrategy.DEFER_UNTIL_COMMIT_TEXT;
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

        if (mRemainingUserActionsBeforeClear > 0) {
            mRemainingUserActionsBeforeClear = 0;
            mDelegate.clearAllAutocorrectUnderlineSpans();
        }

        switch (mStrategy) {
            case UnderlineStrategy.APPLY_ON_COMMIT_CORRECTION:
                // V2 draws the underline immediately upon handlePendingCorrection().
                applyUnderline(correctionInfo);
                break;
            case UnderlineStrategy.DEFER_UNTIL_COMMIT_TEXT:
                // Only the latest correction is stored; if multiple are sent, the last one wins.
                // This is sufficient because IMEs like GBoard trigger autocorrect by sending
                // a commitCorrection followed immediately by commitText.
                mCorrectionInfo = correctionInfo;
                break;
        }
    }

    public void maybeApplyDeferredUnderline() {
        if (DEBUG_LOGS) Log.i(TAG, "maybeApplyDeferredUnderline");

        switch (mStrategy) {
            case UnderlineStrategy.DEFER_UNTIL_COMMIT_TEXT:
                // We expect a commitText() call to come immediately after commitCorrection()
                // from the IME (observed in Gboard, SwiftKey and Yandex). We wait for that
                // commitText to trigger this method so we can apply the underline span to
                // the text that was just committed.
                if (mCorrectionInfo == null) return;

                applyUnderline(mCorrectionInfo);
                mCorrectionInfo = null;
                return;
            case UnderlineStrategy.APPLY_ON_COMMIT_CORRECTION:
                // V2 does not defer the underline drawing.
                return;
        }
    }

    private void applyUnderline(CorrectionInfo correctionInfo) {
        int start = correctionInfo.getOffset();
        int end = start + findLengthOfTextToUnderline(correctionInfo);

        mRemainingUserActionsBeforeClear =
                mStrategy == UnderlineStrategy.APPLY_ON_COMMIT_CORRECTION
                        ? USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V2
                        : USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V1;
        mDelegate.appendAutocorrectUnderlineSpan(start, end);
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
    private int findLengthOfTextToUnderline(CorrectionInfo correctionInfo) {
        CharSequence text = correctionInfo.getNewText();
        BreakIterator boundary = BreakIterator.getWordInstance();
        boundary.setText(text);

        int last = boundary.last();
        int start = boundary.previous();
        if (boundary.getRuleStatus() != BreakIterator.WORD_NONE) {
            return start;
        }
        return last;
    }

    public void onCommitTextOrSendKeyEvent() {
        if (DEBUG_LOGS) Log.i(TAG, "onCommitTextOrSendKeyEvent");
        if (mRemainingUserActionsBeforeClear == 0) return;
        mRemainingUserActionsBeforeClear--;
        if (mRemainingUserActionsBeforeClear == 0) {
            mDelegate.clearAllAutocorrectUnderlineSpans();
        }
    }
}
