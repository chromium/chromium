// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;
import java.util.List;

/**
 * The host class for the ContactsDialog. Handles communication between the Java side and the C++
 * side.
 */
@JNINamespace("content")
@NullMarked
public class ContactsDialogHost implements ContactsPickerListener {
    private static final String TAG = "ContactsDialogHost";

    private long mNativeContactsProviderAndroid;
    private final WebContents mWebContents;

    private static @Nullable ContactsPermissionProvider sContactsPermissionProvider;

    /**
     * Allows setting a {@link ContactsPermissionProvider}.
     *
     * @param contactsPermissionProvider A {@link ContactsPermissionProvider} instance.
     */
    public static void setPermissionProvider(
            ContactsPermissionProvider contactsPermissionProvider) {
        sContactsPermissionProvider = contactsPermissionProvider;
    }

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
        if (sContactsPermissionProvider == null) {
            Log.e(TAG, "Permission provider not set");
            ContactsDialogHostJni.get().endWithPermissionDenied(mNativeContactsProviderAndroid);
        } else {
            ContactsPickerListener listener = this;

            sContactsPermissionProvider.run(
                    mWebContents,
                    new ContactsPermissionProvider.Callback() {
                        @Override
                        public void onAllowed(ContactsFetcher contactsFetcher) {
                            if (isDestroyed()) {
                                return;
                            }
                            if (!ContactsPicker.showContactsPicker(
                                    mWebContents,
                                    listener,
                                    multiple,
                                    includeNames,
                                    includeEmails,
                                    includeTel,
                                    includeAddresses,
                                    includeIcons,
                                    formattedOrigin,
                                    contactsFetcher)) {
                                onDenied();
                            }
                        }

                        @Override
                        public void onDenied() {
                            if (isDestroyed()) {
                                return;
                            }
                            ContactsDialogHostJni.get()
                                    .endWithPermissionDenied(mNativeContactsProviderAndroid);
                        }
                    });
        }
    }

    @Override
    public void onContactsPickerUserAction(
            @ContactsPickerAction int action,
            @Nullable List<Contact> contacts,
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
                assumeNonNull(contacts);
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
                String @Nullable [] names,
                String @Nullable [] emails,
                String @Nullable [] tel,
                ByteBuffer @Nullable [] addresses,
                ByteBuffer @Nullable [] icons);

        void endContactsList(
                long nativeContactsProviderAndroid,
                int percentageShared,
                int propertiesSiteRequested);

        void endWithPermissionDenied(long nativeContactsProviderAndroid);
    }
}
