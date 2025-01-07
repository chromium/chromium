// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.text.SpannableString;
import android.view.View;

import org.chromium.ui.base.WindowAndroid;

/**
 * A subclass of SuggestionsPopupWindow to be used for showing suggestions from a spell check
 * marker.
 */
public class SpellCheckPopupWindow extends SuggestionsPopupWindow {
    private String[] mSuggestions = new String[0];

    /**
     * @param context Android context to use.
     * @param textSuggestionHost TextSuggestionHost instance (used to communicate with Blink).
     * @param windowAndroid The current WindowAndroid instance.
     * @param parentView The view used to attach the PopupWindow.
     */
    public SpellCheckPopupWindow(
            Context context,
            TextSuggestionHost textSuggestionHost,
            WindowAndroid windowAndroid,
            View parentView) {
        super(context, textSuggestionHost, windowAndroid, parentView);
    }

    /** Shows the spell check menu at the specified coordinates (relative to the viewport). */
    public void show(double caretX, double caretY, String highlightedText, String[] suggestions) {
        mSuggestions = suggestions.clone();
        setAddToDictionaryEnabled(true);
        super.show(caretX, caretY, highlightedText);
    }

    @Override
    protected int getSuggestionsCount() {
        return mSuggestions.length;
    }

    @Override
    protected Object getSuggestionItem(int position) {
        return mSuggestions[position];
    }

    @Override
    protected SpannableString getSuggestionText(int position) {
        return new SpannableString(mSuggestions[position]);
    }

    @Override
    protected void applySuggestion(int position) {
        mTextSuggestionHost.applySpellCheckSuggestion(mSuggestions[position]);
    }
}
