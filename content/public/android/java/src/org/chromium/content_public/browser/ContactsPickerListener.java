// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.IntDef;

import org.chromium.blink.mojom.ContactIconBlob;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.payments.mojom.PaymentAddress;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/** The callback used to indicate what action the user took in the picker. */
@NullMarked
public interface ContactsPickerListener {
    /** A container class for exchanging contact details. */
    public static class Contact {
        public final @Nullable List<String> names;
        public final @Nullable List<String> emails;
        public final @Nullable List<String> tel;
        public final @Nullable List<ByteBuffer> serializedAddresses;
        public final @Nullable List<ByteBuffer> serializedIcons;

        public Contact(
                @Nullable List<String> contactNames,
                @Nullable List<String> contactEmails,
                @Nullable List<String> contactTel,
                @Nullable List<PaymentAddress> contactAddresses,
                @Nullable List<ContactIconBlob> contactIcons) {
            names = contactNames;
            emails = contactEmails;
            tel = contactTel;

            if (contactAddresses != null) {
                serializedAddresses = new ArrayList<ByteBuffer>();
                for (PaymentAddress address : contactAddresses) {
                    serializedAddresses.add(address.serialize());
                }
            } else {
                serializedAddresses = null;
            }

            if (contactIcons != null) {
                serializedIcons = new ArrayList<ByteBuffer>();
                for (ContactIconBlob icon : contactIcons) {
                    serializedIcons.add(icon.serialize());
                }
            } else {
                serializedIcons = null;
            }
        }
    }

    /** The action the user took in the picker. */
    @IntDef({
        ContactsPickerAction.CANCEL,
        ContactsPickerAction.CONTACTS_SELECTED,
        ContactsPickerAction.SELECT_ALL,
        ContactsPickerAction.UNDO_SELECT_ALL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContactsPickerAction {
        int CANCEL = 0;
        int CONTACTS_SELECTED = 1;
        int SELECT_ALL = 2;
        int UNDO_SELECT_ALL = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * Called when the user has selected an action. For possible actions see above.
     *
     * @param contacts The list of contacts selected.
     * @param percentageShared How big a percentage of the full contact list was shared (for metrics
     *     purposes).
     * @param propertiesSiteRequested The properties requested by the website (bitmask of names,
     *     emails, telephones, etc).
     * @param propertiesUserRejected The properties rejected by the user (bitmask of names, emails,
     *     telephones, etc) when the sharing dialog is presented.
     */
    void onContactsPickerUserAction(
            @ContactsPickerAction int action,
            @Nullable List<Contact> contacts,
            int percentageShared,
            int propertiesSiteRequested,
            int propertiesUserRejected);
}
