// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;

/** Error when deserializing a mojo message. */
@NullMarked
public class DeserializationException extends RuntimeException {

    /** Constructs a new deserialization exception with the specified detail message. */
    public DeserializationException(String message) {
        super(message);
    }

    /** Constructs a new deserialization exception with the specified cause. */
    public DeserializationException(Exception cause) {
        super(cause);
    }
}
