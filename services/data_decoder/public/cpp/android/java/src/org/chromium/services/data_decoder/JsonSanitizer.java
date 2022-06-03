// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.data_decoder;

import android.util.JsonReader;
import android.util.JsonToken;
import android.util.JsonWriter;
import android.util.MalformedJsonException;

import org.chromium.base.StreamUtil;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.io.IOException;
import java.io.StringReader;
import java.io.StringWriter;

/**
 * Sanitizes and normalizes a JSON string by parsing it, checking for wellformedness, and
 * serializing it again. This class is meant to be used from native code.
 */
@JNINamespace("data_decoder")
public class JsonSanitizer {
    // Disallow instantiating the class.
    private JsonSanitizer() {}

    /**
     * The maximum nesting depth to which the native JSON parser restricts input in order to avoid
     * stack overflows.
     */
    private static final int MAX_NESTING_DEPTH = 200;

    /**
     * Validates input JSON string and returns the sanitized version of the string that's safe to
     * parse.
     *
     * @param unsafeJson The input string to validate and sanitize.
     * @return The sanitized version of the input string.
     */
    public static String sanitize(String unsafeJson) throws IOException, IllegalStateException {
        JsonReader reader = new JsonReader(new StringReader(unsafeJson));
        StringWriter stringWriter = new StringWriter(unsafeJson.length());
        JsonWriter writer = new JsonWriter(stringWriter);
        StackChecker stackChecker = new StackChecker();
        String result = null;
        try {
            boolean end = false;
            while (!end) {
                JsonToken token = reader.peek();
                switch (token) {
                    case BEGIN_ARRAY:
                        stackChecker.increaseAndCheck();
                        reader.beginArray();
                        writer.beginArray();
                        break;
                    case END_ARRAY:
                        stackChecker.decrease();
                        reader.endArray();
                        writer.endArray();
                        break;
                    case BEGIN_OBJECT:
                        stackChecker.increaseAndCheck();
                        reader.beginObject();
                        writer.beginObject();
                        break;
                    case END_OBJECT:
                        stackChecker.decrease();
                        reader.endObject();
                        writer.endObject();
                        break;
                    case NAME:
                        writer.name(sanitizeString(reader.nextName()));
                        break;
                    case STRING:
                        writer.value(sanitizeString(reader.nextString()));
                        break;
                    case NUMBER: {
                        // Read the value as a string, then try to parse it first as a long, then as
                        // a double.
                        String value = reader.nextString();
                        try {
                            writer.value(Long.parseLong(value));
                        } catch (NumberFormatException e) {
                            writer.value(Double.parseDouble(value));
                        }
                        break;
                    }
                    case BOOLEAN:
                        writer.value(reader.nextBoolean());
                        break;
                    case NULL:
                        reader.nextNull();
                        writer.nullValue();
                        break;
                    case END_DOCUMENT:
                        end = true;
                        break;
                    default:
                        assert false : token;
                }
            }
            result = stringWriter.toString();
        } finally {
            StreamUtil.closeQuietly(reader);
            StreamUtil.closeQuietly(writer);
        }
        return result;
    }

    @CalledByNative
    public static void sanitize(long nativePtr, String unsafeJson) {
        String result = null;
        try {
            result = sanitize(unsafeJson);
        } catch (IOException | IllegalStateException e) {
            JsonSanitizerJni.get().onError(nativePtr, e.getMessage());
            return;
        }
        JsonSanitizerJni.get().onSuccess(nativePtr, result);
    }

    /**
     * Helper class to check nesting depth of JSON expressions.
     */
    private static class StackChecker {
        private int mStackDepth;

        public void increaseAndCheck() {
            if (++mStackDepth >= MAX_NESTING_DEPTH) {
                throw new IllegalStateException("Too much nesting");
            }
        }

        public void decrease() {
            mStackDepth--;
        }
    }

    private static String sanitizeString(String string) throws MalformedJsonException {
        if (!checkString(string)) {
            throw new MalformedJsonException("Invalid escape sequence");
        }
        return string;
    }

    /**
     * Checks whether a given String is well-formed UTF-16, i.e. all surrogates appear in high-low
     * pairs, in other words: each character is a valid Unicode code point.
     *
     * @param string The string to check.
     * @return Whether the given string is well-formed UTF-16.
     */
    private static boolean checkString(String string) {
        int length = string.length();
        for (int i = 0; i < length; i++) {
            char c = string.charAt(i);
            // Check that surrogates only appear in pairs of a high surrogate followed by a low
            // surrogate.
            // A lone surrogate is not allowed.
            if (Character.isLowSurrogate(c)) return false;

            int codePoint;
            if (Character.isHighSurrogate(c)) {
                // A high surrogate has to be followed by a low surrogate.
                char high = c;
                if (++i >= length) return false;

                char low = string.charAt(i);
                if (!Character.isLowSurrogate(low)) return false;

                // Decode the high-low pair into a code point.
                codePoint = Character.toCodePoint(high, low);
            } else {
                // The code point is neither a low surrogate nor a high surrogate, so
                // it's a valid Unicode character.
                codePoint = c;
            }
        }
        return true;
    }

    @NativeMethods
    interface Natives {
        void onSuccess(long id, String json);
        void onError(long id, String error);
    }
}
