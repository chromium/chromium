// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static com.google.common.truth.Truth.assertThat;

import android.os.Parcel;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class Fido2ApiRobolectricTest {
    private static final byte[] sCredentialId = new byte[] {1, 2, 3, 4};

    @Test
    @SmallTest
    public void testParseCredentialList_fromCacheIsDiscoverableTrue() {
        Parcel p = Parcel.obtain();
        p.writeInt(1); // One credential in list.
        p.writeInt(4); // VAL_PARCELABLE
        p.writeString("com.google.android.gms.fido.fido2.api.common.DiscoverableCredentialInfo");
        writeCredential(p, sCredentialId, /* isDiscoverable= */ true);

        p.setDataPosition(0);
        List<WebauthnCredentialDetails> credentials =
                Fido2Api.parseCredentialList(p, /* fromCache= */ true);
        assertThat(credentials).hasSize(1);
        assertThat(credentials.get(0).mIsDiscoverable).isTrue();
        assertThat(credentials.get(0).mCredentialId).isEqualTo(sCredentialId);
    }

    @Test
    @SmallTest
    public void testParseCredentialList_notFromCacheIsDiscoverableTrue() {
        Parcel p = Parcel.obtain();
        p.writeInt(1); // One credential in list.
        p.writeInt(4); // VAL_PARCELABLE
        p.writeString("com.google.android.gms.fido.fido2.api.common.DiscoverableCredentialInfo");
        writeCredential(p, sCredentialId, /* isDiscoverable= */ true);

        p.setDataPosition(0);
        List<WebauthnCredentialDetails> credentials =
                Fido2Api.parseCredentialList(p, /* fromCache= */ false);
        assertThat(credentials).hasSize(1);
        assertThat(credentials.get(0).mIsDiscoverable).isTrue();
        assertThat(credentials.get(0).mCredentialId).isEqualTo(sCredentialId);
    }

    @Test
    @SmallTest
    public void testParseCredentialList_notFromCacheIsDiscoverableFalse() {
        Parcel p = Parcel.obtain();
        p.writeInt(1); // One credential in list.
        p.writeInt(4); // VAL_PARCELABLE
        p.writeString("com.google.android.gms.fido.fido2.api.common.DiscoverableCredentialInfo");
        writeCredential(p, sCredentialId, /* isDiscoverable= */ false);

        p.setDataPosition(0);
        List<WebauthnCredentialDetails> credentials =
                Fido2Api.parseCredentialList(p, /* fromCache= */ false);
        assertThat(credentials).hasSize(1);
        assertThat(credentials.get(0).mIsDiscoverable).isFalse();
        assertThat(credentials.get(0).mCredentialId).isEqualTo(sCredentialId);
    }

    private void writeCredential(Parcel p, byte[] credentialId, boolean isDiscoverable) {
        final int objectStart = writeHeader(20293, p); // OBJECT_MAGIC
        final int credIdStart = writeHeader(4, p);
        p.writeByteArray(credentialId);
        writeLength(credIdStart, p);

        final int userNameStart = writeHeader(1, p);
        p.writeString("name");
        writeLength(userNameStart, p);
        final int userDisplayNameStart = writeHeader(2, p);
        p.writeString("displayName");
        writeLength(userDisplayNameStart, p);
        final int userIdStart = writeHeader(3, p);
        p.writeByteArray(new byte[] {1});
        writeLength(userIdStart, p);

        final int isDiscoverableStart = writeHeader(5, p);
        p.writeInt(isDiscoverable ? 1 : 0);
        writeLength(isDiscoverableStart, p);

        writeLength(objectStart, p);
    }

    private static int writeHeader(int tag, Parcel parcel) {
        parcel.writeInt(0xffff0000 | tag);
        return startLength(parcel);
    }

    private static int startLength(Parcel parcel) {
        int pos = parcel.dataPosition();
        parcel.writeInt(0xdddddddd);
        return pos;
    }

    private static void writeLength(int pos, Parcel parcel) {
        int totalLength = parcel.dataPosition();
        parcel.setDataPosition(pos);
        parcel.writeInt(totalLength - pos - 4);
        parcel.setDataPosition(totalLength);
    }
}
