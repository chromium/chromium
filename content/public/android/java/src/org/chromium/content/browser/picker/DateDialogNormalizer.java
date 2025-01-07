// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import android.widget.DatePicker;
import android.widget.DatePicker.OnDateChangedListener;

import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.TimeZone;

/** Sets the current, min, and max values on the given DatePicker. */
public class DateDialogNormalizer {

    /**
     * Stores a date (year-month-day) and the number of milliseconds corresponding to that date
     * according to the DatePicker's calendar.
     */
    private static class DateAndMillis {
        /**
         * Number of milliseconds from the epoch (1970-01-01) to the beginning of year-month-day
         * in the default time zone (TimeZone.getDefault()) using the Julian/Gregorian split
         * calendar. This value is interopable with {@link DatePicker#getMinDate} and
         * {@link DatePicker#setMinDate}.
         */
        public final long millisForPicker;

        public final int year;
        public final int month; // 0-based
        public final int day;

        DateAndMillis(long millisForPicker, int year, int month, int day) {
            this.millisForPicker = millisForPicker;
            this.year = year;
            this.month = month;
            this.day = day;
        }

        static DateAndMillis create(long millisUtc) {
            // millisUtc uses the Gregorian calendar only, so disable the Julian changeover date.
            GregorianCalendar utcCal = new GregorianCalendar(TimeZone.getTimeZone("UTC"));
            utcCal.setGregorianChange(new Date(Long.MIN_VALUE));
            utcCal.setTimeInMillis(millisUtc);
            int year = utcCal.get(Calendar.YEAR);
            int month = utcCal.get(Calendar.MONTH);
            int day = utcCal.get(Calendar.DAY_OF_MONTH);
            return create(year, month, day);
        }

        static DateAndMillis create(int year, int month, int day) {
            // By contrast, millisForPicker uses the default Gregorian/Julian changeover date.
            Calendar defaultTimeZoneCal = Calendar.getInstance(TimeZone.getDefault());
            defaultTimeZoneCal.clear();
            defaultTimeZoneCal.set(year, month, day);
            long millisForPicker = defaultTimeZoneCal.getTimeInMillis();
            return new DateAndMillis(millisForPicker, year, month, day);
        }
    }

    private static void setLimits(
            DatePicker picker, long minMillisForPicker, long maxMillisForPicker) {
        // On KitKat and earlier, DatePicker requires the minDate is always less than maxDate, even
        // during the process of setting those values (eek), so set them in an order that preserves
        // this invariant throughout.
        if (minMillisForPicker > picker.getMaxDate()) {
            picker.setMaxDate(maxMillisForPicker);
            picker.setMinDate(minMillisForPicker);
        } else {
            picker.setMinDate(minMillisForPicker);
            picker.setMaxDate(maxMillisForPicker);
        }
    }

    /**
     * Sets the current, min, and max values on the given DatePicker and ensures that
     * min <= current <= max, adjusting current and max if needed.
     *
     * @param year The current year to set.
     * @param month The current month to set. 0-based.
     * @param day The current day to set.
     * @param minMillisUtc The minimum allowed date, in milliseconds from the epoch according to a
     *                     proleptic Gregorian calendar (no Julian switch).
     * @param maxMillisUtc The maximum allowed date, in milliseconds from the epoch according to a
     *                     proleptic Gregorian calendar (no Julian switch).
     */
    public static void normalize(
            DatePicker picker,
            final OnDateChangedListener listener,
            int year,
            int month,
            int day,
            long minMillisUtc,
            long maxMillisUtc) {
        DateAndMillis currentDate = DateAndMillis.create(year, month, day);
        DateAndMillis minDate = DateAndMillis.create(minMillisUtc);
        DateAndMillis maxDate = DateAndMillis.create(maxMillisUtc);

        // Ensure min <= current <= max, adjusting current and max if needed.
        if (maxDate.millisForPicker < minDate.millisForPicker) {
            maxDate = minDate;
        }
        if (currentDate.millisForPicker < minDate.millisForPicker) {
            currentDate = minDate;
        } else if (currentDate.millisForPicker > maxDate.millisForPicker) {
            currentDate = maxDate;
        }

        setLimits(picker, minDate.millisForPicker, maxDate.millisForPicker);
        picker.init(currentDate.year, currentDate.month, currentDate.day, listener);
    }
}
