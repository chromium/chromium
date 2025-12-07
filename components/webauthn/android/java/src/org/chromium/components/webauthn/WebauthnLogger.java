// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.base.Log;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.build.annotations.DoNotStripLogs;
import org.chromium.build.annotations.NullMarked;

/** A class to handle logging for the WebAuthn component. */
@NullMarked
public final class WebauthnLogger {
    public static final String TAG = "ChromiumWebauthn";
    private static boolean sIsLoggable;

    @DoNotStripLogs
    @DoNotInline
    public static void updateLogState() {
        // Use 'setprop log.tag.ChromiumWebauthn DEBUG' to enable the log at runtime.
        // See https://source.android.com/docs/core/tests/debug/understanding-logging#log-standards
        // for more details.
        sIsLoggable = android.util.Log.isLoggable(TAG, Log.DEBUG);
    }

    /**
     * Logs a message with formatting if logging is enabled for the tag.
     *
     * @param className The name of the class from which the log is being sent.
     * @param message The message to log, with optional format specifiers.
     * @param args Optional arguments for the format string.
     */
    public static void log(String className, String message, Object... args) {
        if (sIsLoggable) {
            // Log.i() instead of Log.d() is used here because Log.d() is stripped out in release
            // builds.
            Log.i(TAG, "[" + className + "] " + message, args);
        }
    }

    /**
     * Logs an error message.
     *
     * @param className The name of the class from which the log is being sent.
     * @param message The message to log, with optional format specifiers.
     * @param args Optional arguments for the format string, the last one can be a throwable.
     */
    public static void logError(String className, String message, Object... args) {
        Log.e(TAG, "[" + className + "] " + message, args);
    }

    private WebauthnLogger() {}
}
