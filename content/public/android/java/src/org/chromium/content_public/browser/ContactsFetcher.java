// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.graphics.Bitmap;

import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.payments.mojom.PaymentAddress;

import java.util.ArrayList;
import java.util.List;

/** An interface to fetch contact information for a contact picker. */
@NullMarked
public interface ContactsFetcher {
    /** An interface to pass contact information retrieved from a source. */
    interface RetrievedContact {
        String getId();

        String getDisplayName();

        List<String> getEmails();

        List<String> getPhoneNumbers();

        List<PaymentAddress> getAddresses();
    }

    /** An interface to use to communicate back the results to the client. */
    interface ContactsRetrievedCallback {
        /**
         * A callback to define to receive the contact details.
         *
         * @param contacts The contacts retrieved.
         */
        void contactsRetrieved(ArrayList<RetrievedContact> contacts);
    }

    /** An interface to use to communicate back the results to the client. */
    interface IconRetrievedCallback {
        /**
         * A callback to define to receive the icon for a contact.
         *
         * @param icon The icon retrieved.
         * @param contactId The id of the contact the icon refers to.
         */
        void iconRetrieved(@Nullable Bitmap icon, String contactId);
    }

    /**
     * Fetches the details for all contacts. If it runs AsyncTask, returns it for cancellation.
     *
     * @param includeNames Whether names were requested by the website.
     * @param includeEmails Whether to include emails in the data fetched.
     * @param includeTel Whether to include telephones in the data fetched.
     * @param includeAddresses Whether to include telephones in the data fetched.
     * @param callback The callback to use to communicate back the results.
     * @return The AsyncTask running background.
     */
    @Nullable AsyncTask fetchContacts(
            boolean includeNames,
            boolean includeEmails,
            boolean includeTel,
            boolean includeAddresses,
            ContactsRetrievedCallback callback);

    /**
     * Fetches the icon of a particular contact. If it runs AsyncTask, returns it for cancellation.
     *
     * @param id The id of the contact to look up.
     * @param iconSize the size (both width and height) to scale to.
     * @param callback The callback to use to communicate back the results.
     * @return The AsyncTask running background.
     */
    @Nullable AsyncTask fetchIcon(String id, int iconSize, IconRetrievedCallback callback);
}
