// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

/** Tests for the ContactsDialogHost. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class ContactsDialogHostTest {

    @Test
    @SmallTest
    public void testContactsDialogHostIgnoresDuplicateActions() {
        WebContents mockWebContents = Mockito.mock(WebContents.class);
        long nativePtr = 1234;
        ContactsDialogHost host = ContactsDialogHost.create(mockWebContents, nativePtr);

        ContactsDialogHost.Natives mockJni = Mockito.mock(ContactsDialogHost.Natives.class);
        ContactsDialogHostJni.setInstanceForTesting(mockJni);

        host.onContactsPickerUserAction(
                ContactsPickerListener.ContactsPickerAction.CANCEL,
                /* contacts= */ null,
                /* percentageShared= */ 0,
                /* propertiesSiteRequested= */ 0,
                /* propertiesUserRejected= */ 0);

        // endContactsList should be called once.
        Mockito.verify(mockJni, Mockito.times(1)).endContactsList(nativePtr, 0, 0);

        // Simulate a duplicate call and check that endContactsList is not called a second time.
        host.onContactsPickerUserAction(
                ContactsPickerListener.ContactsPickerAction.CANCEL,
                /* contacts= */ null,
                /* percentageShared= */ 0,
                /* propertiesSiteRequested= */ 0,
                /* propertiesUserRejected= */ 0);
        Mockito.verify(mockJni, Mockito.times(1)).endContactsList(nativePtr, 0, 0);
    }
}
