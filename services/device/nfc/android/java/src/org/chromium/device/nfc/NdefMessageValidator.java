// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefRecord;

/** Utility class that provides validation of NdefMessage. */
public final class NdefMessageValidator {
    /**
     * Validates NdefMessage.
     *
     * @param message to be validated.
     * @return true if message is valid, false otherwise.
     */
    public static boolean isValid(NdefMessage message) {
        if (message == null || message.data == null || message.data.length == 0) {
            return false;
        }

        for (int i = 0; i < message.data.length; ++i) {
            if (!isValid(message.data[i])) return false;
        }
        return true;
    }

    /**
     * Checks that NdefRecord#data and NdefRecord#mediaType fields are valid. NdefRecord#data and
     * NdefRecord#mediaType fields are omitted for the record with EMPTY type.
     */
    private static boolean isValid(NdefRecord record) {
        if (record == null) return false;
        if (record.recordType.equals(NdefMessageUtils.RECORD_TYPE_EMPTY)) return true;
        if (record.data == null) return false;
        if (record.recordType.equals(NdefMessageUtils.RECORD_TYPE_MIME)) {
            // 'mime' type records must have mediaType.
            if (record.mediaType == null || record.mediaType.isEmpty()) {
                return false;
            }
        } else if (record.mediaType != null) {
            // Other types must not have mediaType.
            return false;
        }
        return true;
    }
}
