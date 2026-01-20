// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.view.inputmethod.CorrectionInfo;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

/**
 * A class that manages the visual presentation and state of autocorrect underline. This involves
 * showing the underline as well as removing the underline.
 */
@NullMarked
public class AutocorrectManager {

    private static final String TAG = "AutocorrectManager";
    private static final boolean DEBUG_LOGS = false;

    public AutocorrectManager() {}

    /**
     * Processes a {@link CorrectionInfo} object provided by the IME. This method is called when the
     * IME suggests an autocorrection, triggering UI updates and state changes.
     *
     * @param correctionInfo Details about the suggested correction.
     */
    public void handlePendingCorrection(CorrectionInfo correctionInfo) {
        if (DEBUG_LOGS) Log.i(TAG, "pendingCorrection");
    }
}
