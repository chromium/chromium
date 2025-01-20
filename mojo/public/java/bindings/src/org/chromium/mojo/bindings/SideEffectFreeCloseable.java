// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;

import java.io.Closeable;

/** An implementation of closeable that doesn't do anything. */
@NullMarked
public class SideEffectFreeCloseable implements Closeable {

    /**
     * @see java.io.Closeable#close()
     */
    @Override
    public void close() {}
}
