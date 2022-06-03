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
import org.chromium.device.mojom.NdefRecordTypeCategory;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * Utility class that provides conversion between Android NdefMessage and Mojo NdefMessage data
 * structures.
 */
public final class NdefMessageUtils {
    public static final String RECORD_TYPE_EMPTY = "empty";
    public static final String RECORD_TYPE_TEXT = "text";
    public static final String RECORD_TYPE_URL = "url";
    public static final String RECORD_TYPE_ABSOLUTE_URL = "absolute-url";
    public static final String RECORD_TYPE_MIME = "mime";
    public static final String RECORD_TYPE_UNKNOWN = "unknown";
    public static final String RECORD_TYPE_SMART_POSTER = "smart-poster";

    private static final String ENCODING_UTF8 = "utf-8";
    private static final String ENCODING_UTF16 = "utf-16";

    /**
     * NFC Forum "URI Record Type Definition"
     * This is a mapping of "URI Identifier Codes" to URI string prefixes,
     * per section 3.2.2 of the NFC Forum URI Record Type Definition document.
     */
    private static final String[] URI_PREFIX_MAP = new String[] {
            "", // 0x00
            "http://www.", // 0x01
            "https://www.", // 0x02
            "http://", // 0x03
            "https://", // 0x04
            "tel:", // 0x05
            "mailto:", // 0x06
            "ftp://anonymous:anonymous@", // 0x07
            "ftp://ftp.", // 0x08
            "ftps://", // 0x09
            "sftp://", // 0x0A
            "smb://", // 0x0B
            "nfs://", // 0x0C
            "ftp://", // 0x0D
            "dav://", // 0x0E
            "news:", // 0x0F
            "telnet://", // 0x10
            "imap:", // 0x11
            "rtsp://", // 0x12
            "urn:", // 0x13
            "pop:", // 0x14
            "sip:", // 0x15
            "sips:", // 0x16
            "tftp:", // 0x17
            "btspp://", // 0x18
            "btl2cap://", // 0x19
            "btgoep://", // 0x1A
            "tcpobex://", // 0x1B
            "irdaobex://", // 0x1C
            "file://", // 0x1D
            "urn:epc:id:", // 0x1E
            "urn:epc:tag:", // 0x1F
            "urn:epc:pat:", // 0x20
            "urn:epc:raw:", // 0x21
            "urn:epc:", // 0x22
            "urn:nfc:", // 0x23
    };

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
        NdefMessage message = new NdefMessage();
        List<NdefRecord> records = new ArrayList<NdefRecord>();

        for (int i = 0; i < ndefRecords.length; i++) {
            NdefRecord record = toNdefRecord(ndefRecords[i]);
            if (record != null) records.add(record);
        }

        message.data = new NdefRecord[records.size()];
        records.toArray(message.data);
        return message;
    }

    /**
     * Converts mojo NdefRecord to android.nfc.NdefRecord
     * |record.data| can safely be treated as "UTF-8" encoding bytes for non text records, this is
     * guaranteed by the sender (Blink).
     */
    private static android.nfc.NdefRecord toNdefRecord(NdefRecord record)
            throws InvalidNdefMessageException, IllegalArgumentException,
                   UnsupportedEncodingException {
        if (record.category == NdefRecordTypeCategory.STANDARDIZED) {
            switch (record.recordType) {
                case RECORD_TYPE_URL:
                    return createPlatformUrlRecord(record.data, record.id, false /* isAbsUrl */);
                case RECORD_TYPE_ABSOLUTE_URL:
                    return createPlatformUrlRecord(record.data, record.id, true /* isAbsUrl */);
                case RECORD_TYPE_TEXT:
                    return createPlatformTextRecord(
                            record.id, record.lang, record.encoding, record.data);
                case RECORD_TYPE_MIME:
                    return createPlatformMimeRecord(record.mediaType, record.id, record.data);
                case RECORD_TYPE_UNKNOWN:
                    return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_UNKNOWN,
                            null /* type */,
                            record.id == null ? null
                                              : ApiCompatibilityUtils.getBytesUtf8(record.id),
                            record.data);
                case RECORD_TYPE_EMPTY:
                    return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_EMPTY,
                            null /* type */, null /* id */, null /* payload */);
                case RECORD_TYPE_SMART_POSTER:
                    return createPlatformSmartPosterRecord(record.id, record.payloadMessage);
            }
            throw new InvalidNdefMessageException();
        }

        if (record.category == NdefRecordTypeCategory.EXTERNAL) {
            // It's impossible for a valid record to have non-empty |data| and non-null
            // |payloadMessage| at the same time.
            if (isValidExternalType(record.recordType)
                    && (record.data.length == 0 || record.payloadMessage == null)) {
                return createPlatformExternalRecord(
                        record.recordType, record.id, record.data, record.payloadMessage);
            }
            throw new InvalidNdefMessageException();
        }

        if (record.category == NdefRecordTypeCategory.LOCAL) {
            // It's impossible for a local type record to have non-empty |data| and non-null
            // |payloadMessage| at the same time.
            // TODO(https://crbug.com/520391): Validate the containing ndef message is the payload
            // of another ndef record.
            if (isValidLocalType(record.recordType)
                    && (record.data.length == 0 || record.payloadMessage == null)) {
                // The prefix ':' in |record.recordType| is only used to differentiate local type
                // from other type names, remove it before writing.
                return createPlatformLocalRecord(record.recordType.substring(1), record.id,
                        record.data, record.payloadMessage);
            }
            throw new InvalidNdefMessageException();
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
        if ((record != null) && (ndefRecord.getTnf() != android.nfc.NdefRecord.TNF_EMPTY)) {
            record.id = new String(ndefRecord.getId(), "UTF-8");
        }
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
        nfcRecord.category = NdefRecordTypeCategory.STANDARDIZED;
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
        nfcRecord.category = NdefRecordTypeCategory.STANDARDIZED;
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
        nfcRecord.category = NdefRecordTypeCategory.STANDARDIZED;
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
        nfcRecord.category = NdefRecordTypeCategory.STANDARDIZED;
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
     * Constructs smart-poster NdefRecord
     */
    private static NdefRecord createSmartPosterRecord(byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        nfcRecord.recordType = RECORD_TYPE_SMART_POSTER;
        nfcRecord.data = payload;
        nfcRecord.payloadMessage = getNdefMessageFromPayloadBytes(payload);
        return nfcRecord;
    }

    /**
     * Constructs local type NdefRecord
     */
    private static NdefRecord createLocalRecord(String localType, byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.category = NdefRecordTypeCategory.LOCAL;
        nfcRecord.recordType = localType;
        nfcRecord.data = payload;
        nfcRecord.payloadMessage = getNdefMessageFromPayloadBytes(payload);
        return nfcRecord;
    }

    /**
     * Constructs well known type (TEXT, URI or local type) NdefRecord
     */
    private static NdefRecord createWellKnownRecord(android.nfc.NdefRecord record)
            throws UnsupportedEncodingException {
        if (Arrays.equals(record.getType(), android.nfc.NdefRecord.RTD_URI)) {
            return createURLRecord(record.toUri(), false /* isAbsUrl */);
        }

        if (Arrays.equals(record.getType(), android.nfc.NdefRecord.RTD_TEXT)) {
            return createTextRecord(record.getPayload());
        }

        if (Arrays.equals(record.getType(), android.nfc.NdefRecord.RTD_SMART_POSTER)) {
            return createSmartPosterRecord(record.getPayload());
        }

        // Prefix the raw local type with ':' to differentiate from other type names in WebNFC APIs,
        // e.g. |localType| being "text" will become ":text" to differentiate from the standardized
        // "text" record.
        String recordType = ':' + new String(record.getType(), "UTF-8");
        // We do not validate if we're in the context of a parent record but just expose to JS as is
        // what has been read from the nfc tag.
        if (isValidLocalType(recordType)) {
            return createLocalRecord(recordType, record.getPayload());
        }

        return null;
    }

    /**
     * Constructs unknown known type NdefRecord
     */
    private static NdefRecord createUnknownRecord(byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        nfcRecord.recordType = RECORD_TYPE_UNKNOWN;
        nfcRecord.data = payload;
        return nfcRecord;
    }

    /**
     * Constructs External type NdefRecord
     */
    private static NdefRecord createExternalTypeRecord(String type, byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.category = NdefRecordTypeCategory.EXTERNAL;
        nfcRecord.recordType = type;
        nfcRecord.data = payload;
        nfcRecord.payloadMessage = getNdefMessageFromPayloadBytes(payload);
        return nfcRecord;
    }

    /**
     * Creates a TNF_WELL_KNOWN + RTD_URI or TNF_ABSOLUTE_URI android.nfc.NdefRecord.
     */
    public static android.nfc.NdefRecord createPlatformUrlRecord(
            byte[] url, String id, boolean isAbsUrl) throws UnsupportedEncodingException {
        Uri uri = Uri.parse(new String(url, "UTF-8"));
        assert uri != null;
        uri = uri.normalizeScheme();
        String uriString = uri.toString();
        if (uriString.length() == 0) throw new IllegalArgumentException("uri is empty");
        byte[] idBytes = id == null ? null : ApiCompatibilityUtils.getBytesUtf8(id);

        if (isAbsUrl) {
            return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_ABSOLUTE_URI,
                    ApiCompatibilityUtils.getBytesUtf8(uriString), idBytes, null /* payload */);
        }

        // We encode the URI in the same way with android.nfc.NdefRecord.createUri(), per NFC Forum
        // URI Record Type Definition document.
        byte prefix = 0;
        for (int i = 1; i < URI_PREFIX_MAP.length; i++) {
            if (uriString.startsWith(URI_PREFIX_MAP[i])) {
                prefix = (byte) i;
                uriString = uriString.substring(URI_PREFIX_MAP[i].length());
                break;
            }
        }
        byte[] uriBytes = ApiCompatibilityUtils.getBytesUtf8(uriString);
        byte[] recordBytes = new byte[uriBytes.length + 1];
        recordBytes[0] = prefix;
        System.arraycopy(uriBytes, 0, recordBytes, 1, uriBytes.length);
        return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_WELL_KNOWN,
                android.nfc.NdefRecord.RTD_URI, idBytes, recordBytes);
    }

    /**
     * Creates a TNF_WELL_KNOWN + RTD_TEXT android.nfc.NdefRecord.
     */
    public static android.nfc.NdefRecord createPlatformTextRecord(String id, String lang,
            String encoding, byte[] text) throws UnsupportedEncodingException {
        // Blink always send us valid |lang| and |encoding|, we check them here against compromised
        // data.
        if (lang == null || lang.isEmpty()) throw new IllegalArgumentException("lang is invalid");
        if (encoding == null || encoding.isEmpty()) {
            throw new IllegalArgumentException("encoding is invalid");
        }

        byte[] languageCodeBytes = lang.getBytes(StandardCharsets.US_ASCII);
        // We only have 6 bits in the status byte (the first byte of the payload) to indicate length
        // of ISO/IANA language code. Blink already guarantees the length is less than 63, we check
        // here against compromised data.
        if (languageCodeBytes.length >= 64) {
            throw new IllegalArgumentException("language code is too long, must be <64 bytes.");
        }
        byte status = (byte) languageCodeBytes.length;
        if (!encoding.equals(ENCODING_UTF8)) {
            status |= (byte) (1 << 7);
        }

        ByteBuffer buffer = ByteBuffer.allocate(1 + languageCodeBytes.length + text.length);
        buffer.put(status);
        buffer.put(languageCodeBytes);
        buffer.put(text);
        return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_WELL_KNOWN,
                android.nfc.NdefRecord.RTD_TEXT,
                id == null ? null : ApiCompatibilityUtils.getBytesUtf8(id), buffer.array());
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
     * Creates a TNF_EXTERNAL_TYPE android.nfc.NdefRecord.
     */
    public static android.nfc.NdefRecord createPlatformExternalRecord(
            String recordType, String id, byte[] payload, NdefMessage payloadMessage) {
        // Already guaranteed by the caller.
        assert recordType != null && !recordType.isEmpty();

        // |payloadMessage| being non-null means this record has an NDEF message as its payload.
        if (payloadMessage != null) {
            // Should be guaranteed by the caller that |payload| is an empty byte array.
            assert payload.length == 0;
            payload = getBytesFromPayloadNdefMessage(payloadMessage);
        }

        // NFC Forum requires that the domain and type used in an external record are treated as
        // case insensitive, however Android intent filtering is always case sensitive. So we force
        // the domain and type to lower-case here and later we will compare in a case insensitive
        // way when filtering by them.
        return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_EXTERNAL_TYPE,
                ApiCompatibilityUtils.getBytesUtf8(recordType.toLowerCase(Locale.ROOT)),
                id == null ? null : ApiCompatibilityUtils.getBytesUtf8(id), payload);
    }

    /**
     * Creates a TNF_WELL_KNOWN + RTD_SMART_POSTER android.nfc.NdefRecord.
     */
    public static android.nfc.NdefRecord createPlatformSmartPosterRecord(
            String id, NdefMessage payloadMessage) throws InvalidNdefMessageException {
        if (payloadMessage == null) {
            throw new InvalidNdefMessageException();
        }

        List<android.nfc.NdefRecord> records = new ArrayList<android.nfc.NdefRecord>();
        boolean hasUrlRecord = false;
        boolean hasSizeRecord = false;
        boolean hasTypeRecord = false;
        boolean hasActionRecord = false;
        for (int i = 0; i < payloadMessage.data.length; ++i) {
            NdefRecord record = payloadMessage.data[i];
            if (record.recordType.equals("url")) {
                // The single mandatory url record.
                if (hasUrlRecord) {
                    throw new InvalidNdefMessageException();
                }
                hasUrlRecord = true;
            } else if (record.recordType.equals(":s")) {
                // Zero or one size record.
                // Size record must contain a 4-byte 32 bit unsigned integer.
                if (hasSizeRecord || record.data.length != 4) {
                    throw new InvalidNdefMessageException();
                }
                hasSizeRecord = true;
            } else if (record.recordType.equals(":t")) {
                // Zero or one type record.
                if (hasTypeRecord) {
                    throw new InvalidNdefMessageException();
                }
                hasTypeRecord = true;
            } else if (record.recordType.equals(":act")) {
                // Zero or one action record.
                // Action record must contain only a single byte.
                if (hasActionRecord || record.data.length != 1) {
                    throw new InvalidNdefMessageException();
                }
                hasActionRecord = true;
            } else {
                // No restriction on other record types.
            }

            try {
                records.add(toNdefRecord(payloadMessage.data[i]));
            } catch (UnsupportedEncodingException | InvalidNdefMessageException
                    | IllegalArgumentException e) {
                throw new InvalidNdefMessageException();
            }
        }

        // The single url record is mandatory.
        if (!hasUrlRecord) {
            throw new InvalidNdefMessageException();
        }

        android.nfc.NdefRecord[] ndefRecords = new android.nfc.NdefRecord[records.size()];
        records.toArray(ndefRecords);
        android.nfc.NdefMessage ndefMessage = new android.nfc.NdefMessage(ndefRecords);

        return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_WELL_KNOWN,
                android.nfc.NdefRecord.RTD_SMART_POSTER,
                id == null ? null : ApiCompatibilityUtils.getBytesUtf8(id),
                ndefMessage.toByteArray());
    }

    /**
     * Creates a TNF_WELL_KNOWN + |recordType| android.nfc.NdefRecord.
     */
    public static android.nfc.NdefRecord createPlatformLocalRecord(
            String recordType, String id, byte[] payload, NdefMessage payloadMessage) {
        // Already guaranteed by the caller.
        assert recordType != null && !recordType.isEmpty();

        // |payloadMessage| being non-null means this record has an NDEF message as its payload.
        if (payloadMessage != null) {
            // Should be guaranteed by the caller that |payload| is an empty byte array.
            assert payload.length == 0;
            payload = getBytesFromPayloadNdefMessage(payloadMessage);
        }

        return new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_WELL_KNOWN,
                ApiCompatibilityUtils.getBytesUtf8(recordType),
                id == null ? null : ApiCompatibilityUtils.getBytesUtf8(id), payload);
    }

    /**
     * Validates external types.
     * https://w3c.github.io/web-nfc/#dfn-validate-external-type
     */
    private static boolean isValidExternalType(String input) {
        // Must be an ASCII string first.
        if (!Charset.forName("US-ASCII").newEncoder().canEncode(input)) return false;

        if (input.isEmpty() || input.length() > 255) return false;

        int colonIndex = input.indexOf(':');
        if (colonIndex == -1) return false;

        String domain = input.substring(0, colonIndex).trim();
        if (domain.isEmpty()) return false;
        // TODO(https://crbug.com/520391): Validate |domain|.

        String type = input.substring(colonIndex + 1).trim();
        if (type.isEmpty()) return false;
        if (!type.matches("[a-zA-Z0-9:!()+,\\-=@;$_*'.]+")) return false;

        return true;
    }

    /**
     * Validates local types.
     * https://w3c.github.io/web-nfc/#dfn-validate-local-type
     */
    private static boolean isValidLocalType(String input) {
        // Must be an ASCII string first.
        if (!Charset.forName("US-ASCII").newEncoder().canEncode(input)) return false;

        // The prefix ':' will be omitted when we actually write the record type into the nfc tag.
        // We're taking it into consideration for validating the length here.
        if (input.length() < 2 || input.length() > 256) return false;

        if (input.charAt(0) != ':') return false;
        if (!Character.isLowerCase(input.charAt(1)) && !Character.isDigit(input.charAt(1))) {
            return false;
        }

        // TODO(https://crbug.com/520391): Validate |input| is not equal to the record type of any
        // NDEF record defined in its containing NDEF message.

        return true;
    }

    /**
     * Tries to construct a android.nfc.NdefMessage from the raw bytes |payload| then converts it to
     * a Mojo NdefMessage and returns. Returns null for anything wrong.
     */
    private static NdefMessage getNdefMessageFromPayloadBytes(byte[] payload) {
        try {
            android.nfc.NdefMessage payloadMessage = new android.nfc.NdefMessage(payload);
            return toNdefMessage(payloadMessage);
        } catch (FormatException | UnsupportedEncodingException e) {
        }
        return null;
    }

    /**
     * Tries to convert the Mojo NdefMessage |payloadMessage| to an android.nfc.NdefMessage then
     * returns its raw bytes. Returns null for anything wrong.
     */
    private static byte[] getBytesFromPayloadNdefMessage(NdefMessage payloadMessage) {
        try {
            android.nfc.NdefMessage message = toNdefMessage(payloadMessage);
            return message.toByteArray();
        } catch (InvalidNdefMessageException e) {
        }
        return null;
    }
}
