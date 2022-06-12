// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.Manifest;
import android.content.pm.PackageManager;
import android.text.TextUtils;
import android.widget.Toast;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/**
 * The host class for the ContactsDialog. Handles communication between the Java side and the C++
 * side.
 */
@JNINamespace("content")
public class ContactsDialogHost {

    /**
     * The action the user took in the picker.
     */
    @IntDef({ContactsPickerAction.CANCEL, ContactsPickerAction.CONTACTS_SELECTED,
            ContactsPickerAction.SELECT_ALL, ContactsPickerAction.UNDO_SELECT_ALL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContactsPickerAction {
        int CANCEL = 0;
        int CONTACTS_SELECTED = 1;
        int SELECT_ALL = 2;
        int UNDO_SELECT_ALL = 3;
        int NUM_ENTRIES = 4;
    }

    public static class Contact {
        public final List<String> names;
        public final List<String> emails;
        public final List<String> tel;
        public final List<ByteBuffer> serializedAddresses;
        public final List<ByteBuffer> serializedIcons;

        public Contact(List<String> contactNames, List<String> contactEmails,
                       List<String> contactTel, List<org.chromium.payments.mojom.PaymentAddress> contactAddresses,
                       List<org.chromium.blink.mojom.ContactIconBlob> contactIcons) {
            names = contactNames;
            emails = contactEmails;
            tel = contactTel;

            if (contactAddresses != null) {
                serializedAddresses = new ArrayList<ByteBuffer>();
                for (org.chromium.payments.mojom.PaymentAddress address : contactAddresses) {
                    serializedAddresses.add(address.serialize());
                }
            } else {
                serializedAddresses = null;
            }

            if (contactIcons != null) {
                serializedIcons = new ArrayList<ByteBuffer>();
                for (org.chromium.blink.mojom.ContactIconBlob icon : contactIcons) {
                    serializedIcons.add(icon.serialize());
                }
            } else {
                serializedIcons = null;
            }
        }
    }





    private long mNativeContactsProviderAndroid;
    private final WebContents mWebContents;

    @CalledByNative
    static ContactsDialogHost create(WebContents webContents, long nativeContactsProviderAndroid) {
        return new ContactsDialogHost(webContents, nativeContactsProviderAndroid);
    }

    private ContactsDialogHost(WebContents webContents, long nativeContactsProviderAndroid) {
        mNativeContactsProviderAndroid = nativeContactsProviderAndroid;
        mWebContents = webContents;
    }

    @CalledByNative
    void destroy() {
        mNativeContactsProviderAndroid = 0;
    }

    @CalledByNative
    private void showDialog(boolean multiple, boolean includeNames, boolean includeEmails,
            boolean includeTel, boolean includeAddresses, boolean includeIcons,
            String formattedOrigin) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        assert windowAndroid != null;

        if (windowAndroid.getActivity().get() == null) {
            ContactsDialogHostJni.get().endWithPermissionDenied(mNativeContactsProviderAndroid);
            return;
        }

        if (windowAndroid.hasPermission(Manifest.permission.READ_CONTACTS)) {
            if (!showContactsPicker(mWebContents, multiple, includeNames,
                        includeEmails, includeTel, includeAddresses, includeIcons,
                        formattedOrigin)) {
                ContactsDialogHostJni.get().endWithPermissionDenied(mNativeContactsProviderAndroid);
            }
            return;
        }

        if (!windowAndroid.canRequestPermission(Manifest.permission.READ_CONTACTS)) {
            ContactsDialogHostJni.get().endWithPermissionDenied(mNativeContactsProviderAndroid);
            return;
        }

        windowAndroid.requestPermissions(
                new String[] {Manifest.permission.READ_CONTACTS}, (permissions, grantResults) -> {
                    if (permissions.length == 1 && grantResults.length == 1
                            && TextUtils.equals(permissions[0], Manifest.permission.READ_CONTACTS)
                            && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                        if (!showContactsPicker(mWebContents, multiple,
                                    includeNames, includeEmails, includeTel, includeAddresses,
                                    includeIcons, formattedOrigin)) {
                            ContactsDialogHostJni.get().endWithPermissionDenied(
                                    mNativeContactsProviderAndroid);
                        }
                    } else {
                        ContactsDialogHostJni.get().endWithPermissionDenied(
                                mNativeContactsProviderAndroid);
                    }
                });
    }

    public boolean showContactsPicker(WebContents webContents, boolean allowMultiple, boolean includeNames,
                                             boolean includeEmails, boolean includeTel, boolean includeAddresses,
                                             boolean includeIcons, String formattedOrigin) {
        Toast.makeText(webContents.getTopLevelNativeWindow().getApplicationContext(),
                "TODO showContactsPicker", Toast.LENGTH_SHORT).show();
        return false;
    }

    public void onContactsPickerUserAction(@ContactsPickerAction int action, List<Contact> contacts,
                                           int percentageShared, int propertiesRequested) {
        if (mNativeContactsProviderAndroid == 0) return;

        switch (action) {
            case ContactsPickerAction.CANCEL:
                ContactsDialogHostJni.get().endContactsList(
                        mNativeContactsProviderAndroid, 0, propertiesRequested);
                break;

            case ContactsPickerAction.CONTACTS_SELECTED:
                for (Contact contact : contacts) {
                    ContactsDialogHostJni.get().addContact(mNativeContactsProviderAndroid,
                            contact.names != null
                                    ? contact.names.toArray(new String[contact.names.size()])
                                    : null,
                            contact.emails != null
                                    ? contact.emails.toArray(new String[contact.emails.size()])
                                    : null,
                            contact.tel != null
                                    ? contact.tel.toArray(new String[contact.tel.size()])
                                    : null,
                            contact.serializedAddresses != null
                                    ? contact.serializedAddresses.toArray(
                                    new ByteBuffer[contact.serializedAddresses.size()])
                                    : null,
                            contact.serializedIcons != null ? contact.serializedIcons.toArray(
                                    new ByteBuffer[contact.serializedIcons.size()])
                                    : null);
                }
                ContactsDialogHostJni.get().endContactsList(
                        mNativeContactsProviderAndroid, percentageShared, propertiesRequested);
                break;

            case ContactsPickerAction.SELECT_ALL:
            case ContactsPickerAction.UNDO_SELECT_ALL:
                break;
        }
    }

    @NativeMethods
    interface Natives {
        void addContact(long nativeContactsProviderAndroid, String[] names, String[] emails,
                String[] tel, ByteBuffer[] addresses, ByteBuffer[] icons);
        void endContactsList(
                long nativeContactsProviderAndroid, int percentageShared, int propertiesRequested);
        void endWithPermissionDenied(long nativeContactsProviderAndroid);
    }
}
