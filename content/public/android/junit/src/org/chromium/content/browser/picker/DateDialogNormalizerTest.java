// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.widget.DatePicker;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.TimeZone;

/** Tests for DateDialogNormalizer. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DateDialogNormalizerTest {
    private static final long MILLIS_PER_MINUTE = 60 * 1000;
    private static final long MILLIS_PER_HOUR = 60 * 60 * 1000;
    private static final long PST_OFFSET_MILLIS = 8 * MILLIS_PER_HOUR;
    private static final long JAPAN_OFFSET_MILLIS = -9 * MILLIS_PER_HOUR;

    private static final TimeZone GMT_TIME_ZONE = TimeZone.getTimeZone("GMT");
    private static final TimeZone PST_TIME_ZONE = TimeZone.getTimeZone("GMT-08");
    private static final TimeZone JAPAN_TIME_ZONE = TimeZone.getTimeZone("Japan"); // GMT+09

    // Times are milliseconds since the epoch. The time zone is GMT, unless marked as PST or Japan.
    // The time of day is midnight unless marked otherwise. The calendar is Gregorian, unless
    // marked with _JULIAN, which indicates a hybrid Julian/Gregorian calendar.
    private static final long JULY_31_2012 = 1343692800000L;
    private static final long JULY_31_2012_PST = JULY_31_2012 + PST_OFFSET_MILLIS;
    private static final long JULY_31_2012_JAPAN = JULY_31_2012 + JAPAN_OFFSET_MILLIS;
    private static final long JULY_31_2014 = 1406764800000L;
    private static final long JULY_31_2014_1201AM = JULY_31_2014 + 1 * MILLIS_PER_MINUTE;
    private static final long JULY_31_2014_9PM = JULY_31_2014 + 21 * MILLIS_PER_HOUR;
    private static final long JULY_31_2014_PST = JULY_31_2014 + PST_OFFSET_MILLIS;
    private static final long JULY_31_2014_JAPAN = JULY_31_2014 + JAPAN_OFFSET_MILLIS;
    private static final long JULY_31_2017 = 1501459200000L;
    private static final long JULY_31_2017_PST = JULY_31_2017 + PST_OFFSET_MILLIS;
    private static final long JULY_31_2017_JAPAN = JULY_31_2017 + JAPAN_OFFSET_MILLIS;
    private static final long JULY_31_2017_5AM = JULY_31_2017 + 5 * MILLIS_PER_HOUR;
    private static final long JULY_31_2017_1159PM =
            JULY_31_2017 + 23 * MILLIS_PER_HOUR + 59 * MILLIS_PER_MINUTE;
    private static final long JULY_31_2018 = 1532995200000L;
    private static final long JULY_31_2018_PST = JULY_31_2018 + PST_OFFSET_MILLIS;

    private static final long JANUARY_1_0476 = -47146060800000L;
    private static final long JANUARY_1_0476_JULIAN = -47145974400000L;
    private static final long AUGUST_2_1580 = -12288758400000L;
    private static final long AUGUST_2_1580_JULIAN = -12287894400000L;
    private static final long MARCH_15_3456 = 46899993600000L;
    private static final long DECEMBER_31_5000 = 95649033600000L;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
    }

    /**
     * Asserts that after the input* params are passed to DateDialogNormalize.normalize(), that the
     * DatePicker's state matches the output* params.
     */
    private void run(
            TimeZone defaultTimeZone,
            int inputYear,
            int inputMonth,
            int inputDay,
            long inputMinMillis,
            long inputMaxMillis,
            int outputYear,
            int outputMonth,
            int outputDay,
            long outputMinMillis,
            long outputMaxMillis) {
        TimeZone.setDefault(defaultTimeZone);
        DatePicker picker = new DatePicker(mActivity);
        DateDialogNormalizer.normalize(
                picker, null, inputYear, inputMonth, inputDay, inputMinMillis, inputMaxMillis);

        String pickerDate =
                String.format(
                        "%04d-%02d-%02d",
                        picker.getYear(), picker.getMonth() + 1, picker.getDayOfMonth());
        String expectedDate =
                String.format("%04d-%02d-%02d", outputYear, outputMonth + 1, outputDay);
        assertEquals(expectedDate, pickerDate);
        assertEquals(outputMinMillis, picker.getMinDate());
        assertEquals(outputMaxMillis, picker.getMaxDate());
    }

    @Test
    public void testNormalize() {
        TimeZone originalDefaultTimeZone = TimeZone.getDefault();

        // Typical case: value is between min and max.
        run(
                GMT_TIME_ZONE,
                2015,
                5,
                25,
                JULY_31_2012,
                JULY_31_2017,
                2015,
                5,
                25,
                JULY_31_2012,
                JULY_31_2017);
        run(
                PST_TIME_ZONE,
                2015,
                5,
                25,
                JULY_31_2012,
                JULY_31_2017,
                2015,
                5,
                25,
                JULY_31_2012_PST,
                JULY_31_2017_PST);

        // Hours/minutes/seconds on min and max dates should be truncated.
        run(
                PST_TIME_ZONE,
                2015,
                5,
                25,
                JULY_31_2014_1201AM,
                JULY_31_2017_1159PM,
                2015,
                5,
                25,
                JULY_31_2014_PST,
                JULY_31_2017_PST);
        run(
                JAPAN_TIME_ZONE,
                2015,
                5,
                25,
                JULY_31_2014_9PM,
                JULY_31_2017_5AM,
                2015,
                5,
                25,
                JULY_31_2014_JAPAN,
                JULY_31_2017_JAPAN);

        // If max < min, then max should be changed to min.
        run(
                PST_TIME_ZONE,
                2015,
                5,
                25,
                JULY_31_2017,
                JULY_31_2012,
                2017,
                6,
                31,
                JULY_31_2017_PST,
                JULY_31_2017_PST);

        // If the picker date is before min, it should be changed to min.
        run(
                PST_TIME_ZONE,
                2015,
                5,
                25,
                JULY_31_2017,
                JULY_31_2018,
                2017,
                6,
                31,
                JULY_31_2017_PST,
                JULY_31_2018_PST);

        // If the picker date is after max, it should be changed to max.
        run(
                PST_TIME_ZONE,
                2025,
                5,
                25,
                JULY_31_2017,
                JULY_31_2018,
                2018,
                6,
                31,
                JULY_31_2017_PST,
                JULY_31_2018_PST);
        run(
                JAPAN_TIME_ZONE,
                2025,
                5,
                25,
                JULY_31_2012,
                JULY_31_2014,
                2014,
                6,
                31,
                JULY_31_2012_JAPAN,
                JULY_31_2014_JAPAN);

        // Ensure that dates before the Julian/Gregorian changeover in 1582 are treated correctly,
        // as proleptic Gregorian dates, not as Julian dates.
        run(
                GMT_TIME_ZONE,
                1581,
                3,
                4,
                AUGUST_2_1580,
                MARCH_15_3456,
                1581,
                3,
                4,
                AUGUST_2_1580_JULIAN,
                MARCH_15_3456);

        // Ensure time ranges in the distant past and future work correctly.
        run(
                GMT_TIME_ZONE,
                1215,
                5,
                15,
                JANUARY_1_0476,
                AUGUST_2_1580,
                1215,
                5,
                15,
                JANUARY_1_0476_JULIAN,
                AUGUST_2_1580_JULIAN);
        run(
                GMT_TIME_ZONE,
                1608,
                3,
                10,
                JANUARY_1_0476,
                AUGUST_2_1580,
                1580,
                7,
                2,
                JANUARY_1_0476_JULIAN,
                AUGUST_2_1580_JULIAN);
        run(
                GMT_TIME_ZONE,
                14,
                7,
                19,
                JANUARY_1_0476,
                AUGUST_2_1580,
                476,
                0,
                1,
                JANUARY_1_0476_JULIAN,
                AUGUST_2_1580_JULIAN);
        run(
                GMT_TIME_ZONE,
                4444,
                3,
                4,
                MARCH_15_3456,
                DECEMBER_31_5000,
                4444,
                3,
                4,
                MARCH_15_3456,
                DECEMBER_31_5000);
        run(
                GMT_TIME_ZONE,
                2001,
                3,
                4,
                MARCH_15_3456,
                DECEMBER_31_5000,
                3456,
                2,
                15,
                MARCH_15_3456,
                DECEMBER_31_5000);
        run(
                GMT_TIME_ZONE,
                10001,
                3,
                4,
                MARCH_15_3456,
                DECEMBER_31_5000,
                5000,
                11,
                31,
                MARCH_15_3456,
                DECEMBER_31_5000);

        TimeZone.setDefault(originalDefaultTimeZone);
    }
}
