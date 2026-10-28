// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;

/**
 * A struct without any fields. Used by generated bindings in place of the parameter structs of
 * interface methods that have no (response) parameters.
 */
@NullMarked
public final class EmptyStruct extends Struct {
    private static final int STRUCT_SIZE = 8;
    private static final int STRUCT_VERSION = 0;

    /** EmptyStruct has no state, so a single instance can be shared. */
    public static final EmptyStruct INSTANCE = new EmptyStruct();

    private EmptyStruct() {
        super(STRUCT_SIZE, STRUCT_VERSION);
    }

    /**
     * Validates that the given decoder is positioned at an empty struct.
     *
     * @throws DeserializationException on deserialization failure.
     */
    public static void validate(Decoder decoder) {
        decoder.readAndValidateStructHeader(STRUCT_SIZE);
    }

    @Override
    protected void encodeFields(Encoder encoder) {}
}
