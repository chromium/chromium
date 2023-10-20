// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for the {@link NfcBlocklist} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NfcBlocklistTest {
    // Static historical bytes
    private static final byte[] YUBIKEY_NEO_HISTORICAL_BYTES =
            new byte[] {
                (byte) 0x59, 0x75, 0x62, 0x69, 0x6b, 0x65, 0x79, 0x4e, 0x45, 0x4f, 0x72, 0x33
            };
    private static final byte[] YUBIKEY_5_SERIES_HISTORICAL_BYTES =
            new byte[] {
                (byte) 0x80,
                0x73,
                (byte) 0xc0,
                0x21,
                (byte) 0xc0,
                0x57,
                0x59,
                0x75,
                0x62,
                0x69,
                0x4b,
                0x65,
                0x79
            };

    private boolean areHistoricalBytesBlocked(byte[] historicalBytes) {
        return NfcBlocklist.getInstance().areHistoricalBytesBlocked(historicalBytes);
    }

    @Test
    @Feature({"NfcBlocklistTest"})
    public void testHistoricalBytesWithoutProvidedServerValues() {
        NfcBlocklist.overrideNfcBlocklistForTests(/* serverProvidedValues= */ null);

        // Static historical bytes are blocked.
        assertTrue(areHistoricalBytesBlocked(YUBIKEY_NEO_HISTORICAL_BYTES));
        assertTrue(areHistoricalBytesBlocked(YUBIKEY_5_SERIES_HISTORICAL_BYTES));

        // Random historical bytes are not blocked.
        assertFalse(areHistoricalBytesBlocked(new byte[] {}));
        assertFalse(areHistoricalBytesBlocked(new byte[] {0x01, 0x02, 0x03}));
    }

    @Test
    @Feature({"NfcBlocklistTest"})
    public void testHistoricalBytesWithValidProvidedServerValues() {
        NfcBlocklist.overrideNfcBlocklistForTests("010203040506070809,0A0B0C0D0E0F");

        byte[] firstHistoricalBytes =
                new byte[] {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
        byte[] secondHistoricalBytes = new byte[] {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

        // Static historical bytes are blocked.
        assertTrue(areHistoricalBytesBlocked(YUBIKEY_NEO_HISTORICAL_BYTES));
        assertTrue(areHistoricalBytesBlocked(YUBIKEY_5_SERIES_HISTORICAL_BYTES));

        // Server provided historical bytes are blocked.
        assertTrue(areHistoricalBytesBlocked(firstHistoricalBytes));
        assertTrue(areHistoricalBytesBlocked(secondHistoricalBytes));

        // Random historical bytes are not blocked.
        assertFalse(areHistoricalBytesBlocked(new byte[] {}));
        assertFalse(areHistoricalBytesBlocked(new byte[] {0x01, 0x02, 0x03}));
    }

    @Test
    @Feature({"NfcBlocklistTest"})
    public void testHistoricalBytesWithInvalidProvidedServerValues() {
        NfcBlocklist.overrideNfcBlocklistForTests("0x,010203040506070809,fish,0A0B0C0D0E0F");

        byte[] firstHistoricalBytes =
                new byte[] {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
        byte[] secondHistoricalBytes = new byte[] {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

        // Static historical bytes are blocked.
        assertTrue(areHistoricalBytesBlocked(YUBIKEY_NEO_HISTORICAL_BYTES));
        assertTrue(areHistoricalBytesBlocked(YUBIKEY_5_SERIES_HISTORICAL_BYTES));

        // Valid server provided historical bytes are blocked.
        assertTrue(areHistoricalBytesBlocked(firstHistoricalBytes));
        assertTrue(areHistoricalBytesBlocked(secondHistoricalBytes));

        // Random historical bytes are not blocked.
        assertFalse(areHistoricalBytesBlocked(new byte[] {}));
        assertFalse(areHistoricalBytesBlocked(new byte[] {0x01, 0x02, 0x03}));
    }
}
