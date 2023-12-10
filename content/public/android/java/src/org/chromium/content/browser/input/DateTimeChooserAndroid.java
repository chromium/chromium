// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.content.browser.picker.DateTimeSuggestion;
import org.chromium.content.browser.picker.InputDialogContainer;
import org.chromium.ui.base.WindowAndroid;

/** Plumbing for the different date/time dialog adapters. */
@JNINamespace("content")
class DateTimeChooserAndroid {
    private long mNativeDateTimeChooserAndroid;
    private final InputDialogContainer mInputDialogContainer;

    private DateTimeChooserAndroid(Context context, long nativeDateTimeChooserAndroid) {
        mNativeDateTimeChooserAndroid = nativeDateTimeChooserAndroid;
        mInputDialogContainer =
                new InputDialogContainer(
                        context,
                        new InputDialogContainer.InputActionDelegate() {
                            @Override
                            public void replaceDateTime(double value) {
                                if (mNativeDateTimeChooserAndroid == 0) {
                                    return;
                                }
                                DateTimeChooserAndroidJni.get()
                                        .replaceDateTime(
                                                mNativeDateTimeChooserAndroid,
                                                DateTimeChooserAndroid.this,
                                                value);
                            }

                            @Override
                            public void cancelDateTimeDialog() {
                                if (mNativeDateTimeChooserAndroid == 0) {
                                    return;
                                }
                                DateTimeChooserAndroidJni.get()
                                        .cancelDialog(
                                                mNativeDateTimeChooserAndroid,
                                                DateTimeChooserAndroid.this);
                            }
                        });
    }

    private void showDialog(
            int dialogType,
            double dialogValue,
            double min,
            double max,
            double step,
            DateTimeSuggestion[] suggestions) {
        mInputDialogContainer.showDialog(dialogType, dialogValue, min, max, step, suggestions);
    }

    @CalledByNative
    private void dismissAndDestroy() {
        mNativeDateTimeChooserAndroid = 0;
        mInputDialogContainer.dismissDialog();
    }

    @CalledByNative
    private static DateTimeChooserAndroid createDateTimeChooser(
            WindowAndroid windowAndroid,
            long nativeDateTimeChooserAndroid,
            int dialogType,
            double dialogValue,
            double min,
            double max,
            double step,
            DateTimeSuggestion[] suggestions) {
        Context windowAndroidContext = windowAndroid.getContext().get();
        if (windowAndroidContext == null
                || ContextUtils.activityFromContext(windowAndroidContext) == null) {
            return null;
        }
        DateTimeChooserAndroid chooser =
                new DateTimeChooserAndroid(windowAndroidContext, nativeDateTimeChooserAndroid);
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
    private static void setDateTimeSuggestionAt(
            DateTimeSuggestion[] array,
            int index,
            double value,
            String localizedValue,
            String label) {
        array[index] = new DateTimeSuggestion(value, localizedValue, label);
    }

    @NativeMethods
    interface Natives {
        void replaceDateTime(
                long nativeDateTimeChooserAndroid,
                DateTimeChooserAndroid caller,
                double dialogValue);

        void cancelDialog(long nativeDateTimeChooserAndroid, DateTimeChooserAndroid caller);
    }
}
