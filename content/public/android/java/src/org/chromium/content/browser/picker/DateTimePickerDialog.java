// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.DatePicker;
import android.widget.DatePicker.OnDateChangedListener;
import android.widget.TimePicker;
import android.widget.TimePicker.OnTimeChangedListener;

import androidx.annotation.VisibleForTesting;

import org.chromium.content.R;

import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.TimeZone;

/**
 * A dialog that allows the user to choose a date and time. Shown for HTML form input elements
 * with type "datetime" or "datetime-local".
 */
public class DateTimePickerDialog extends AlertDialog
        implements OnClickListener, OnDateChangedListener, OnTimeChangedListener {
    private final DatePicker mDatePicker;
    private final TimePicker mTimePicker;
    private final OnDateTimeSetListener mCallBack;

    private final long mMinTimeMillis;
    private final long mMaxTimeMillis;

    /** The callback used to indicate the user is done filling in the date. */
    public interface OnDateTimeSetListener {

        /**
         * @param dateView The DatePicker view associated with this listener.
         * @param timeView The TimePicker view associated with this listener.
         * @param year The year that was set.
         * @param monthOfYear The month that was set (0-11) for compatibility
         *            with {@link java.util.Calendar}.
         * @param dayOfMonth The day of the month that was set.
         * @param hourOfDay The hour that was set.
         * @param minute The minute that was set.
         */
        void onDateTimeSet(
                DatePicker dateView,
                TimePicker timeView,
                int year,
                int monthOfYear,
                int dayOfMonth,
                int hourOfDay,
                int minute);
    }

    /**
     * @param context The context the dialog is to run in.
     * @param callBack How the parent is notified that the date is set.
     * @param year The initial year of the dialog.
     * @param monthOfYear The initial month of the dialog.
     * @param dayOfMonth The initial day of the dialog.
     */
    public DateTimePickerDialog(
            Context context,
            OnDateTimeSetListener callBack,
            int year,
            int monthOfYear,
            int dayOfMonth,
            int hourOfDay,
            int minute,
            boolean is24HourView,
            double min,
            double max) {
        super(context, 0);

        mMinTimeMillis = (long) min;
        mMaxTimeMillis = (long) max;

        mCallBack = callBack;

        setButton(BUTTON_POSITIVE, context.getText(R.string.date_picker_dialog_set), this);
        setButton(
                BUTTON_NEGATIVE, context.getText(android.R.string.cancel), (OnClickListener) null);
        setIcon(0);
        setTitle(context.getText(R.string.date_time_picker_dialog_title));

        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View view = inflater.inflate(R.layout.date_time_picker_dialog, null);
        setView(view);
        mDatePicker = (DatePicker) view.findViewById(R.id.date_picker);
        DateDialogNormalizer.normalize(
                mDatePicker, this, year, monthOfYear, dayOfMonth, mMinTimeMillis, mMaxTimeMillis);

        mTimePicker = (TimePicker) view.findViewById(R.id.time_picker);
        mTimePicker.setIs24HourView(is24HourView);
        setHour(mTimePicker, hourOfDay);
        setMinute(mTimePicker, minute);
        mTimePicker.setOnTimeChangedListener(this);
        onTimeChanged(mTimePicker, getHour(mTimePicker), getMinute(mTimePicker));
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        tryNotifyDateTimeSet();
    }

    private void tryNotifyDateTimeSet() {
        if (mCallBack != null) {
            mDatePicker.clearFocus();
            mTimePicker.clearFocus();
            mCallBack.onDateTimeSet(
                    mDatePicker,
                    mTimePicker,
                    mDatePicker.getYear(),
                    mDatePicker.getMonth(),
                    mDatePicker.getDayOfMonth(),
                    getHour(mTimePicker),
                    getMinute(mTimePicker));
        }
    }

    @Override
    public void onDateChanged(DatePicker view, int year, int month, int day) {
        // Signal a time change so the max/min checks can be applied.
        if (mTimePicker != null) {
            onTimeChanged(mTimePicker, getHour(mTimePicker), getMinute(mTimePicker));
        }
    }

    @Override
    public void onTimeChanged(TimePicker view, int hourOfDay, int minute) {
        onTimeChangedInternal(
                mDatePicker.getYear(),
                mDatePicker.getMonth(),
                mDatePicker.getDayOfMonth(),
                mTimePicker,
                mMinTimeMillis,
                mMaxTimeMillis);
    }

    @VisibleForTesting
    public static void onTimeChangedInternal(
            int year,
            int month,
            int day,
            TimePicker picker,
            long minTimeMillis,
            long maxTimeMillis) {
        // Need to use a calendar object for UTC because we'd like to compare
        // it with minimum/maximum values in UTC.
        Calendar calendar = new GregorianCalendar(TimeZone.getTimeZone("UTC"));
        calendar.clear();
        calendar.set(year, month, day, getHour(picker), getMinute(picker), 0);

        if (calendar.getTimeInMillis() < minTimeMillis) {
            calendar.setTimeInMillis(minTimeMillis);
        } else if (calendar.getTimeInMillis() > maxTimeMillis) {
            calendar.setTimeInMillis(maxTimeMillis);
        }
        setHour(picker, calendar.get(Calendar.HOUR_OF_DAY));
        setMinute(picker, calendar.get(Calendar.MINUTE));
    }

    /**
     * Sets the current date.
     *
     * @param year The date year.
     * @param monthOfYear The date month.
     * @param dayOfMonth The date day of month.
     */
    public void updateDateTime(
            int year, int monthOfYear, int dayOfMonth, int hourOfDay, int minutOfHour) {
        mDatePicker.updateDate(year, monthOfYear, dayOfMonth);
        setHour(mTimePicker, hourOfDay);
        setMinute(mTimePicker, minutOfHour);
    }

    // TODO(newt): delete these deprecated method calls once we support only API 23 and higher.

    @SuppressWarnings("deprecation")
    private static void setHour(TimePicker picker, int hour) {
        picker.setCurrentHour(hour);
    }

    @SuppressWarnings("deprecation")
    private static void setMinute(TimePicker picker, int minute) {
        picker.setCurrentMinute(minute);
    }

    @SuppressWarnings("deprecation")
    private static int getHour(TimePicker picker) {
        return picker.getCurrentHour();
    }

    @SuppressWarnings("deprecation")
    private static int getMinute(TimePicker picker) {
        return picker.getCurrentMinute();
    }
}
