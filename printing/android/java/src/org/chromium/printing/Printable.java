// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

/**
 * Describes a class that can initiate the printing process.
 *
 * This interface helps decoupling Tab from the printing implementation and helps with testing.
 */
public interface Printable {
    /** Start the PDF generation process. */
    boolean print(int renderProcessId, int renderFrameId);

    /** Get the title of the generated PDF document. */
    String getTitle();

    /** Get the error message from the Printable. */
    String getErrorMessage();

    /** Check if the current Printable can print. */
    boolean canPrint();
}
