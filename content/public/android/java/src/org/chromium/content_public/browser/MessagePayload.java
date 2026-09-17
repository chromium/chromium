// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/**
 * Represents a JavaScript message payload. Currently only String, ArrayBuffer and SharedArrayBuffer
 * are supported.
 */
@NullMarked
public final class MessagePayload {
    @MessagePayloadType private final int mType;
    private final @Nullable String mString;
    private final byte @Nullable [] mArrayBuffer;
    private final @Nullable SharedArrayBuffer mSharedArrayBuffer;

    /**
     * Create a MessagePayload String type. To keep backward compatibility, string can be null, then
     * it's replaced to empty string in JNI.
     */
    public MessagePayload(@Nullable String string) {
        mType = MessagePayloadType.STRING;
        mString = string;
        mArrayBuffer = null;
        mSharedArrayBuffer = null;
    }

    /** Create a MessagePayload ArrayBuffer type. */
    public MessagePayload(byte[] arrayBuffer) {
        Objects.requireNonNull(arrayBuffer, "arrayBuffer cannot be null.");
        mType = MessagePayloadType.ARRAY_BUFFER;
        mArrayBuffer = arrayBuffer;
        mString = null;
        mSharedArrayBuffer = null;
    }

    /** Create a MessagePayload SharedArrayBuffer type. */
    public MessagePayload(SharedArrayBuffer sharedArrayBuffer) {
        Objects.requireNonNull(sharedArrayBuffer, "sharedArrayBuffer cannot be null.");
        mType = MessagePayloadType.SHARED_ARRAY_BUFFER;
        mSharedArrayBuffer = sharedArrayBuffer;
        mString = null;
        mArrayBuffer = null;
    }

    @MessagePayloadType
    public int getType() {
        return mType;
    }

    public @Nullable String getAsString() {
        checkType(MessagePayloadType.STRING);
        return mString;
    }

    public byte[] getAsArrayBuffer() {
        checkType(MessagePayloadType.ARRAY_BUFFER);
        Objects.requireNonNull(mArrayBuffer, "mArrayBuffer cannot be null.");
        return mArrayBuffer;
    }

    public SharedArrayBuffer getAsSharedArrayBuffer() {
        checkType(MessagePayloadType.SHARED_ARRAY_BUFFER);
        Objects.requireNonNull(mSharedArrayBuffer, "mSharedArrayBuffer cannot be null.");
        return mSharedArrayBuffer;
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

    public static String typeToString(@MessagePayloadType int type) {
        switch (type) {
            case MessagePayloadType.STRING:
                return "String";
            case MessagePayloadType.ARRAY_BUFFER:
                return "ArrayBuffer";
            case MessagePayloadType.SHARED_ARRAY_BUFFER:
                return "SharedArrayBuffer";
            case MessagePayloadType.INVALID:
                return "Invalid";
        }
        throw new IllegalArgumentException("Unknown type: " + type);
    }
}
