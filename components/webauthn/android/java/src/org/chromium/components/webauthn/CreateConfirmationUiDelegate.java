// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;

/**
 * Interface for code that will show the user a confirmation before creating a credential.
 *
 * <p>This is intended for use in Incognito mode.
 */
public interface CreateConfirmationUiDelegate {
    interface Factory {
        /**
         * Creates a {@link CreateConfirmationUiDelegate} if required for a given {@link
         * WebContents}
         *
         * @param webContents {@link WebContents} to create the UI delegate for.
         * @return Returns null if CreateConfirmationUiDelegate is not required for the webContents
         */
        @Nullable
        CreateConfirmationUiDelegate create(WebContents webContents);
    }

    /**
     * Shows the UI delegate for creation confirmation.
     *
     * @param onUserAccept Callback to run if the user accepts.
     * @param onUserReject Callback to run if the user rejects.
     * @return true iff the UI delegate is shown
     */
    boolean show(Runnable onUserAccept, Runnable onUserReject);
}
