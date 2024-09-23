// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/** A delegate interface for the contacts picker. */
public interface ContactsPickerDelegate {
    /**
     * Called to display the contacts picker.
     *
     * @param webContents The Web Contents that triggered the dialog.
     * @param listener The listener that will be notified of the action the user took in the picker.
     * @param allowMultiple Whether to allow multiple contacts to be picked.
     * @param includeNames Whether to include names of the shared contacts.
     * @param includeEmails Whether to include emails of the shared contacts.
     * @param includeTel Whether to include telephone numbers of the shared contacts.
     * @param includeAddresses Whether to include addresses of the shared contacts.
     * @param includeIcons Whether to include icons of the shared contacts.
     * @param formattedOrigin The origin the data will be shared with, formatted for display with
     *     the scheme omitted.
     * @return the contacts picker object.
     */
    Object showContactsPicker(
            WebContents webContents,
            ContactsPickerListener listener,
            boolean allowMultiple,
            boolean includeNames,
            boolean includeEmails,
            boolean includeTel,
            boolean includeAddresses,
            boolean includeIcons,
            String formattedOrigin);
}
