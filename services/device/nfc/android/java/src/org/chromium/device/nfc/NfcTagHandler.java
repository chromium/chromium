// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.nfc.FormatException;
import android.nfc.NdefMessage;
import android.nfc.Tag;
import android.nfc.TagLostException;
import android.nfc.tech.Ndef;
import android.nfc.tech.NdefFormatable;
import android.nfc.tech.TagTechnology;

import java.io.IOException;

/** Utility class that provides I/O operations for NFC tags. */
public class NfcTagHandler {
    private final TagTechnology mTech;
    private final TagTechnologyHandler mTechHandler;
    private boolean mWasConnected;
    private final String mSerialNumber;

    /**
     * Factory method that creates NfcTagHandler for a given NFC Tag.
     *
     * @see android.nfc.Tag
     * @return NfcTagHandler or null when unsupported Tag is provided.
     */
    public static NfcTagHandler create(Tag tag) {
        if (tag == null) return null;

        if (NfcBlocklist.getInstance().isTagBlocked(tag)) return null;

        Ndef ndef = Ndef.get(tag);
        if (ndef != null) {
            return new NfcTagHandler(ndef, new NdefHandler(ndef), tag.getId());
        }

        NdefFormatable formattable = NdefFormatable.get(tag);
        if (formattable != null) {
            return new NfcTagHandler(
                    formattable, new NdefFormattableHandler(formattable), tag.getId());
        }

        return null;
    }

    /**
     * NdefFormatable and Ndef interfaces have different signatures for operating with NFC tags.
     * This interface provides generic methods.
     */
    private interface TagTechnologyHandler {
        public void write(NdefMessage message)
                throws IOException, TagLostException, FormatException, IllegalStateException;

        public boolean makeReadOnly() throws IOException, TagLostException;

        public NdefMessage read()
                throws IOException, TagLostException, FormatException, IllegalStateException;

        public boolean canAlwaysOverwrite()
                throws IOException, TagLostException, FormatException, IllegalStateException;
    }

    /**
     * Implementation of TagTechnologyHandler that uses Ndef tag technology.
     * @see android.nfc.tech.Ndef
     */
    private static class NdefHandler implements TagTechnologyHandler {
        private final Ndef mNdef;

        NdefHandler(Ndef ndef) {
            mNdef = ndef;
        }

        @Override
        public void write(NdefMessage message)
                throws IOException, TagLostException, FormatException, IllegalStateException {
            mNdef.writeNdefMessage(message);
            mNdef.close();
        }

        @Override
        public boolean makeReadOnly() throws IOException, TagLostException {
            return mNdef.makeReadOnly();
        }

        @Override
        public NdefMessage read()
                throws IOException, TagLostException, FormatException, IllegalStateException {
            return mNdef.getNdefMessage();
        }

        @Override
        public boolean canAlwaysOverwrite()
                throws IOException, TagLostException, FormatException, IllegalStateException {
            // Getting null means the tag is empty, overwrite is safe.
            return mNdef.getNdefMessage() == null;
        }
    }

    /**
     * Implementation of TagTechnologyHandler that uses NdefFormatable tag technology.
     * @see android.nfc.tech.NdefFormatable
     */
    private static class NdefFormattableHandler implements TagTechnologyHandler {
        private final NdefFormatable mNdefFormattable;

        NdefFormattableHandler(NdefFormatable ndefFormattable) {
            mNdefFormattable = ndefFormattable;
        }

        @Override
        public void write(NdefMessage message)
                throws IOException, TagLostException, FormatException, IllegalStateException {
            mNdefFormattable.format(message);
        }

        @Override
        public boolean makeReadOnly() throws IOException, TagLostException {
            try {
                mNdefFormattable.formatReadOnly(NdefMessageUtils.emptyNdefMessage());
            } catch (FormatException e) {
                return false;
            }
            return true;
        }

        @Override
        public NdefMessage read() throws FormatException {
            return NdefMessageUtils.emptyNdefMessage();
        }

        @Override
        public boolean canAlwaysOverwrite() {
            return true;
        }
    }

    protected NfcTagHandler(TagTechnology tech, TagTechnologyHandler handler, byte[] id) {
        mTech = tech;
        mTechHandler = handler;
        mSerialNumber = bytesToSerialNumber(id);
    }

    /** Convert byte array to serial number string (4-7 ASCII hex digits concatenated by ":"). */
    private static String bytesToSerialNumber(byte[] octets) {
        if (octets.length < 0) return null;

        StringBuilder sb = new StringBuilder(octets.length * 3);
        for (byte b : octets) {
            if (sb.length() > 0) {
                sb.append(":");
            }
            sb.append(String.format("%02x", b & 0xff));
        }
        return sb.toString();
    }

    /** Get the serial number of this NFC tag. */
    public String serialNumber() {
        return mSerialNumber;
    }

    /** Connects to NFC tag. */
    public void connect() throws IOException, SecurityException, TagLostException {
        if (!mTech.isConnected()) {
            mTech.connect();
            mWasConnected = true;
        }
    }

    /** Writes NdefMessage to NFC tag. */
    public void write(NdefMessage message)
            throws IOException, TagLostException, FormatException, IllegalStateException {
        mTechHandler.write(message);
    }

    /** Make NFC tag read-only. */
    public boolean makeReadOnly() throws IOException, TagLostException {
        return mTechHandler.makeReadOnly();
    }

    public NdefMessage read()
            throws IOException, TagLostException, FormatException, IllegalStateException {
        return mTechHandler.read();
    }

    /**
     * If tag was previously connected and subsequent connection to the same tag fails, consider
     * tag to be out of range.
     */
    public boolean isTagOutOfRange() {
        try {
            connect();
        } catch (IOException | SecurityException e) {
            return mWasConnected;
        }
        return false;
    }

    /**
     * Returns false only if the tag is already NDEF formatted and contains some records. Otherwise
     * true.
     */
    public boolean canAlwaysOverwrite()
            throws IOException, TagLostException, FormatException, IllegalStateException {
        return mTechHandler.canAlwaysOverwrite();
    }
}
