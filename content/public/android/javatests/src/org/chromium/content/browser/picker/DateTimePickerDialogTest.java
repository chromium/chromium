// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import android.widget.TimePicker;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

/** Tests for DateTimePickerDialog. */
@RunWith(BaseJUnit4ClassRunner.class)
public class DateTimePickerDialogTest {
    // TODO(tkent): fix deprecation warnings crbug.com/537037
    @Test
    @SuppressWarnings("deprecation")
    @SmallTest
    public void testOnTimeChanged() {
        int september = 8;
        TimePicker picker = new TimePicker(InstrumentationRegistry.getContext());
        // 2015-09-16 00:00 UTC
        long min = 1442361600000L;
        // 2015-09-17 00:00 UTC
        long max = 1442448000000L;

        // Test a value near to the minimum.
        picker.setCurrentHour(1);
        picker.setCurrentMinute(30);
        DateTimePickerDialog.onTimeChangedInternal(2015, september, 16, picker, min, max);
        Assert.assertEquals(1, picker.getCurrentHour().intValue());
        Assert.assertEquals(30, picker.getCurrentMinute().intValue());

        // Test a value near to the maximum.
        picker.setCurrentHour(22);
        picker.setCurrentMinute(56);
        DateTimePickerDialog.onTimeChangedInternal(2015, september, 16, picker, min, max);
        Assert.assertEquals(22, picker.getCurrentHour().intValue());
        Assert.assertEquals(56, picker.getCurrentMinute().intValue());

        // Clamping.
        picker.setCurrentHour(23);
        picker.setCurrentMinute(56);
        // 2015-09-16 23:30 UTC
        max = 1442446200000L;
        DateTimePickerDialog.onTimeChangedInternal(2015, september, 16, picker, min, max);
        Assert.assertEquals(23, picker.getCurrentHour().intValue());
        Assert.assertEquals(30, picker.getCurrentMinute().intValue());
    }
}
