// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import android.app.Activity;
import android.content.Context;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintJob;
import android.print.PrintJobInfo;
import android.print.PrintManager;
import android.text.TextUtils;

import org.chromium.base.Log;

import java.util.List;

/** An implementation of {@link PrintManagerDelegate} using the Android framework print manager. */
public class PrintManagerDelegateImpl implements PrintManagerDelegate {
    private static final String TAG = "printing";
    private final PrintManager mPrintManager;

    public PrintManagerDelegateImpl(Activity activity) {
        mPrintManager = (PrintManager) activity.getSystemService(Context.PRINT_SERVICE);
    }

    @Override
    public void print(
            String printJobName, PrintDocumentAdapter documentAdapter, PrintAttributes attributes) {
        dumpJobStatesForDebug();
        mPrintManager.print(printJobName, documentAdapter, attributes);
    }

    private void dumpJobStatesForDebug() {
        if (!Log.isLoggable(TAG, Log.VERBOSE)) return;

        List<PrintJob> printJobs = mPrintManager.getPrintJobs();
        String[] states = new String[printJobs.size()];

        for (int i = 0; i < printJobs.size(); i++) {
            String stateString;
            switch (printJobs.get(i).getInfo().getState()) {
                case PrintJobInfo.STATE_CREATED:
                    stateString = "STATE_CREATED";
                    break;
                case PrintJobInfo.STATE_QUEUED:
                    stateString = "STATE_QUEUED";
                    break;
                case PrintJobInfo.STATE_STARTED:
                    stateString = "STATE_STARTED";
                    break;
                case PrintJobInfo.STATE_BLOCKED:
                    stateString = "STATE_BLOCKED";
                    break;
                case PrintJobInfo.STATE_FAILED:
                    stateString = "STATE_FAILED";
                    break;
                case PrintJobInfo.STATE_COMPLETED:
                    stateString = "STATE_COMPLETED";
                    break;
                case PrintJobInfo.STATE_CANCELED:
                    stateString = "STATE_CANCELED";
                    break;
                default:
                    stateString = "STATE_UNKNOWN";
                    break;
            }
            states[i] = stateString;
        }
        Log.v(TAG, "Initiating new print with states in queue: {%s}", TextUtils.join(", ", states));
    }
}
