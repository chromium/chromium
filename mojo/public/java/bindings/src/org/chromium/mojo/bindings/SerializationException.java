// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;

/** Error that can be thrown when serializing a mojo message. */
@NullMarked
public class SerializationException extends RuntimeException {

    /** Constructs a new serialization exception with the specified detail message. */
    public SerializationException(String message) {
        super(message);
    }

    /** Constructs a new serialization exception with the specified cause. */
    public SerializationException(Exception cause) {
        super(cause);
    }
}
