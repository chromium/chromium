// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import android.app.AlertDialog;
import android.app.DatePickerDialog.OnDateSetListener;
import android.app.TimePickerDialog;
import android.app.TimePickerDialog.OnTimeSetListener;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.text.format.DateFormat;
import android.view.View;
import android.widget.AdapterView;
import android.widget.DatePicker;
import android.widget.ListView;
import android.widget.TimePicker;

import org.chromium.base.Log;
import org.chromium.content.R;
import org.chromium.content.browser.picker.DateTimePickerDialog.OnDateTimeSetListener;
import org.chromium.content.browser.picker.MultiFieldTimePickerDialog.OnMultiFieldTimeSetListener;
import org.chromium.content_public.browser.util.DialogTypeRecorder;
import org.chromium.ui.base.ime.TextInputType;

import java.util.Arrays;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.TimeZone;
import java.util.concurrent.TimeUnit;

/** Opens the appropriate date/time picker dialog for the given dialog type. */
public class InputDialogContainer {
    private static final String TAG = "InputDialogContainer";

    /** Delegate that implements the picker's actions. */
    public interface InputActionDelegate {
        void cancelDateTimeDialog();

        void replaceDateTime(double value);
    }

    private final Context mContext;

    // Prevents sending two notifications (from onClick and from onDismiss)
    private boolean mDialogAlreadyDismissed;

    private AlertDialog mDialog;
    private final InputActionDelegate mInputActionDelegate;

    public static boolean isDialogInputType(int type) {
        return type == TextInputType.DATE
                || type == TextInputType.TIME
                || type == TextInputType.DATE_TIME
                || type == TextInputType.DATE_TIME_LOCAL
                || type == TextInputType.MONTH
                || type == TextInputType.WEEK;
    }

    public InputDialogContainer(Context context, InputActionDelegate inputActionDelegate) {
        mContext = context;
        mInputActionDelegate = inputActionDelegate;
    }

    protected void showPickerDialog(
            final int dialogType, double dialogValue, double min, double max, double step) {
        Calendar cal;
        // |dialogValue|, |min|, |max| mean different things depending on the |dialogType|.
        // For input type=month is the number of months since 1970.
        // For input type=time it is milliseconds since midnight.
        // For other types they are just milliseconds since 1970.
        // If |dialogValue| is NaN it means an empty value. We will show the current time.
        if (Double.isNaN(dialogValue)) {
            cal = Calendar.getInstance();
            cal.set(Calendar.MILLISECOND, 0);
        } else {
            if (dialogType == TextInputType.MONTH) {
                cal = MonthPicker.createDateFromValue(dialogValue);
            } else if (dialogType == TextInputType.WEEK) {
                cal = WeekPicker.createDateFromValue(dialogValue);
            } else {
                GregorianCalendar gregorianCalendar =
                        new GregorianCalendar(TimeZone.getTimeZone("UTC"));
                // According to the HTML spec we only use the Gregorian calendar
                // so we ignore the Julian/Gregorian transition.
                gregorianCalendar.setGregorianChange(new Date(Long.MIN_VALUE));
                gregorianCalendar.setTimeInMillis((long) dialogValue);
                cal = gregorianCalendar;
            }
        }
        if (dialogType == TextInputType.DATE) {
            showPickerDialog(
                    dialogType,
                    cal.get(Calendar.YEAR),
                    cal.get(Calendar.MONTH),
                    cal.get(Calendar.DAY_OF_MONTH),
                    0,
                    0,
                    0,
                    0,
                    0,
                    min,
                    max,
                    step);
            DialogTypeRecorder.recordDialogType(DialogTypeRecorder.DialogType.DATE);
        } else if (dialogType == TextInputType.TIME) {
            showPickerDialog(
                    dialogType,
                    0,
                    0,
                    0,
                    cal.get(Calendar.HOUR_OF_DAY),
                    cal.get(Calendar.MINUTE),
                    0,
                    0,
                    0,
                    min,
                    max,
                    step);
            DialogTypeRecorder.recordDialogType(DialogTypeRecorder.DialogType.TIME);
        } else if (dialogType == TextInputType.DATE_TIME
                || dialogType == TextInputType.DATE_TIME_LOCAL) {
            showPickerDialog(
                    dialogType,
                    cal.get(Calendar.YEAR),
                    cal.get(Calendar.MONTH),
                    cal.get(Calendar.DAY_OF_MONTH),
                    cal.get(Calendar.HOUR_OF_DAY),
                    cal.get(Calendar.MINUTE),
                    cal.get(Calendar.SECOND),
                    cal.get(Calendar.MILLISECOND),
                    0,
                    min,
                    max,
                    step);
            DialogTypeRecorder.recordDialogType(DialogTypeRecorder.DialogType.DATETIME);
        } else if (dialogType == TextInputType.MONTH) {
            showPickerDialog(
                    dialogType,
                    cal.get(Calendar.YEAR),
                    cal.get(Calendar.MONTH),
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    min,
                    max,
                    step);
            DialogTypeRecorder.recordDialogType(DialogTypeRecorder.DialogType.MONTH);
        } else if (dialogType == TextInputType.WEEK) {
            int year = WeekPicker.getISOWeekYearForDate(cal);
            int week = WeekPicker.getWeekForDate(cal);
            showPickerDialog(dialogType, year, 0, 0, 0, 0, 0, 0, week, min, max, step);
            DialogTypeRecorder.recordDialogType(DialogTypeRecorder.DialogType.WEEK);
        }
    }

    private void showSuggestionDialog(
            final int dialogType,
            final double dialogValue,
            final double min,
            final double max,
            final double step,
            DateTimeSuggestion[] suggestions) {
        ListView suggestionListView = new ListView(mContext);
        final DateTimeSuggestionListAdapter adapter =
                new DateTimeSuggestionListAdapter(mContext, Arrays.asList(suggestions));
        suggestionListView.setAdapter(adapter);
        suggestionListView.setOnItemClickListener(
                new AdapterView.OnItemClickListener() {
                    @Override
                    public void onItemClick(
                            AdapterView<?> parent, View view, int position, long id) {
                        if (position == adapter.getCount() - 1) {
                            dismissDialog();
                            showPickerDialog(dialogType, dialogValue, min, max, step);
                        } else {
                            double suggestionValue = adapter.getItem(position).value();
                            mInputActionDelegate.replaceDateTime(suggestionValue);
                            dismissDialog();
                            mDialogAlreadyDismissed = true;
                        }
                    }
                });

        int dialogTitleId = R.string.date_picker_dialog_title;
        if (dialogType == TextInputType.TIME) {
            dialogTitleId = R.string.time_picker_dialog_title;
        } else if (dialogType == TextInputType.DATE_TIME
                || dialogType == TextInputType.DATE_TIME_LOCAL) {
            dialogTitleId = R.string.date_time_picker_dialog_title;
        } else if (dialogType == TextInputType.MONTH) {
            dialogTitleId = R.string.month_picker_dialog_title;
        } else if (dialogType == TextInputType.WEEK) {
            dialogTitleId = R.string.week_picker_dialog_title;
        }

        mDialog =
                new AlertDialog.Builder(mContext)
                        .setTitle(dialogTitleId)
                        .setView(suggestionListView)
                        .setNegativeButton(
                                mContext.getText(android.R.string.cancel),
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        dismissDialog();
                                    }
                                })
                        .create();

        mDialog.setOnDismissListener(
                new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(DialogInterface dialog) {
                        if (mDialog == dialog && !mDialogAlreadyDismissed) {
                            mDialogAlreadyDismissed = true;
                            mInputActionDelegate.cancelDateTimeDialog();
                        }
                    }
                });
        mDialogAlreadyDismissed = false;
        mDialog.show();
    }

    public void showDialog(
            final int type,
            final double value,
            double min,
            double max,
            double step,
            DateTimeSuggestion[] suggestions) {
        // When the web page asks to show a dialog while there is one already open,
        // dismiss the old one.
        dismissDialog();
        if (suggestions == null) {
            showPickerDialog(type, value, min, max, step);
        } else {
            showSuggestionDialog(type, value, min, max, step, suggestions);
        }
    }

    protected void showPickerDialog(
            final int dialogType,
            int year,
            int month,
            int monthDay,
            int hourOfDay,
            int minute,
            int second,
            int millis,
            int week,
            double min,
            double max,
            double step) {
        dismissDialog();

        int stepTime = (int) step;

        if (dialogType == TextInputType.DATE) {
            DatePickerDialogCompat dialog =
                    new DatePickerDialogCompat(
                            mContext, new DateListener(dialogType), year, month, monthDay);
            DateDialogNormalizer.normalize(
                    dialog.getDatePicker(), dialog, year, month, monthDay, (long) min, (long) max);

            dialog.setTitle(mContext.getText(R.string.date_picker_dialog_title));
            mDialog = dialog;
        } else if (dialogType == TextInputType.TIME) {
            // If user doesn't need to set seconds and milliseconds, show the default clock style
            // time picker dialog. Otherwise, show a full spinner style time picker.
            if (stepTime < 0 || stepTime >= 60000 /* milliseconds in a minute */) {
                mDialog =
                        new TimePickerDialog(
                                mContext,
                                new TimeListener(dialogType),
                                hourOfDay,
                                minute,
                                DateFormat.is24HourFormat(mContext));
            } else {
                mDialog =
                        new MultiFieldTimePickerDialog(
                                mContext,
                                /* theme= */ 0,
                                hourOfDay,
                                minute,
                                second,
                                millis,
                                (int) min,
                                (int) max,
                                stepTime,
                                DateFormat.is24HourFormat(mContext),
                                new FullTimeListener(dialogType));
            }
        } else if (dialogType == TextInputType.DATE_TIME
                || dialogType == TextInputType.DATE_TIME_LOCAL) {
            mDialog =
                    new DateTimePickerDialog(
                            mContext,
                            new DateTimeListener(dialogType),
                            year,
                            month,
                            monthDay,
                            hourOfDay,
                            minute,
                            DateFormat.is24HourFormat(mContext),
                            min,
                            max);
        } else if (dialogType == TextInputType.MONTH) {
            mDialog =
                    new MonthPickerDialog(
                            mContext, new MonthOrWeekListener(dialogType), year, month, min, max);
        } else if (dialogType == TextInputType.WEEK) {
            mDialog =
                    new WeekPickerDialog(
                            mContext, new MonthOrWeekListener(dialogType), year, week, min, max);
        }

        mDialog.setButton(
                DialogInterface.BUTTON_POSITIVE,
                mContext.getText(R.string.date_picker_dialog_set),
                (DialogInterface.OnClickListener) mDialog);

        mDialog.setButton(
                DialogInterface.BUTTON_NEGATIVE,
                mContext.getText(android.R.string.cancel),
                (DialogInterface.OnClickListener) null);

        mDialog.setButton(
                DialogInterface.BUTTON_NEUTRAL,
                mContext.getText(R.string.date_picker_dialog_clear),
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mDialogAlreadyDismissed = true;
                        mInputActionDelegate.replaceDateTime(Double.NaN);
                    }
                });

        mDialog.setOnDismissListener(
                new OnDismissListener() {
                    @Override
                    public void onDismiss(final DialogInterface dialog) {
                        if (!mDialogAlreadyDismissed) {
                            mDialogAlreadyDismissed = true;
                            mInputActionDelegate.cancelDateTimeDialog();
                        }
                    }
                });

        mDialogAlreadyDismissed = false;
        mDialog.show();
    }

    private boolean isDialogShowing() {
        return mDialog != null && mDialog.isShowing();
    }

    public void dismissDialog() {
        if (!isDialogShowing()) return;
        try {
            mDialog.dismiss();
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "Ignoring exception from dialog.dismiss", e);
        }
    }

    private class DateListener implements OnDateSetListener {
        private final int mDialogType;

        DateListener(int dialogType) {
            mDialogType = dialogType;
        }

        @Override
        public void onDateSet(DatePicker view, int year, int month, int monthDay) {
            setFieldDateTimeValue(mDialogType, year, month, monthDay, 0, 0, 0, 0, 0);
        }
    }

    private class TimeListener implements OnTimeSetListener {
        private final int mDialogType;

        TimeListener(int dialogType) {
            mDialogType = dialogType;
        }

        @Override
        public void onTimeSet(TimePicker view, int hourOfDay, int minute) {
            setFieldDateTimeValue(mDialogType, 0, 0, 0, hourOfDay, minute, 0, 0, 0);
        }
    }

    private class FullTimeListener implements OnMultiFieldTimeSetListener {
        private final int mDialogType;

        FullTimeListener(int dialogType) {
            mDialogType = dialogType;
        }

        @Override
        public void onTimeSet(int hourOfDay, int minute, int second, int milli) {
            setFieldDateTimeValue(mDialogType, 0, 0, 0, hourOfDay, minute, second, milli, 0);
        }
    }

    private class DateTimeListener implements OnDateTimeSetListener {
        private final int mDialogType;

        public DateTimeListener(int dialogType) {
            mDialogType = dialogType;
        }

        @Override
        public void onDateTimeSet(
                DatePicker dateView,
                TimePicker timeView,
                int year,
                int month,
                int monthDay,
                int hourOfDay,
                int minute) {
            setFieldDateTimeValue(mDialogType, year, month, monthDay, hourOfDay, minute, 0, 0, 0);
        }
    }

    private class MonthOrWeekListener implements TwoFieldDatePickerDialog.OnValueSetListener {
        private final int mDialogType;

        MonthOrWeekListener(int dialogType) {
            mDialogType = dialogType;
        }

        @Override
        public void onValueSet(int year, int positionInYear) {
            if (mDialogType == TextInputType.MONTH) {
                setFieldDateTimeValue(mDialogType, year, positionInYear, 0, 0, 0, 0, 0, 0);
            } else {
                setFieldDateTimeValue(mDialogType, year, 0, 0, 0, 0, 0, 0, positionInYear);
            }
        }
    }

    protected void setFieldDateTimeValue(
            int dialogType,
            int year,
            int month,
            int monthDay,
            int hourOfDay,
            int minute,
            int second,
            int millis,
            int week) {
        // Prevents more than one callback being sent to the native
        // side when the dialog triggers multiple events.
        if (mDialogAlreadyDismissed) return;
        mDialogAlreadyDismissed = true;

        if (dialogType == TextInputType.MONTH) {
            mInputActionDelegate.replaceDateTime((year - 1970) * 12 + month);
        } else if (dialogType == TextInputType.WEEK) {
            mInputActionDelegate.replaceDateTime(
                    (double) WeekPicker.createDateFromWeek(year, week).getTimeInMillis());
        } else if (dialogType == TextInputType.TIME) {
            mInputActionDelegate.replaceDateTime(
                    (double)
                            (TimeUnit.HOURS.toMillis(hourOfDay)
                                    + TimeUnit.MINUTES.toMillis(minute)
                                    + TimeUnit.SECONDS.toMillis(second)
                                    + millis));
        } else {
            Calendar cal = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
            cal.clear();
            cal.set(Calendar.YEAR, year);
            cal.set(Calendar.MONTH, month);
            cal.set(Calendar.DAY_OF_MONTH, monthDay);
            cal.set(Calendar.HOUR_OF_DAY, hourOfDay);
            cal.set(Calendar.MINUTE, minute);
            cal.set(Calendar.SECOND, second);
            cal.set(Calendar.MILLISECOND, millis);
            mInputActionDelegate.replaceDateTime((double) cal.getTimeInMillis());
        }
    }
}
