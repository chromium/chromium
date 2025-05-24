// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Util class that handles command line switches that are specific to the content/
 * portion of Chromium on Android.
 */
@NullMarked
public final class ContentSwitchUtils {
    // Prevent instantiation.
    private ContentSwitchUtils() {}

    public static @Nullable String getSwitchValue(final String[] commandLine, String switchKey) {
        if (commandLine == null || switchKey == null) {
            return null;
        }
        // This format should be matched with the one defined in command_line.h.
        final String switchKeyPrefix = "--" + switchKey + "=";
        for (String command : commandLine) {
            if (command != null && command.startsWith(switchKeyPrefix)) {
                return command.substring(switchKeyPrefix.length());
            }
        }
        return null;
    }
}
