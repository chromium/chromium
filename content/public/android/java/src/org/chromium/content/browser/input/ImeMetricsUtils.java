// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/** Utility class for recording IME-related UMA metrics. */
@NullMarked
public class ImeMetricsUtils {

    /** Supported image file extensions for commitContent metrics. */
    @IntDef({
        ExtensionFormat.GIF,
        ExtensionFormat.JPEG,
        ExtensionFormat.JPG,
        ExtensionFormat.PNG,
        ExtensionFormat.SVG,
        ExtensionFormat.WEBP,
        ExtensionFormat.OTHER
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ExtensionFormat {
        int GIF = 0;
        int JPEG = 1;
        int JPG = 2;
        int PNG = 3;
        int SVG = 4;
        int WEBP = 5;
        int OTHER = 6;
        int NUM_ENTRIES = 7;
    }

    private ImeMetricsUtils() {}

    /** Converts a file extension string into an ExtensionFormat enum value. */
    @ExtensionFormat
    public static int extensionToFormat(@Nullable String extension) {
        if (extension == null || extension.isEmpty()) {
            return ExtensionFormat.OTHER;
        }
        String ext = extension.toLowerCase(Locale.US);
        switch (ext) {
            case "gif":
                return ExtensionFormat.GIF;
            case "jpeg":
                return ExtensionFormat.JPEG;
            case "jpg":
                return ExtensionFormat.JPG;
            case "png":
                return ExtensionFormat.PNG;
            case "svg":
                return ExtensionFormat.SVG;
            case "webp":
                return ExtensionFormat.WEBP;
            default:
                return ExtensionFormat.OTHER;
        }
    }

    /**
     * Records the success or failure of an IME commitContent image insertion attempt.
     *
     * @param extension the file extension of the inserted media content
     * @param success whether the insertion succeeded
     */
    public static void recordCommitContentSuccess(@Nullable String extension, boolean success) {
        int format = extensionToFormat(extension);
        String histogramName =
                success ? "Input.CommitContent.Success" : "Input.CommitContent.Failure";
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, format, ExtensionFormat.NUM_ENTRIES);
    }
}
