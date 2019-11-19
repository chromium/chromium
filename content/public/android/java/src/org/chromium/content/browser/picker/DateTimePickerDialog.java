// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import android.app.AlertDialog;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Build;
import android.os.LocaleList;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.DatePicker;
import android.widget.DatePicker.OnDateChangedListener;
import android.widget.TimePicker;
import android.widget.TimePicker.OnTimeChangedListener;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.content.R;

import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.IllegalFormatConversionException;
import java.util.Locale;
import java.util.TimeZone;
import java.util.regex.Pattern;

import javax.annotation.CheckForNull;

/**
 * A dialog that allows the user to choose a date and time. Shown for HTML form input elements
 * with type "datetime" or "datetime-local".
 */
public class DateTimePickerDialog extends AlertDialog implements OnClickListener,
        OnDateChangedListener, OnTimeChangedListener {
    private final DatePicker mDatePicker;
    private final TimePicker mTimePicker;
    private final OnDateTimeSetListener mCallBack;

    private final long mMinTimeMillis;
    private final long mMaxTimeMillis;

    /**
     * The callback used to indicate the user is done filling in the date.
     */
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
        void onDateTimeSet(DatePicker dateView, TimePicker timeView, int year, int monthOfYear,
                int dayOfMonth, int hourOfDay, int minute);
    }

    /**
     * @param context The context the dialog is to run in.
     * @param callBack How the parent is notified that the date is set.
     * @param year The initial year of the dialog.
     * @param monthOfYear The initial month of the dialog.
     * @param dayOfMonth The initial day of the dialog.
     */
    public DateTimePickerDialog(Context context,
            OnDateTimeSetListener callBack,
            int year,
            int monthOfYear,
            int dayOfMonth,
            int hourOfDay, int minute, boolean is24HourView,
            double min, double max) {
        super(context, 0);

        mMinTimeMillis = (long) min;
        mMaxTimeMillis = (long) max;

        mCallBack = callBack;

        setButton(BUTTON_POSITIVE, context.getText(
                R.string.date_picker_dialog_set), this);
        setButton(BUTTON_NEGATIVE, context.getText(android.R.string.cancel),
                (OnClickListener) null);
        setIcon(0);
        setTitle(context.getText(R.string.date_time_picker_dialog_title));

        Context dialogContext = getDialogContext(context);
        LayoutInflater inflater =
                (LayoutInflater) dialogContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View view = inflater.inflate(R.layout.date_time_picker_dialog, null);
        setView(view);
        mDatePicker = (DatePicker) view.findViewById(R.id.date_picker);
        DateDialogNormalizer.normalize(mDatePicker, this,
                year, monthOfYear, dayOfMonth, mMinTimeMillis, mMaxTimeMillis);

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
            mCallBack.onDateTimeSet(mDatePicker, mTimePicker, mDatePicker.getYear(),
                    mDatePicker.getMonth(), mDatePicker.getDayOfMonth(),
                    getHour(mTimePicker), getMinute(mTimePicker));
        }
    }

    @Override
    public void onDateChanged(DatePicker view, int year,
            int month, int day) {
        // Signal a time change so the max/min checks can be applied.
        if (mTimePicker != null) {
            onTimeChanged(mTimePicker, getHour(mTimePicker), getMinute(mTimePicker));
        }
    }

    @Override
    public void onTimeChanged(TimePicker view, int hourOfDay, int minute) {
        onTimeChangedInternal(mDatePicker.getYear(), mDatePicker.getMonth(),
                mDatePicker.getDayOfMonth(), mTimePicker, mMinTimeMillis, mMaxTimeMillis);
    }

    @VisibleForTesting
    public static void onTimeChangedInternal(int year, int month, int day, TimePicker picker,
            long minTimeMillis, long maxTimeMillis) {
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
    public void updateDateTime(int year, int monthOfYear, int dayOfMonth,
            int hourOfDay, int minutOfHour) {
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

    /**
     * Wraps context with {@link WorkaroundContextForSamsungLDateTimeBug} instance if needed.
     */
    private static Context getDialogContext(Context context) {
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP
                || Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP_MR1) {
            return new WorkaroundContextForSamsungLDateTimeBug(context);
        }

        return context;
    }

    /**
     * Workaround for Samsung Lollipop devices that may crash due to wrong string resource supplied
     * to {@link android.widget.SimpleMonthView}'s content description.
     */
    private static class WorkaroundContextForSamsungLDateTimeBug extends ContextWrapper {
        @CheckForNull
        private Resources mWrappedResources;

        private WorkaroundContextForSamsungLDateTimeBug(Context context) {
            super(context);
        }

        @Override
        public Resources getResources() {
            if (mWrappedResources == null) {
                Resources r = super.getResources();
                mWrappedResources = new WrappedResources(
                        r.getAssets(), r.getDisplayMetrics(), r.getConfiguration()) {};
            }
            return mWrappedResources;
        }

        private static class WrappedResources extends Resources {
            @SuppressWarnings("deprecation")
            WrappedResources(AssetManager assets, DisplayMetrics displayMetrics,
                    Configuration configuration) {
                // The Resources constructor is safe to use on L & L_MR1
                super(assets, displayMetrics, configuration);
            }

            @NonNull
            @Override
            public String getString(int id, Object... formatArgs) throws NotFoundException {
                try {
                    return super.getString(id, formatArgs);
                } catch (IllegalFormatConversionException conversationException) {
                    String template = super.getString(id);
                    char conversion = conversationException.getConversion();
                    // Trying to replace either all digit patterns (%d) or first one (%1$d).
                    template = template.replaceAll(Pattern.quote("%" + conversion), "%s")
                                       .replaceAll(Pattern.quote("%1$" + conversion), "%s");

                    return String.format(getLocale(), template, formatArgs);
                }
            }

            private Locale getLocale() {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    LocaleList locales = getConfiguration().getLocales();
                    if (locales.size() > 0) {
                        return locales.get(0);
                    }
                }
                @SuppressWarnings("deprecation")
                Locale locale = getConfiguration().locale;
                return locale;
            }
        }
    }
}
