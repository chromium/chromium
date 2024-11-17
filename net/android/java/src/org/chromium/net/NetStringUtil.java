// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CodingErrorAction;
import java.text.Normalizer;
import java.util.Locale;

/** Utility functions for converting strings between formats when not built with icu. */
@JNINamespace("net::android")
public class NetStringUtil {
    /**
     * Attempts to convert text in a given character set to a Unicode string. Returns null on
     * failure.
     *
     * @param text ByteBuffer containing the character array to convert.
     * @param charsetName Character set it's in encoded in.
     * @return Unicode string on success, null on failure.
     */
    @CalledByNative
    private static String convertToUnicode(ByteBuffer text, String charsetName) {
        try {
            Charset charset = Charset.forName(charsetName);
            CharsetDecoder decoder = charset.newDecoder();
            // On invalid characters, this will throw an exception.
            return decoder.decode(text).toString();
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Attempts to convert text in a given character set to a Unicode string, and normalize it.
     * Returns null on failure.
     *
     * @param text ByteBuffer containing the character array to convert.
     * @param charsetName Character set it's in encoded in.
     * @return Unicode string on success, null on failure.
     */
    @CalledByNative
    private static String convertToUnicodeAndNormalize(ByteBuffer text, String charsetName) {
        String unicodeString = convertToUnicode(text, charsetName);
        if (unicodeString == null) return null;
        return Normalizer.normalize(unicodeString, Normalizer.Form.NFC);
    }

    /**
     * Convert text in a given character set to a Unicode string. Any invalid characters are
     * replaced with U+FFFD. Returns null if the character set is not recognized.
     *
     * @param text ByteBuffer containing the character array to convert.
     * @param charsetName Character set it's in encoded in.
     * @return Unicode string on success, null on failure.
     */
    @CalledByNative
    private static String convertToUnicodeWithSubstitutions(ByteBuffer text, String charsetName) {
        try {
            Charset charset = Charset.forName(charsetName);

            // TODO(mmenke):  Investigate if Charset.decode() can be used
            // instead.  The question is whether it uses the proper replace
            // character.  JDK CharsetDecoder docs say U+FFFD is the default,
            // but Charset.decode() docs say it uses the "charset's default
            // replacement byte array".
            CharsetDecoder decoder = charset.newDecoder();
            decoder.onMalformedInput(CodingErrorAction.REPLACE);
            decoder.onUnmappableCharacter(CodingErrorAction.REPLACE);
            decoder.replaceWith("\uFFFD");
            return decoder.decode(text).toString();
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Convert a string to uppercase.
     *
     * @param str String to convert.
     * @return String converted to uppercase using default locale, null on failure.
     */
    @CalledByNative
    private static String toUpperCase(String str) {
        try {
            return str.toUpperCase(Locale.getDefault());
        } catch (Exception e) {
            return null;
        }
    }
}
