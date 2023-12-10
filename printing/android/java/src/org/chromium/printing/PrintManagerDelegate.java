// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;

/**
 * Defines an interface for the Android system printing service, for easier testing.
 * We can't simply extend from {@link android.print.PrintManager}, since it's a final class.
 */
public interface PrintManagerDelegate {

    /**
     * Same as {@link android.print.PrintManager#print}, except this doesn't return a
     * {@link android.print.PrintJob} since the clients don't need it.
     */
    void print(
            String printJobName, PrintDocumentAdapter documentAdapter, PrintAttributes attributes);
}
