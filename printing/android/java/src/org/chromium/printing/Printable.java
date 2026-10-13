// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import android.os.ParcelFileDescriptor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.io.InputStream;

/**
 * Describes a class that can initiate the printing process.
 *
 * <p>This interface helps decoupling Tab from the printing implementation and helps with testing.
 */
@NullMarked
public interface Printable {
    /**
     * Dispatches beforeprint early, before the framework asks for the document content, so that the
     * page can render asynchronously while the system print dialog is up. Returns whether the
     * request was sent to the renderer.
     */
    boolean initiatePrint(int renderProcessId, int renderFrameId);

    /**
     * Start the PDF generation process.
     *
     * <p>Contract: The callee takes ownership of {@code fd} unconditionally upon invocation and is
     * responsible for closing it on all code paths (success or failure).
     */
    boolean print(
            int renderProcessId,
            int renderFrameId,
            int fd,
            int dpi,
            int width,
            int height,
            int @Nullable [] pages);

    /** Helper for implementations of {@link #print} that need to drop an unused {@code fd}. */
    static void closeFd(int fd) {
        if (fd < 0) return;
        try (ParcelFileDescriptor pfd = ParcelFileDescriptor.adoptFd(fd)) {
            // Automatically closed by try-with-resources.
        } catch (IOException e) {
            /* ignore */
        }
    }

    /** Finalizes the print session: dispatches afterprint. Called even if printing failed. */
    void finishPrint(int renderProcessId, int renderFrameId);

    /** Get the title of the generated PDF document. */
    String getTitle();

    /** Get the error message from the Printable. */
    String getErrorMessage();

    /** Check if the current Printable can print. */
    boolean canPrint();

    /**
     * Opens a new {@link InputStream} for the PDF document when {@link #isPdf()} is true. The
     * caller owns the returned stream and must close it.
     */
    @Nullable InputStream getPdfInputStream();

    /** Returns whether this Printable represents an existing PDF document. */
    default boolean isPdf() {
        return false;
    }
}
