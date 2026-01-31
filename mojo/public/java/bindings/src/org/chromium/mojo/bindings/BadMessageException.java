// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;

/**
 * Indicates that a message is malformed. This can be due to a malformed message or some sort of
 * invariance that is broken. An example of a broken invariance would be receiving a response
 * message for a non-existent request.
 */
@NullMarked
public class BadMessageException extends Exception {
    protected BadMessageException(String message) {
        super(message);
    }

    protected BadMessageException(String message, Throwable cause) {
        super(message, cause);
    }
}
