// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.Manifest;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.ContactsPicker;
import org.chromium.content_public.browser.ContactsPickerListener;
import org.chromium.content_public.browser.WebContents;
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

    private boolean isDestroyed() {
        return mNativeContactsProviderAndroid == 0;
    }

    @CalledByNative
    private void showDialog(
            boolean multiple,
            boolean includeNames,
            boolean includeEmails,
            boolean includeTel,
            boolean includeAddresses,
            boolean includeIcons,
            String formattedOrigin) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        assert windowAndroid != null;

        if (windowAndroid.getActivity().get() == null) {
            ContactsDialogHostJni.get().endWithPermissionDenied(mNativeContactsProviderAndroid);
            return;
        }

        if (windowAndroid.hasPermission(Manifest.permission.READ_CONTACTS)) {
            if (!ContactsPicker.showContactsPicker(
                    mWebContents,
                    this,
                    multiple,
                    includeNames,
                    includeEmails,
                    includeTel,
                    includeAddresses,
                    includeIcons,
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
                new String[] {Manifest.permission.READ_CONTACTS},
                (permissions, grantResults) -> {
                    if (isDestroyed()) return;
                    if (permissions.length == 1
                            && grantResults.length == 1
                            && TextUtils.equals(permissions[0], Manifest.permission.READ_CONTACTS)
                            && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                        if (!ContactsPicker.showContactsPicker(
                                mWebContents,
                                this,
                                multiple,
                                includeNames,
                                includeEmails,
                                includeTel,
                                includeAddresses,
                                includeIcons,
                                formattedOrigin)) {
                            ContactsDialogHostJni.get()
                                    .endWithPermissionDenied(mNativeContactsProviderAndroid);
                        }
                    } else {
                        ContactsDialogHostJni.get()
                                .endWithPermissionDenied(mNativeContactsProviderAndroid);
                    }
                });
    }

    @Override
    public void onContactsPickerUserAction(
            @ContactsPickerAction int action,
            List<Contact> contacts,
            int percentageShared,
            int propertiesSiteRequested,
            int propertiesUserRejected) {
        if (mNativeContactsProviderAndroid == 0) return;

        switch (action) {
            case ContactsPickerAction.CANCEL:
                ContactsDialogHostJni.get()
                        .endContactsList(
                                mNativeContactsProviderAndroid, 0, propertiesSiteRequested);
                break;

            case ContactsPickerAction.CONTACTS_SELECTED:
                for (Contact contact : contacts) {
                    ContactsDialogHostJni.get()
                            .addContact(
                                    mNativeContactsProviderAndroid,
                                    contact.names != null
                                            ? contact.names.toArray(
                                                    new String[contact.names.size()])
                                            : null,
                                    contact.emails != null
                                            ? contact.emails.toArray(
                                                    new String[contact.emails.size()])
                                            : null,
                                    contact.tel != null
                                            ? contact.tel.toArray(new String[contact.tel.size()])
                                            : null,
                                    contact.serializedAddresses != null
                                            ? contact.serializedAddresses.toArray(
                                                    new ByteBuffer
                                                            [contact.serializedAddresses.size()])
                                            : null,
                                    contact.serializedIcons != null
                                            ? contact.serializedIcons.toArray(
                                                    new ByteBuffer[contact.serializedIcons.size()])
                                            : null);
                }
                ContactsDialogHostJni.get()
                        .endContactsList(
                                mNativeContactsProviderAndroid,
                                percentageShared,
                                propertiesSiteRequested);
                break;

            case ContactsPickerAction.SELECT_ALL:
            case ContactsPickerAction.UNDO_SELECT_ALL:
                break;
        }
    }

    @NativeMethods
    interface Natives {
        void addContact(
                long nativeContactsProviderAndroid,
                String[] names,
                String[] emails,
                String[] tel,
                ByteBuffer[] addresses,
                ByteBuffer[] icons);

        void endContactsList(
                long nativeContactsProviderAndroid,
                int percentageShared,
                int propertiesSiteRequested);

        void endWithPermissionDenied(long nativeContactsProviderAndroid);
    }
}
