// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for the ContactsDialogHost. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContactsDialogHostTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private WebContents mWebContents;
    @Mock private ContactsDialogHost.Natives mContactsDialogHostNatives;

    @Test
    public void testContactsDialogHostIgnoresDuplicateActions() {
        long nativePtr = 1234;
        ContactsDialogHost host = ContactsDialogHost.create(mWebContents, nativePtr);

        ContactsDialogHostJni.setInstanceForTesting(mContactsDialogHostNatives);

        host.onContactsPickerUserAction(
                ContactsPickerListener.ContactsPickerAction.CANCEL,
                /* contacts= */ null,
                /* percentageShared= */ 0,
                /* propertiesSiteRequested= */ 0,
                /* propertiesUserRejected= */ 0);

        // endContactsList should be called once.
        Mockito.verify(mContactsDialogHostNatives, Mockito.times(1))
                .endContactsList(nativePtr, 0, 0);

        // Simulate a duplicate call and check that endContactsList is not called a second time.
        host.onContactsPickerUserAction(
                ContactsPickerListener.ContactsPickerAction.CANCEL,
                /* contacts= */ null,
                /* percentageShared= */ 0,
                /* propertiesSiteRequested= */ 0,
                /* propertiesUserRejected= */ 0);
        Mockito.verify(mContactsDialogHostNatives, Mockito.times(1))
                .endContactsList(nativePtr, 0, 0);
    }
}
