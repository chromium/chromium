// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.util;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class DialogTypeRecorder {
    private static final String HISTOGRAM_NAME = "Android.UIDialogShown";

    // Used for logging histogram of dialog types. Do not change these constants.
    @IntDef({
        DialogType.DATE,
        DialogType.TIME,
        DialogType.DATETIME,
        DialogType.MONTH,
        DialogType.WEEK,
        DialogType.COLOR_PICKER,
        DialogType.SELECT_ELEMENT,
        DialogType.JS_POPUP,
        DialogType.MAX
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DialogType {
        int DATE = 0;
        int TIME = 1;
        int DATETIME = 2;
        int MONTH = 3;
        int WEEK = 4;
        int COLOR_PICKER = 5;
        int SELECT_ELEMENT = 6;
        int JS_POPUP = 7;
        int MAX = 8;
    }

    public static void recordDialogType(@DialogType int type) {
        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM_NAME, type, DialogType.MAX);
    }

    private DialogTypeRecorder() {}
}
