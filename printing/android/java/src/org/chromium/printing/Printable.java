// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Describes a class that can initiate the printing process.
 *
 * <p>This interface helps decoupling Tab from the printing implementation and helps with testing.
 */
@NullMarked
public interface Printable {
    /** Start the PDF generation process. */
    boolean print(int renderProcessId, int renderFrameId);

    /** Get the title of the generated PDF document. */
    String getTitle();

    /** Get the error message from the Printable. */
    String getErrorMessage();

    /** Check if the current Printable can print. */
    boolean canPrint();

    /** Get the file path if the print job is already a pdf. Otherwise return null. */
    @Nullable String getPdfFilePath();
}
