// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public class InvalidHeaderException extends BadMessageException {
    public InvalidHeaderException(String message) {
        super(message);
    }
}
