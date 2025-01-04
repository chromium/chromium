// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.build.annotations.NullMarked;

import java.io.Closeable;

/** A class which implements this interface can receive {@link Message} objects. */
@NullMarked
public interface MessageReceiver extends Closeable {

    /**
     * Receive a {@link Message}. The {@link MessageReceiver} is allowed to mutate the message.
     * Returns |true| if the message has been handled, |false| otherwise.
     */
    boolean accept(Message message);

    /**
     * @see java.io.Closeable#close()
     */
    @Override
    public void close();
}
