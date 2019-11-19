// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content.browser.picker.DateTimeSuggestion;
import org.chromium.content.browser.picker.InputDialogContainer;
import org.chromium.ui.base.WindowAndroid;

/**
 * Plumbing for the different date/time dialog adapters.
 */
@JNINamespace("content")
class DateTimeChooserAndroid {

    private final long mNativeDateTimeChooserAndroid;
    private final InputDialogContainer mInputDialogContainer;

    private DateTimeChooserAndroid(Context context,
            long nativeDateTimeChooserAndroid) {
        mNativeDateTimeChooserAndroid = nativeDateTimeChooserAndroid;
        mInputDialogContainer = new InputDialogContainer(context,
                new InputDialogContainer.InputActionDelegate() {

                    @Override
                    public void replaceDateTime(double value) {
                        DateTimeChooserAndroidJni.get().replaceDateTime(
                                mNativeDateTimeChooserAndroid, DateTimeChooserAndroid.this, value);
                    }

                    @Override
                    public void cancelDateTimeDialog() {
                        DateTimeChooserAndroidJni.get().cancelDialog(
                                mNativeDateTimeChooserAndroid, DateTimeChooserAndroid.this);
                    }
                });
    }

    private void showDialog(int dialogType, double dialogValue,
                            double min, double max, double step,
                            DateTimeSuggestion[] suggestions) {
        mInputDialogContainer.showDialog(dialogType, dialogValue, min, max, step, suggestions);
    }

    @CalledByNative
    private static DateTimeChooserAndroid createDateTimeChooser(
            WindowAndroid windowAndroid,
            long nativeDateTimeChooserAndroid,
            int dialogType, double dialogValue,
            double min, double max, double step,
            DateTimeSuggestion[] suggestions) {
        Activity windowAndroidActivity = windowAndroid.getActivity().get();
        if (windowAndroidActivity == null) return null;
        DateTimeChooserAndroid chooser =
                new DateTimeChooserAndroid(windowAndroidActivity, nativeDateTimeChooserAndroid);
        chooser.showDialog(dialogType, dialogValue, min, max, step, suggestions);
        return chooser;
    }

    @CalledByNative
    private static DateTimeSuggestion[] createSuggestionsArray(int size) {
        return new DateTimeSuggestion[size];
    }

    /**
     * @param array DateTimeSuggestion array that should get a new suggestion set.
     * @param index Index in the array where to place a new suggestion.
     * @param value Value of the suggestion.
     * @param localizedValue Localized value of the suggestion.
     * @param label Label of the suggestion.
     */
    @CalledByNative
    private static void setDateTimeSuggestionAt(DateTimeSuggestion[] array, int index,
            double value, String localizedValue, String label) {
        array[index] = new DateTimeSuggestion(value, localizedValue, label);
    }

    @NativeMethods
    interface Natives {
        void replaceDateTime(long nativeDateTimeChooserAndroid, DateTimeChooserAndroid caller,
                double dialogValue);
        void cancelDialog(long nativeDateTimeChooserAndroid, DateTimeChooserAndroid caller);
    }
}
