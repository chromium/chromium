// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Records physical keyboard shortcut events. Suitable for use by any content embedder. */
public class KeyboardShortcutRecorder {
    // This should be kept in sync with the definition |PhysicalKeyboardShortcut| in
    // tools/metrics/histograms/enums.xml and
    // third_party/blink/renderer/core/input/keyboard_shortcut_recorder.h
    @IntDef({
        KeyboardShortcut.ZOOM_IN,
        KeyboardShortcut.ZOOM_OUT,
        KeyboardShortcut.ZOOM_RESET,
        KeyboardShortcut.DELETE_LINE,
        KeyboardShortcut.PAGE_UP,
        KeyboardShortcut.PAGE_DOWN,
        KeyboardShortcut.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface KeyboardShortcut {
        int ZOOM_IN = 0;
        int ZOOM_OUT = 1;
        int ZOOM_RESET = 2;
        int DELETE_LINE = 3;
        int PAGE_UP = 4;
        int PAGE_DOWN = 5;
        int COUNT = 6;
    }

    private KeyboardShortcutRecorder() {}

    public static void recordKeyboardShortcut(@KeyboardShortcut int keyboardShortcut) {
        RecordHistogram.recordEnumeratedHistogram(
                "InputMethod.PhysicalKeyboard.KeyboardShortcut",
                keyboardShortcut,
                KeyboardShortcut.COUNT);
    }
}
