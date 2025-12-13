// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.build.annotations.NullMarked;

/** An interface to perform permission check for the contact picker. */
@NullMarked
public interface ContactsPermissionProvider {
    /** Callback to tell the result to the client. */
    interface Callback {
        /**
         * Called when permission is allowed.
         *
         * @param contactsFetcher The source of contact information.
         */
        void onAllowed(ContactsFetcher contactsFetcher);

        /** Called when permission is denied. */
        void onDenied();
    }

    /**
     * Runs delegation and return back the result via callback.
     *
     * @param webContents Current {@link WebContents} the contact picker running in.
     * @param callback Callback to tell the result to the caller.
     */
    void run(WebContents webContents, Callback callback);
}
