// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.Objects;

/**
 * Represents a JavaScript message payload.
 * Currently only String and ArrayBuffer is supported.
 */
public final class MessagePayload {
    @MessagePayloadType private final int mType;
    @Nullable private final String mString;
    @Nullable private final byte[] mArrayBuffer;

    /**
     * Create a MessagePayload String type.
     * To keep backward compatibility, string can be null, then it's replaced to empty string in
     * JNI.
     */
    public MessagePayload(@Nullable String string) {
        mType = MessagePayloadType.STRING;
        mString = string;
        mArrayBuffer = null;
    }

    /** Create a MessagePayload ArrayBuffer type. */
    public MessagePayload(@NonNull byte[] arrayBuffer) {
        Objects.requireNonNull(arrayBuffer, "arrayBuffer cannot be null.");
        mType = MessagePayloadType.ARRAY_BUFFER;
        mArrayBuffer = arrayBuffer;
        mString = null;
    }

    @MessagePayloadType
    public int getType() {
        return mType;
    }

    @Nullable
    public String getAsString() {
        checkType(MessagePayloadType.STRING);
        return mString;
    }

    @NonNull
    public byte[] getAsArrayBuffer() {
        checkType(MessagePayloadType.ARRAY_BUFFER);
        Objects.requireNonNull(mArrayBuffer, "mArrayBuffer cannot be null.");
        return mArrayBuffer;
    }

    private void checkType(@MessagePayloadType int expectedType) {
        if (mType != expectedType) {
            throw new IllegalStateException(
                    "Expected "
                            + typeToString(expectedType)
                            + ", but type is "
                            + typeToString(mType));
        }
    }

    @NonNull
    public static String typeToString(@MessagePayloadType int type) {
        switch (type) {
            case MessagePayloadType.STRING:
                return "String";
            case MessagePayloadType.ARRAY_BUFFER:
                return "ArrayBuffer";
            case MessagePayloadType.INVALID:
                return "Invalid";
        }
        throw new IllegalArgumentException("Unknown type: " + type);
    }
}
