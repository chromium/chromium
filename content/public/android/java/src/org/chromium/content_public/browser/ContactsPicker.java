// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.VisibleForTesting;

/**
 * A utility class that allows the embedder to provide a Contacts Picker implementation to content.
 */
public final class ContactsPicker {
    /**
     * The current delegate for the contacts picker, or null if navigator.contacts is not
     * supported.
     */
    private static ContactsPickerDelegate sContactsPickerDelegate;

    /**
     * The object that represents the currently visible contacts picker UI, or null if none is
     * visible.
     */
    private static Object sPicker;

    private ContactsPicker() {}

    /**
     * Allows setting a delegate for an Android contacts picker.
     * @param delegate A {@link ContactsPickerDelegate} instance.
     */
    public static void setContactsPickerDelegate(ContactsPickerDelegate delegate) {
        sContactsPickerDelegate = delegate;
    }

    /**
     * @param webContents The web contents using the contacts API.
     * @return Whether the contacts picker should be shown.
     */
    @VisibleForTesting
    public static boolean canShowContactsPicker(WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) {
            return false;
        }

        return webContents.getVisibility() == Visibility.VISIBLE;
    }

    /**
     * Called to display the contacts picker.
     * @param webContents The Web Contents that triggered this call.
     * @param listener The listener that will be notified of the action the user took in the
     *                 picker.
     * @param allowMultiple Whether to allow multiple contacts to be selected.
     * @param includeNames Whether to include names of the shared contacts.
     * @param includeEmails Whether to include emails of the shared contacts.
     * @param includeTel Whether to include telephone numbers of the shared contacts.
     * @param includeAddresses Whether to include addresses of the shared contacts.
     * @param includeIcons Whether to include icons of the shared contacts.
     * @param formattedOrigin The origin the data will be shared with, formatted for display with
     *         the scheme omitted.
     * @return whether a contacts picker is successfully shown.
     */
    public static boolean showContactsPicker(
            WebContents webContents,
            ContactsPickerListener listener,
            boolean allowMultiple,
            boolean includeNames,
            boolean includeEmails,
            boolean includeTel,
            boolean includeAddresses,
            boolean includeIcons,
            String formattedOrigin) {
        if (sContactsPickerDelegate == null) return false;
        assert sPicker == null;

        if (!canShowContactsPicker(webContents)) {
            return false;
        }
        sPicker =
                sContactsPickerDelegate.showContactsPicker(
                        webContents,
                        listener,
                        allowMultiple,
                        includeNames,
                        includeEmails,
                        includeTel,
                        includeAddresses,
                        includeIcons,
                        formattedOrigin);
        return true;
    }

    /** Called when the contacts picker dialog has been dismissed. */
    public static void onContactsPickerDismissed() {
        assert sPicker != null;
        sPicker = null;
    }
}
