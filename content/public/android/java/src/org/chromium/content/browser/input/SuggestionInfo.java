// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import org.jni_zero.CalledByNative;

/**
 * Represents an entry in a text suggestion popup menu. Contains the information
 * necessary to display the menu entry and the information necessary to apply
 * the suggestion.
 */
public class SuggestionInfo {
    private final int mMarkerTag;
    private final int mSuggestionIndex;
    private final String mPrefix;
    private final String mSuggestion;
    private final String mSuffix;

    SuggestionInfo(
            int markerTag, int suggestionIndex, String prefix, String suggestion, String suffix) {
        mMarkerTag = markerTag;
        mSuggestionIndex = suggestionIndex;
        mPrefix = prefix;
        mSuggestion = suggestion;
        mSuffix = suffix;
    }

    /** Used as an opaque identifier to tell Blink which suggestion was picked. */
    public int getMarkerTag() {
        return mMarkerTag;
    }

    /** Used as an opaque identifier to tell Blink which suggestion was picked. */
    public int getSuggestionIndex() {
        return mSuggestionIndex;
    }

    /**
     * Text at the beginning of the highlighted suggestion region that will not be changed by
     * applying the suggestion.
     */
    public String getPrefix() {
        return mPrefix;
    }

    /**
     * Text that will replace the text between the prefix and suffix strings if the suggestion is
     * applied.
     */
    public String getSuggestion() {
        return mSuggestion;
    }

    /**
     * Text at the end of the highlighted suggestion region that will not be changed by
     * applying the suggestion.
     */
    public String getSuffix() {
        return mSuffix;
    }

    @CalledByNative
    private static SuggestionInfo[] createArray(int length) {
        return new SuggestionInfo[length];
    }

    @CalledByNative
    private static void createSuggestionInfoAndPutInArray(
            SuggestionInfo[] suggestionInfos,
            int index,
            int markerTag,
            int suggestionIndex,
            String prefix,
            String suggestion,
            String suffix) {
        SuggestionInfo suggestionInfo =
                new SuggestionInfo(markerTag, suggestionIndex, prefix, suggestion, suffix);
        suggestionInfos[index] = suggestionInfo;
    }
}
