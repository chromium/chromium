// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import android.text.TextUtils;

/**
 * Date/time suggestion container used to store information for each suggestion that will be shown
 * in the suggestion list dialog. Keep in sync with date_time_suggestion.h.
 */
public class DateTimeSuggestion {
    private final double mValue;
    private final String mLocalizedValue;
    private final String mLabel;

    /**
     * Constructs a color suggestion container.
     * @param value The suggested date/time value.
     * @param localizedValue The suggested value localized.
     * @param label The label for the suggestion.
     */
    public DateTimeSuggestion(double value, String localizedValue, String label) {
        mValue = value;
        mLocalizedValue = localizedValue;
        mLabel = label;
    }

    double value() {
        return mValue;
    }

    String localizedValue() {
        return mLocalizedValue;
    }

    String label() {
        return mLabel;
    }

    @Override
    public boolean equals(Object object) {
        if (!(object instanceof DateTimeSuggestion)) {
            return false;
        }
        final DateTimeSuggestion other = (DateTimeSuggestion) object;
        return mValue == other.mValue
                && TextUtils.equals(mLocalizedValue, other.mLocalizedValue)
                && TextUtils.equals(mLabel, other.mLabel);
    }

    @Override
    public int hashCode() {
        int hash = 31;
        hash = 37 * hash + (int) mValue;
        hash = 37 * hash + mLocalizedValue.hashCode();
        hash = 37 * hash + mLabel.hashCode();
        return hash;
    }
}
