// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.TextAppearanceSpan;
import android.view.View;

import org.chromium.content.R;
import org.chromium.ui.base.WindowAndroid;

/**
 * A subclass of SuggestionsPopupWindow to be used for showing suggestions from one or more
 * SuggestionSpans.
 */
public class TextSuggestionsPopupWindow extends SuggestionsPopupWindow {
    private SuggestionInfo[] mSuggestionInfos;
    private TextAppearanceSpan mPrefixSpan;
    private TextAppearanceSpan mSuffixSpan;

    /**
     * @param context Android context to use.
     * @param textSuggestionHost TextSuggestionHost instance (used to communicate with Blink).
     * @param windowAndroid The current WindowAndroid instance.
     * @param parentView The view used to attach the PopupWindow.
     */
    public TextSuggestionsPopupWindow(
            Context context,
            TextSuggestionHost textSuggestionHost,
            WindowAndroid windowAndroid,
            View parentView) {
        super(context, textSuggestionHost, windowAndroid, parentView);

        mPrefixSpan =
                new TextAppearanceSpan(context, R.style.TextAppearance_SuggestionPrefixOrSuffix);
        mSuffixSpan =
                new TextAppearanceSpan(context, R.style.TextAppearance_SuggestionPrefixOrSuffix);
    }

    /** Shows the text suggestion menu at the specified coordinates (relative to the viewport). */
    public void show(
            double caretX,
            double caretY,
            String highlightedText,
            SuggestionInfo[] suggestionInfos) {
        mSuggestionInfos = suggestionInfos.clone();

        // Android's Editor.java shows the "Add to dictonary" button if and only if there's a
        // SuggestionSpan with FLAG_MISSPELLED set. However, some OEMs (e.g. Samsung) appear to
        // change the behavior on their devices to never show this button, since their IMEs don't go
        // through the normal spell-checking API and instead add SuggestionSpans directly. Since
        // it's difficult to determine how the OEM has set up the native menu, we instead only show
        // the "Add to dictionary" button for spelling markers added by Chrome from running the
        // system spell checker. SuggestionSpans with FLAG_MISSPELLED set (i.e., a spelling
        // underline added directly by the IME) do not show this button.
        setAddToDictionaryEnabled(false);
        super.show(caretX, caretY, highlightedText);
    }

    @Override
    protected int getSuggestionsCount() {
        return mSuggestionInfos.length;
    }

    @Override
    protected Object getSuggestionItem(int position) {
        return mSuggestionInfos[position];
    }

    @Override
    protected SpannableString getSuggestionText(int position) {
        final SuggestionInfo suggestionInfo = mSuggestionInfos[position];

        SpannableString suggestionText =
                new SpannableString(
                        suggestionInfo.getPrefix()
                                + suggestionInfo.getSuggestion()
                                + suggestionInfo.getSuffix());

        // Gray out prefix text (if any).
        suggestionText.setSpan(
                mPrefixSpan,
                0,
                suggestionInfo.getPrefix().length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        // Gray out suffix text (if any).
        suggestionText.setSpan(
                mSuffixSpan,
                suggestionInfo.getPrefix().length() + suggestionInfo.getSuggestion().length(),
                suggestionInfo.getPrefix().length()
                        + suggestionInfo.getSuggestion().length()
                        + suggestionInfo.getSuffix().length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        return suggestionText;
    }

    @Override
    protected void applySuggestion(int position) {
        SuggestionInfo info = mSuggestionInfos[position];
        mTextSuggestionHost.applyTextSuggestion(info.getMarkerTag(), info.getSuggestionIndex());
    }
}
