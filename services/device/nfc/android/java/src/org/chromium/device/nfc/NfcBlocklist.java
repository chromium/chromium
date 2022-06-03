// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.nfc.Tag;
import android.nfc.tech.IsoDep;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.components.variations.VariationsAssociatedData;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Utility class that provides functions to tell whether tag should be accessible using Web NFC. The
 * blocklist was introduced to block access to YubiKeys. Without it, websites could use Web
 * NFC to read NDEF URI record in YubiKeys that includes the OTP code.
 */
public class NfcBlocklist {
    private static final String TAG = "NfcBlocklist";

    private static final byte[][] STATIC_HISTORICAL_BYTES = {
            new byte[] {(byte) 0x80, 0x73, (byte) 0xc0, 0x21, (byte) 0xc0, 0x57, 0x59, 0x75, 0x62,
                    0x69, 0x4b, 0x65, 0x79}, // YubiKey 5 series
            new byte[] {(byte) 0x59, 0x75, 0x62, 0x69, 0x6b, 0x65, 0x79, 0x4e, 0x45, 0x4f, 0x72,
                    0x33} // YubiKey NEO
    };

    private static final String TRIAL_NAME = "WebNFCBlockList";
    private static final String HISTORICAL_BYTES_PARAM_NAME = "historical_bytes_additions";

    private final List<byte[]> mServerProvidedHistoricalBytes = new ArrayList<byte[]>();

    private static NfcBlocklist sInstance;

    private Boolean mIsTagBlockedForTesting;

    public static NfcBlocklist getInstance() {
        if (sInstance == null) {
            sInstance = new NfcBlocklist();
        }
        return sInstance;
    }

    @VisibleForTesting
    public static void overrideNfcBlocklistForTests(String serverProvidedValues) {
        sInstance = new NfcBlocklist(serverProvidedValues);
    }

    private NfcBlocklist() {
        String serverProvidedValues = VariationsAssociatedData.getVariationParamValue(
                TRIAL_NAME, HISTORICAL_BYTES_PARAM_NAME);
        populateWithServerProvidedValues(serverProvidedValues);
    }

    @VisibleForTesting
    public NfcBlocklist(String serverProvidedValues) {
        populateWithServerProvidedValues(serverProvidedValues);
    }

    private void populateWithServerProvidedValues(String serverProvidedValues) {
        if (serverProvidedValues == null || serverProvidedValues.isEmpty()) return;

        String[] historicalBytesParamValues = serverProvidedValues.split(",");
        for (String historicalBytesString : historicalBytesParamValues) {
            // For example, "597562696B65794E454F7233" (no leading 0x)
            byte[] historicalBytes = hexStringToByteArray(historicalBytesString);
            if (historicalBytes != null) {
                mServerProvidedHistoricalBytes.add(historicalBytes);
            }
        }
    }

    private static byte[] hexStringToByteArray(String str) {
        int len = str.length();
        if (len % 2 == 1) {
            Log.w(TAG, "Length of %s is odd", str);
            return null;
        }
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            char hex1 = str.charAt(i);
            char hex2 = str.charAt(i + 1);
            if (Character.digit(hex1, 16) < 0 || Character.digit(hex2, 16) < 0) {
                Log.w(TAG, "Invalid hex character found in %s", str);
                return null;
            }
            data[i / 2] = (byte) ((Character.digit(hex1, 16) << 4) + Character.digit(hex2, 16));
        }
        return data;
    }

    /**
     * Returns true if tag is blocked, otherwise false. A tag is blocked if it is part of
     * STATIC_HISTORICAL_BYTES or server provided historical bytes.
     *
     * @param tag @see android.nfc.Tag
     * @return true if tag is blocked, otherwise false.
     */
    public boolean isTagBlocked(Tag tag) {
        if (mIsTagBlockedForTesting != null) {
            return mIsTagBlockedForTesting;
        }

        if (tag == null) return false;

        IsoDep iso = IsoDep.get(tag);
        if (iso != null) {
            byte[] historicalBytes = iso.getHistoricalBytes();
            if (areHistoricalBytesBlocked(historicalBytes)) {
                Log.w(TAG, "Access to NFC tag is blocked.");
                return true;
            }
        }

        return false;
    }

    /**
     * Returns true if historical bytes are part of
     * STATIC_HISTORICAL_BYTES or server provided historical bytes.
     *
     * @param byte[] historical bytes from a NFC tag
     * @return true if historical bytes are blocked, otherwise false.
     */
    @VisibleForTesting
    boolean areHistoricalBytesBlocked(byte[] historicalBytes) {
        for (int i = 0; i < STATIC_HISTORICAL_BYTES.length; i++) {
            if (Arrays.equals(historicalBytes, STATIC_HISTORICAL_BYTES[i])) return true;
        }
        for (int i = 0; i < mServerProvidedHistoricalBytes.size(); i++) {
            if (Arrays.equals(historicalBytes, mServerProvidedHistoricalBytes.get(i))) return true;
        }
        return false;
    }

    /** Block/unblock NFC tag access for testing use only. */
    @VisibleForTesting
    public void setIsTagBlockedForTesting(Boolean blocked) {
        mIsTagBlockedForTesting = blocked;
    }
}
