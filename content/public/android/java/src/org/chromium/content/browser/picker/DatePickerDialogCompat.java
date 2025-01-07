// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import android.app.DatePickerDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.widget.DatePicker;

/**
 * The behavior of the DatePickerDialog changed after JellyBean so it now calls
 * OndateSetListener.onDateSet() even when the dialog is dismissed (e.g. back button, tap
 * outside). This class will call the listener instead of the DatePickerDialog only when the
 * BUTTON_POSITIVE has been clicked.
 */
class DatePickerDialogCompat extends DatePickerDialog {
    private final OnDateSetListener mCallBack;

    public DatePickerDialogCompat(
            Context context,
            OnDateSetListener callBack,
            int year,
            int monthOfYear,
            int dayOfMonth) {
        super(context, callBack, year, monthOfYear, dayOfMonth);

        mCallBack = callBack;
    }

    /**
     * The superclass DatePickerDialog has null for OnDateSetListener so we need to call the
     * listener manually.
     */
    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == BUTTON_POSITIVE && mCallBack != null) {
            DatePicker datePicker = getDatePicker();
            datePicker.clearFocus();
            mCallBack.onDateSet(
                    datePicker,
                    datePicker.getYear(),
                    datePicker.getMonth(),
                    datePicker.getDayOfMonth());
        }
    }

    @Override
    public void setTitle(CharSequence title) {
        // On Android L+, the dialog shouldn't have a title. This works around a bug in
        // DatePickerDialog where calling updateDate() before the dialog has been shown causes
        // a title to appear. http://crbug.com/541350
        title = "";
        super.setTitle(title);
    }
}
