// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.Manifest;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.ContactsPickerListener;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

import java.nio.ByteBuffer;
import java.util.List;

/**
 * The host class for the ContactsDialog. Handles communication between the Java side and the C++
 * side.
 */
@JNINamespace("content")
public class ContactsDialogHost implements ContactsPickerListener {
    private long mNativeContactsProviderAndroid;
    private final WindowAndroid mWindowAndroid;

    @CalledByNative
    static ContactsDialogHost create(
            WindowAndroid windowAndroid, long nativeContactsProviderAndroid) {
        return new ContactsDialogHost(windowAndroid, nativeContactsProviderAndroid);
    }

    private ContactsDialogHost(WindowAndroid windowAndroid, long nativeContactsProviderAndroid) {
        mNativeContactsProviderAndroid = nativeContactsProviderAndroid;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    void destroy() {
        mNativeContactsProviderAndroid = 0;
    }

    @CalledByNative
    private void showDialog(boolean multiple, boolean includeNames, boolean includeEmails,
            boolean includeTel, boolean includeAddresses, boolean includeIcons,
            String formattedOrigin) {
        if (mWindowAndroid.getActivity().get() == null) {
            ContactsDialogHostJni.get().endWithPermissionDenied(mNativeContactsProviderAndroid);
            return;
        }

        if (mWindowAndroid.hasPermission(Manifest.permission.READ_CONTACTS)) {
            if (!UiUtils.showContactsPicker(mWindowAndroid.getActivity().get(), this, multiple,
                        includeNames, includeEmails, includeTel, includeAddresses, includeIcons,
                        formattedOrigin)) {
                ContactsDialogHostJni.get().endWithPermissionDenied(mNativeContactsProviderAndroid);
            }
            return;
        }

        if (!mWindowAndroid.canRequestPermission(Manifest.permission.READ_CONTACTS)) {
            ContactsDialogHostJni.get().endWithPermissionDenied(mNativeContactsProviderAndroid);
            return;
        }

        mWindowAndroid.requestPermissions(
                new String[] {Manifest.permission.READ_CONTACTS}, (permissions, grantResults) -> {
                    if (permissions.length == 1 && grantResults.length == 1
                            && TextUtils.equals(permissions[0], Manifest.permission.READ_CONTACTS)
                            && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                        if (!UiUtils.showContactsPicker(mWindowAndroid.getActivity().get(), this,
                                    multiple, includeNames, includeEmails, includeTel,
                                    includeAddresses, includeIcons, formattedOrigin)) {
                            ContactsDialogHostJni.get().endWithPermissionDenied(
                                    mNativeContactsProviderAndroid);
                        }
                    } else {
                        ContactsDialogHostJni.get().endWithPermissionDenied(
                                mNativeContactsProviderAndroid);
                    }
                });
    }

    @Override
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
