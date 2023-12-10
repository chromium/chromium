// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Core;

/** Base class for all mojo unions. */
public abstract class Union {
    /** They type of object that has been set. */
    protected int mTag;

    /** Returns the type of object being held by this union. */
    public int which() {
        return mTag;
    }

    /** Returns whether the type of object in this union is known. */
    public boolean isUnknown() {
        return mTag == -1;
    }

    /**
     * Returns the serialization of the union. This method can close Handles.
     *
     * @param core the |Core| implementation used to generate handles. Only used if the data
     *            structure being encoded contains interfaces, can be |null| otherwise.
     */
    public Message serialize(Core core) {
        Encoder encoder = new Encoder(core, BindingsHelper.UNION_SIZE);
        encoder.claimMemory(16);
        encode(encoder, 0);
        return encoder.getMessage();
    }

    /** Serializes this data structure using the given encoder. */
    protected abstract void encode(Encoder encoder, int offset);
}
