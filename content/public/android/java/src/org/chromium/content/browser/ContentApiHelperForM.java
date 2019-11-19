// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.ActionMode;
import android.view.View;

import org.chromium.base.annotations.VerifiesOnM;
import org.chromium.content.browser.selection.FloatingActionModeCallback;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;

/**
 * Utility class to use new APIs that were added in M (API level 23). These need to exist in a
 * separate class so that Android framework can successfully verify selection classes without
 * encountering the new APIs.
 */

@VerifiesOnM
@TargetApi(Build.VERSION_CODES.M)
public final class ContentApiHelperForM {
    private ContentApiHelperForM() {}

    /**
     * See {@link View#startActionMode(ActionMode.Callback, int)}, which was added in M.
     */
    public static ActionMode startActionMode(View view,
            SelectionPopupControllerImpl selectionPopupController, ActionMode.Callback callback) {
        return view.startActionMode(
                new FloatingActionModeCallback(selectionPopupController, callback),
                ActionMode.TYPE_FLOATING);
    }
}
