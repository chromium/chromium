// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.content.Intent;
import android.net.Uri;
import android.nfc.FormatException;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefRecord;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Utility class that provides conversion between Android NdefMessage and Mojo NdefMessage data
 * structures.
 */
public final class NdefMessageUtils {
    private static final String TAG = "NdefMessageUtils";
    private static final String AUTHOR_RECORD_DOMAIN = "w3.org";
    private static final String AUTHOR_RECORD_TYPE = "A";
    private static final String ENCODING_UTF8 = "utf-8";
    private static final String ENCODING_UTF16 = "utf-16";

    public static final String RECORD_TYPE_EMPTY = "empty";
    public static final String RECORD_TYPE_TEXT = "text";
    public static final String RECORD_TYPE_URL = "url";
    public static final String RECORD_TYPE_ABSOLUTE_URL = "absolute-url";
    public static final String RECORD_TYPE_MIME = "mime";
    public static final String RECORD_TYPE_UNKNOWN = "unknown";
    public static final String RECORD_TYPE_SMART_POSTER = "smart-poster";

    private static class PairOfDomainAndType {
        private String mDomain;
        private String mType;

        private PairOfDomainAndType(String domain, String type) {
            mDomain = domain;
            mType = type;
        }
    }

    /**
     * Converts mojo NdefMessage to android.nfc.NdefMessage
     */
    public static android.nfc.NdefMessage toNdefMessage(NdefMessage message)
            throws InvalidNdefMessageException {
        try {
            List<android.nfc.NdefRecord> records = new ArrayList<android.nfc.NdefRecord>();
            for (int i = 0; i < message.data.length; ++i) {
                records.add(toNdefRecord(message.data[i]));
            }
            // NdefRecord.createExternal() will internally convert both the domain and type to
            // lower-case. Details: https://github.com/w3c/web-nfc/issues/308
            records.add(android.nfc.NdefRecord.createExternal(AUTHOR_RECORD_DOMAIN,
                    AUTHOR_RECORD_TYPE, ApiCompatibilityUtils.getBytesUtf8(message.url)));
            android.nfc.NdefRecord[] ndefRecords = new android.nfc.NdefRecord[records.size()];
            records.toArray(ndefRecords);
            return new android.nfc.NdefMessage(ndefRecords);
        } catch (UnsupportedEncodingException | InvalidNdefMessageException
                | IllegalArgumentException e) {
            throw new InvalidNdefMessageException();
        }
    }

    /**
     * Converts android.nfc.NdefMessage to mojo NdefMessage
     */
    public static NdefMessage toNdefMessage(android.nfc.NdefMessage ndefMessage)
            throws UnsupportedEncodingException {
        android.nfc.NdefRecord[] ndefRecords = ndefMessage.getRecords();
        NdefMessage webNdefMessage = new NdefMessage();
        List<NdefRecord> nfcRecords = new ArrayList<NdefRecord>();

        for (int i = 0; i < ndefRecords.length; i++) {
            // NFC Forum requires that the domain and type used in an external record are treated as
            // case insensitive, so we compare while ignoring the case.
            if ((ndefRecords[i].getTnf() == android.nfc.NdefRecord.TNF_EXTERNAL_TYPE)
                    && new String(ndefRecords[i].getType(), "UTF-8")
                                    .compareToIgnoreCase(
                                            AUTHOR_RECORD_DOMAIN + ":" + AUTHOR_RECORD_TYPE)
                            == 0) {
                webNdefMessage.url = new String(ndefRecords[i].getPayload(), "UTF-8");
                continue;
            }

            NdefRecord nfcRecord = toNdefRecord(ndefRecords[i]);
            if (nfcRecord != null) nfcRecords.add(nfcRecord);
        }

        webNdefMessage.data = new NdefRecord[nfcRecords.size()];
        nfcRecords.toArray(webNdefMessage.data);
        return webNdefMessage;
    }

    /**
     * Converts mojo NdefRecord to android.nfc.NdefRecord
     * |record.data| can safely be treated as "UTF-8" encoding bytes for non text records, this is
     * guaranteed by the sender (Blink).
     */
    private static android.nfc.NdefRecord toNdefRecord(NdefRecord record)
            throws InvalidNdefMessageException, IllegalArgumentException,
                   UnsupportedEncodingException {
        switch (record.recordType) {
            case RECORD_TYPE_URL:
                return createPlatformUrlRecord(record.data, false /* isAbsUrl */);
            case RECORD_TYPE_ABSOLUTE_URL:
                return createPlatformUrlRecord(record.data, true /* isAbsUrl */);
            case RECORD_TYPE_TEXT:
                byte[] payload = createPayloadForTextRecord(record);
                return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_WELL_KNOWN,
                        android.nfc.NdefRecord.RTD_TEXT, null, payload);
            case RECORD_TYPE_MIME:
                return createPlatformMimeRecord(record.mediaType, record.id, record.data);
            case RECORD_TYPE_UNKNOWN:
                return new android.nfc.NdefRecord(
                        android.nfc.NdefRecord.TNF_UNKNOWN, null, null, record.data);
            case RECORD_TYPE_EMPTY:
                return new android.nfc.NdefRecord(
                        android.nfc.NdefRecord.TNF_EMPTY, null, null, null);
            case RECORD_TYPE_SMART_POSTER:
                // TODO(https://crbug.com/520391): Support 'smart-poster' type records.
                throw new InvalidNdefMessageException();
        }
        // TODO(https://crbug.com/520391): Need to create an external record for either a custom
        // type name or a local type name (for an embedded record).
        PairOfDomainAndType pair = parseDomainAndType(record.recordType);
        if (pair != null) {
            return android.nfc.NdefRecord.createExternal(pair.mDomain, pair.mType, record.data);
        }

        throw new InvalidNdefMessageException();
    }

    /**
     * Converts android.nfc.NdefRecord to mojo NdefRecord
     */
    private static NdefRecord toNdefRecord(android.nfc.NdefRecord ndefRecord)
            throws UnsupportedEncodingException {
        NdefRecord record = null;
        switch (ndefRecord.getTnf()) {
            case android.nfc.NdefRecord.TNF_EMPTY:
                record = createEmptyRecord();
                break;
            case android.nfc.NdefRecord.TNF_MIME_MEDIA:
                record = createMIMERecord(
                        new String(ndefRecord.getType(), "UTF-8"), ndefRecord.getPayload());
                break;
            case android.nfc.NdefRecord.TNF_ABSOLUTE_URI:
                record = createURLRecord(ndefRecord.toUri(), true /* isAbsUrl */);
                break;
            case android.nfc.NdefRecord.TNF_WELL_KNOWN:
                record = createWellKnownRecord(ndefRecord);
                break;
            case android.nfc.NdefRecord.TNF_UNKNOWN:
                record = createUnknownRecord(ndefRecord.getPayload());
                break;
            case android.nfc.NdefRecord.TNF_EXTERNAL_TYPE:
                record = createExternalTypeRecord(
                        new String(ndefRecord.getType(), "UTF-8"), ndefRecord.getPayload());
                break;
        }
        record.id = new String(ndefRecord.getId(), "UTF-8");
        return record;
    }

    /**
     * Constructs empty NdefMessage
     */
    public static android.nfc.NdefMessage emptyNdefMessage() {
        return new android.nfc.NdefMessage(
                new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_EMPTY, null, null, null));
    }

    /**
     * Constructs empty NdefRecord
     */
    private static NdefRecord createEmptyRecord() {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = RECORD_TYPE_EMPTY;
        nfcRecord.data = new byte[0];
        return nfcRecord;
    }

    /**
     * Constructs url NdefRecord
     */
    private static NdefRecord createURLRecord(Uri uri, boolean isAbsUrl) {
        if (uri == null) return null;
        NdefRecord nfcRecord = new NdefRecord();
        if (isAbsUrl) {
            nfcRecord.recordType = RECORD_TYPE_ABSOLUTE_URL;
        } else {
            nfcRecord.recordType = RECORD_TYPE_URL;
        }
        nfcRecord.data = ApiCompatibilityUtils.getBytesUtf8(uri.toString());
        return nfcRecord;
    }

    /**
    /**
     * Constructs mime NdefRecord
     */
    private static NdefRecord createMIMERecord(String mediaType, byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = RECORD_TYPE_MIME;
        nfcRecord.mediaType = mediaType;
        nfcRecord.data = payload;
        return nfcRecord;
    }

    /**
     * Constructs TEXT NdefRecord
     */
    private static NdefRecord createTextRecord(byte[] text) throws UnsupportedEncodingException {
        // Check that text byte array is not empty.
        if (text.length == 0) {
            return null;
        }

        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = RECORD_TYPE_TEXT;
        // According to NFCForum-TS-RTD_Text_1.0 specification, section 3.2.1 Syntax.
        // First byte of the payload is status byte, defined in Table 3: Status Byte Encodings.
        // 0-5: lang code length
        // 6  : must be zero
        // 7  : 0 - text is in UTF-8 encoding, 1 - text is in UTF-16 encoding.
        nfcRecord.encoding = (text[0] & (1 << 7)) == 0 ? ENCODING_UTF8 : ENCODING_UTF16;
        int langCodeLength = (text[0] & (byte) 0x3F);
        nfcRecord.lang = new String(text, 1, langCodeLength, "US-ASCII");
        int textBodyStartPos = langCodeLength + 1;
        if (textBodyStartPos > text.length) {
            return null;
        }
        nfcRecord.data = Arrays.copyOfRange(text, textBodyStartPos, text.length);
        return nfcRecord;
    }

    /**
     * Constructs well known type (TEXT or URI) NdefRecord
     */
    private static NdefRecord createWellKnownRecord(android.nfc.NdefRecord record)
            throws UnsupportedEncodingException {
        if (Arrays.equals(record.getType(), android.nfc.NdefRecord.RTD_URI)) {
            return createURLRecord(record.toUri(), false /* isAbsUrl */);
        }

        if (Arrays.equals(record.getType(), android.nfc.NdefRecord.RTD_TEXT)) {
            return createTextRecord(record.getPayload());
        }

        // TODO(https://crbug.com/520391): Support RTD_SMART_POSTER type records.

        return null;
    }

    /**
     * Constructs unknown known type NdefRecord
     */
    private static NdefRecord createUnknownRecord(byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = RECORD_TYPE_UNKNOWN;
        nfcRecord.data = payload;
        return nfcRecord;
    }

    /**
     * Constructs External type NdefRecord
     */
    private static NdefRecord createExternalTypeRecord(String type, byte[] payload) {
        // |type| may be a custom type name or a local type name (for an embedded record).
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = type;
        nfcRecord.data = payload;
        nfcRecord.payloadMessage = getNdefMessageFromPayload(payload);
        return nfcRecord;
    }

    /**
     * Creates a TNF_WELL_KNOWN + RTD_URI or TNF_ABSOLUTE_URI android.nfc.NdefRecord.
     */
    public static android.nfc.NdefRecord createPlatformUrlRecord(byte[] url, boolean isAbsUrl)
            throws UnsupportedEncodingException {
        if (isAbsUrl) {
            Uri uri = Uri.parse(new String(url, "UTF-8"));
            if (uri == null) throw new NullPointerException("uri is null");
            uri = uri.normalizeScheme();
            String uriString = uri.toString();
            if (uriString.length() == 0) throw new IllegalArgumentException("uri is empty");
            return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_ABSOLUTE_URI,
                    ApiCompatibilityUtils.getBytesUtf8(uriString), null, null);
        }
        return android.nfc.NdefRecord.createUri(new String(url, "UTF-8"));
    }

    /**
     * Creates a TNF_MIME_MEDIA android.nfc.NdefRecord.
     */
    public static android.nfc.NdefRecord createPlatformMimeRecord(
            String mimeType, String id, byte[] payload) {
        // Already verified by NdefMessageValidator.
        assert mimeType != null && !mimeType.isEmpty();

        // We only do basic MIME type validation: trying to follow the
        // RFCs strictly only ends in tears, since there are lots of MIME
        // types in common use that are not strictly valid as per RFC rules.
        mimeType = Intent.normalizeMimeType(mimeType);
        if (mimeType.length() == 0) throw new IllegalArgumentException("mimeType is empty");
        int slashIndex = mimeType.indexOf('/');
        if (slashIndex == 0) throw new IllegalArgumentException("mimeType must have major type");
        if (slashIndex == mimeType.length() - 1) {
            throw new IllegalArgumentException("mimeType must have minor type");
        }
        // missing '/' is allowed

        return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_MIME_MEDIA,
                ApiCompatibilityUtils.getBytesUtf8(mimeType),
                id == null ? null : ApiCompatibilityUtils.getBytesUtf8(id), payload);
    }

    /**
     * Parses the input custom type to get its domain and type.
     * e.g. returns a pair ('w3.org', 'xyz') for the input 'w3.org:xyz'.
     * Returns null for invalid input.
     * https://w3c.github.io/web-nfc/#the-ndefrecordtype-string
     *
     * TODO(https://crbug.com/520391): Refine the validation algorithm here accordingly once there
     * is a conclusion on some case-sensitive things at https://github.com/w3c/web-nfc/issues/331.
     */
    private static PairOfDomainAndType parseDomainAndType(String customType) {
        int colonIndex = customType.indexOf(':');
        if (colonIndex == -1) return null;

        // TODO(ThisCL): verify |domain| is a valid FQDN, asking help at
        // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/QN2mHt_WgHo.
        String domain = customType.substring(0, colonIndex);
        if (domain.isEmpty()) return null;

        String type = customType.substring(colonIndex + 1);
        if (type.isEmpty()) return null;
        if (!type.matches("[a-zA-Z0-9()+,\\-:=@;$_!*'.]+")) return null;

        return new PairOfDomainAndType(domain, type);
    }

    private static byte[] createPayloadForTextRecord(NdefRecord record)
            throws UnsupportedEncodingException {
        byte[] languageCodeBytes = record.lang.getBytes(StandardCharsets.US_ASCII);
        ByteBuffer buffer = ByteBuffer.allocate(1 + languageCodeBytes.length + record.data.length);
        // Lang length is always less than 64 as it is guaranteed by Blink.
        byte status = (byte) languageCodeBytes.length;
        if (!record.encoding.equals(ENCODING_UTF8)) {
            status |= (byte) (1 << 7);
        }
        buffer.put(status);
        buffer.put(languageCodeBytes);
        buffer.put(record.data);
        return buffer.array();
    }

    /**
     * Tries to construct a android.nfc.NdefMessage from the raw bytes |payload| then converts it to
     * a Mojo NdefMessage and returns. Returns null for anything wrong.
     */
    private static NdefMessage getNdefMessageFromPayload(byte[] payload) {
        try {
            android.nfc.NdefMessage payloadMessage = new android.nfc.NdefMessage(payload);
            return toNdefMessage(payloadMessage);
        } catch (FormatException | UnsupportedEncodingException e) {
        }
        return null;
    }
}
