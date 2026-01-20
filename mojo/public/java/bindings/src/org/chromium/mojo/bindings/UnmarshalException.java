// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;

/**
 * Used internally by mojo to convert an internal unmarshalling exception. This is a signal that a
 * deserialization error has occurred during message dispatch.
 */
@NullMarked
public class UnmarshalException extends BadMessageException {
    public UnmarshalException(DeserializationException e) {
        super("encountered deserialization error", e);
    }
}
