// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.picker.InputDialogContainer;
import org.chromium.ui.base.ime.TextInputType;

/**
 * Unittests for the {@link org.chromium.content.browser.picker.InputDialogContainer} class.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class InputDialogContainerTest {
    // Defined in third_party/WebKit/Source/platform/DateComponents.h
    private static final double DATE_DIALOG_DEFAULT_MIN = -62135596800000.0;
    private static final double DATE_DIALOG_DEFAULT_MAX = 8640000000000000.0;
    private static final double DATETIMELOCAL_DIALOG_DEFAULT_MIN = -62135596800000.0;
    private static final double DATETIMELOCAL_DIALOG_DEFAULT_MAX = 8640000000000000.0;
    private static final double MONTH_DIALOG_DEFAULT_MIN = -23628.0;
    private static final double MONTH_DIALOG_DEFAULT_MAX = 3285488.0;
    private static final double TIME_DIALOG_DEFAULT_MIN = 0.0;
    private static final double TIME_DIALOG_DEFAULT_MAX = 86399999.0;
    private static final double WEEK_DIALOG_DEFAULT_MIN = -62135596800000.0;
    private static final double WEEK_DIALOG_DEFAULT_MAX = 8639999568000000.0;

    private static final double ASSERTION_DELTA = 0;

    InputActionDelegateForTests mInputActionDelegate;
    InputDialogContainerForTests mInputDialogContainer;

    @Before
    public void setUp() {
        mInputActionDelegate = new InputActionDelegateForTests();
        mInputDialogContainer = new InputDialogContainerForTests(
                InstrumentationRegistry.getContext(), mInputActionDelegate);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testDateValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TextInputType.DATE,
                1970, 0, 1,
                0, 0, 0, 0, 0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.DATE, 0.0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.DATE,
                1, 0, 1,
                0, 0, 0, 0, 0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.DATE, -62135596800000.0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.DATE,
                275760, 8, 13,
                0, 0, 0, 0, 0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.DATE, 8640000000000000.0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.DATE,
                2013, 10, 7,
                0, 0, 0, 0, 0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.DATE, 1383782400000.0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testDatetimelocalValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TextInputType.DATE_TIME_LOCAL,
                1970, 0, 1,
                0, 0, 0, 0, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.DATE_TIME_LOCAL, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.DATE_TIME_LOCAL,
                1, 0, 1,
                0, 0, 0, 0, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.DATE_TIME_LOCAL, -62135596800000.0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.DATE_TIME_LOCAL,
                275760, 8, 13,
                0, 0, 0, 0, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.DATE_TIME_LOCAL, 8640000000000000.0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.DATE_TIME_LOCAL,
                2013, 10, 8,
                1, 1, 2, 196, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 0.001);
        mInputDialogContainer.showPickerDialog(TextInputType.DATE_TIME_LOCAL, 1383872462196.0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 0.001);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testMonthValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TextInputType.MONTH,
                1970, 0, 0,
                0, 0, 0, 0, 0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.MONTH, 0.0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.MONTH,
                1, 0, 0,
                0, 0, 0, 0, 0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.MONTH, -23628.0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.MONTH,
                275760, 8, 0,
                0, 0, 0, 0, 0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.MONTH, 3285488.0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.MONTH,
                2013, 10, 0,
                0, 0, 0, 0, 0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.MONTH, 526.0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testTimeValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TextInputType.TIME,
                0, 0, 0,
                0, 0, 0, 0, 0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.TIME, 0.0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);

        // Time dialog only shows the hour and minute fields.
        mInputDialogContainer.setShowDialogExpectation(TextInputType.TIME,
                0, 0, 0,
                23, 59, 0, 0, 0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.TIME, 86399999.0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.TIME,
                0, 0, 0,
                15, 23, 0, 0, 0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.TIME, 55425678.0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testWeekValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TextInputType.WEEK,
                1970, 0, 0,
                0, 0, 0, 0, 1,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.WEEK, -259200000.0,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.WEEK,
                1, 0, 0,
                0, 0, 0, 0, 1,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.WEEK, -62135596800000.0,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.WEEK,
                275760, 0, 0,
                0, 0, 0, 0, 37,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.WEEK, 8639999568000000.0,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TextInputType.WEEK,
                2013, 0, 0,
                0, 0, 0, 0, 44,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TextInputType.WEEK, 1382918400000.0,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testDateValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.DATE,
                1970, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(-62135596800000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.DATE,
                1, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(8640000000000000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.DATE,
                275760, 8, 13,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(1383782400000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.DATE,
                2013, 10, 7,
                0, 0, 0, 0, 0);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testDatetimelocalValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.DATE_TIME_LOCAL,
                1970, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(-62135596800000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.DATE_TIME_LOCAL,
                1, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(8640000000000000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.DATE_TIME_LOCAL,
                275760, 8, 13,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(1383872462196.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.DATE_TIME_LOCAL,
                2013, 10, 8,
                1, 1, 2, 196, 0);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testMonthValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.MONTH,
                1970, 0, 0,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(-62135596800000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.MONTH,
                1, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(8640000000000000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.MONTH,
                275760, 8, 0,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(1383872462196.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.MONTH,
                2013, 10, 0,
                0, 0, 0, 0, 0);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testTimeValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(0.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.TIME,
                0, 0, 0,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(86399999.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.TIME,
                0, 0, 0,
                23, 59, 59, 999, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(55425678.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.TIME,
                2013, 10, 0,
                3, 23, 45, 678, 0);
    }

    @Test
    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testWeekValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(-259200000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.WEEK,
                1970, 0, 0,
                0, 0, 0, 0, 1);

        mInputActionDelegate.setReplaceDateTimeExpectation(-62135596800000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.WEEK,
                1, 0, 0,
                0, 0, 0, 0, 1);

        mInputActionDelegate.setReplaceDateTimeExpectation(8639999568000000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.WEEK,
                275760, 0, 0,
                0, 0, 0, 0, 37);

        mInputActionDelegate.setReplaceDateTimeExpectation(1382918400000.0);
        mInputDialogContainer.setFieldDateTimeValue(TextInputType.WEEK,
                2013, 0, 0,
                0, 0, 0, 0, 44);
    }

    private static class InputActionDelegateForTests
            implements InputDialogContainer.InputActionDelegate {
        private double mExpectedDialogValue;

        public void setReplaceDateTimeExpectation(double dialogValue) {
            mExpectedDialogValue = dialogValue;
        }

        @Override
        public void replaceDateTime(double dialogValue) {
            Assert.assertEquals(mExpectedDialogValue, dialogValue, ASSERTION_DELTA);
        }

        @Override
        public void cancelDateTimeDialog() {
        }
    }

    private static class InputDialogContainerForTests extends InputDialogContainer {
        private int mExpectedDialogType;
        private int mExpectedYear;
        private int mExpectedMonth;
        private int mExpectedMonthDay;
        private int mExpectedHourOfDay;
        private int mExpectedMinute;
        private int mExpectedSecond;
        private int mExpectedMillis;
        private int mExpectedWeek;
        private double mExpectedMin;
        private double mExpectedMax;
        private double mExpectedStep;

        public InputDialogContainerForTests(
                Context context,
                InputDialogContainer.InputActionDelegate inputActionDelegate) {
            super(context, inputActionDelegate);
        }

        void setShowDialogExpectation(int dialogType,
                int year, int month, int monthDay,
                int hourOfDay, int minute, int second, int millis, int week,
                double min, double max, double step) {
            mExpectedDialogType = dialogType;
            mExpectedYear = year;
            mExpectedMonth = month;
            mExpectedMonthDay = monthDay;
            mExpectedHourOfDay = hourOfDay;
            mExpectedMinute = minute;
            mExpectedSecond = second;
            mExpectedMillis = millis;
            mExpectedWeek = week;
            mExpectedMin = min;
            mExpectedMax = max;
            mExpectedStep = step;
        }

        @Override
        public void showPickerDialog(
                final int dialogType, double dialogValue, double min, double max, double step) {
            super.showPickerDialog(dialogType, dialogValue, min, max, step);
        }

        @Override
        protected void showPickerDialog(final int dialogType,
                int year, int month, int monthDay,
                int hourOfDay, int minute, int second, int millis, int week,
                double min, double max, double step) {
            Assert.assertEquals(mExpectedDialogType, dialogType);
            Assert.assertEquals(mExpectedYear, year);
            Assert.assertEquals(mExpectedMonth, month);
            Assert.assertEquals(mExpectedMonthDay, monthDay);
            Assert.assertEquals(mExpectedHourOfDay, hourOfDay);
            Assert.assertEquals(mExpectedMinute, minute);
            Assert.assertEquals(mExpectedSecond, second);
            Assert.assertEquals(mExpectedMillis, millis);
            Assert.assertEquals(mExpectedWeek, week);
            Assert.assertEquals(mExpectedMin, min, ASSERTION_DELTA);
            Assert.assertEquals(mExpectedMax, max, ASSERTION_DELTA);
            Assert.assertEquals(mExpectedStep, step, ASSERTION_DELTA);
        }

        @Override
        public void setFieldDateTimeValue(int dialogType,
                                       int year, int month, int monthDay,
                                       int hourOfDay, int minute, int second, int millis,
                                       int week) {
            super.setFieldDateTimeValue(dialogType,
                                        year, month, monthDay,
                                        hourOfDay, minute, second, millis,
                                        week);
        }
    }
}
